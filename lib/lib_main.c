#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <stdint.h>
#include <endian.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <signal.h>
#include <pthread.h>

#include "lib_main.h"
#include "lib_list.h"
#include "lib_dev.h"
#include "lib_mem.h"
#include "lib_tap.h"
#include "lib_vfio.h"

static int ufp_ifname_base(struct ufp_dev *dev, struct ufp_iface *iface);
static int ufp_alloc_ring(struct ufp_dev *dev, struct ufp_ring *ring,
	unsigned long size_desc, uint32_t num_desc, struct ufp_mpool *mpool);
static void ufp_release_rings(struct ufp_dev *dev, struct ufp_iface *iface);
static int ufp_alloc_ring(struct ufp_dev *dev, struct ufp_ring *ring,
	unsigned long size_desc, uint32_t num_desc, struct ufp_mpool *mpool);
static void ufp_release_ring(struct ufp_dev *dev, struct ufp_ring *ring,
	unsigned long size_desc);
static struct ufp_ops *ufp_ops_alloc(struct ufp_dev *dev);
static void ufp_ops_release(struct ufp_dev *dev, struct ufp_ops *ops);
static int ufp_up_iface(struct ufp_dev *dev, struct ufp_mpool **mpools,
	struct ufp_iface *iface);
static void ufp_down_iface(struct ufp_dev *dev, struct ufp_iface *iface);
static struct ufp_irq *ufp_irq_open(struct ufp_dev *dev,
	unsigned int entry_idx);
static void ufp_irq_close(struct ufp_irq *irq);
static int ufp_irq_setaffinity(struct ufp_irq *irq,
	unsigned int core_id);

inline uint32_t ufp_readl(const volatile void *addr)
{
	return htole32(*(volatile uint32_t *)addr);
}

inline void ufp_writel(uint32_t b, volatile void *addr)
{
	*(volatile uint32_t *)addr = htole32(b);
	return;
}

static int ufp_ifname_base(struct ufp_dev *dev, struct ufp_iface *iface)
{
	uint16_t domain;
	uint8_t bus, slot, func;
	int err;

	err = sscanf(dev->name,
		"%04" SCNx16 ":%02" SCNx8 ":%02" SCNx8 ".%01" SCNx8,
		&domain, &bus, &slot, &func);
	if(err != 4)
		goto err_parse;

	err = snprintf(iface->name, sizeof(iface->name), "un");
	if(err < 0)
		goto err_parse;

	if(domain){
		snprintf(iface->name + strlen(iface->name),
			sizeof(iface->name) - strlen(iface->name),
			"P%d", domain);
		if(err < 0)
			goto err_parse;
	}

	err = snprintf(iface->name + strlen(iface->name),
		sizeof(iface->name) - strlen(iface->name),
		"p%d", bus);
	if(err < 0)
		goto err_parse;

	err = snprintf(iface->name + strlen(iface->name),
		sizeof(iface->name) - strlen(iface->name),
		"s%d", slot);
	if(err < 0)
		goto err_parse;

	err = snprintf(iface->name + strlen(iface->name),
		sizeof(iface->name) - strlen(iface->name),
		"f%d", func);
	if(err < 0)
		goto err_parse;

	return 0;

err_parse:
	return -1;
}

struct ufp_plane *ufp_plane_alloc(struct ufp_dev **devs, int num_devs,
	struct ufp_buf *buf, unsigned int thread_id, unsigned int core_id)
{
	struct ufp_iface *iface;
	struct ufp_plane *plane;
	struct ufp_port *port;
	unsigned int num_ports = 0, port_idx = 0;
	int i, err;

	plane = malloc(sizeof(struct ufp_plane));
	if(!plane)
		goto err_plane_alloc;

	for(i = 0; i < num_devs; i++){
		num_ports += devs[i]->num_ifaces;
	}

	plane->num_ports = num_ports;
	plane->ports = malloc(sizeof(struct ufp_port) * num_ports);
	if(!plane->ports){
		printf("failed to allocate port for each plane\n");
		goto err_alloc_ports;
	}

	for(i = 0; i < num_devs; i++){
		list_for_each(&devs[i]->iface, iface, list){
			port = &plane->ports[port_idx];

			port->ops		= devs[i]->ops;
			port->bar		= devs[i]->bar;
			port->dev_idx		= i;

			port->rx_ring		= &(iface->rx_ring[thread_id]);
			port->tx_ring		= &(iface->tx_ring[thread_id]);
			port->rx_irq		= iface->rx_irq[thread_id];
			port->tx_irq		= iface->tx_irq[thread_id];
			port->num_rx_desc	= iface->num_rx_desc;
			port->num_tx_desc	= iface->num_tx_desc;
			port->num_qps		= iface->num_qps;
			port->rx_budget		= iface->rx_budget;
			port->tx_budget		= iface->tx_budget;
			port->mtu_frame		= iface->mtu_frame;
			memcpy(port->mac_addr, iface->mac_addr, ETH_ALEN);
			port->tap_fd		= iface->tap_fds[thread_id];
			port->tap_index		= iface->tap_index;

			port->rx_slot_next	= 0;
			port->rx_slot_offset	= port_idx * buf->count;
			port->tx_suspended	= 0;
			port->count_rx_alloc_failed	= 0;
			port->count_rx_clean_total	= 0;
			port->count_tx_xmit_failed	= 0;
			port->count_tx_clean_total	= 0;

			err = ufp_irq_setaffinity(port->rx_irq, core_id);
			if(err < 0){
				goto err_alloc_plane;
			}

			err = ufp_irq_setaffinity(port->tx_irq, core_id);
			if(err < 0){
				goto err_alloc_plane;
			}

			port_idx++;
		}
	}

	return plane;

err_alloc_plane:
	free(plane->ports);
err_alloc_ports:
	free(plane);
err_plane_alloc:
	return NULL;
}

void ufp_plane_release(struct ufp_plane *plane)
{
	free(plane->ports);
	free(plane);

	return;
}

struct ufp_mpool *ufp_mpool_init()
{
	struct ufp_mpool *mpool;
	unsigned long size;

	mpool = malloc(sizeof(struct ufp_mpool));
	if(!mpool)
		goto err_alloc_mpool;

	size = SIZE_1GB;
	mpool->addr_virt = mmap(NULL, size, PROT_READ | PROT_WRITE,
#ifdef MAP_HUGE_1GB
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB,
#else
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
#endif
		-1, 0);
	if(mpool->addr_virt == MAP_FAILED){
		goto err_mmap;
	}

	mpool->node = ufp_mem_init(mpool->addr_virt, size);
	if(!mpool->node)
		goto err_mem_init;

	list_init(&mpool->head);

	return mpool;

err_mem_init:
	munmap(mpool->addr_virt, SIZE_1GB);
err_mmap:
	free(mpool);
err_alloc_mpool:
	return NULL;
}

void ufp_mpool_flush(struct ufp_mpool *mpool)
{
	struct ufp_mnode *node, *temp;
	list_for_each_safe(&mpool->head, node, list, temp){
		list_del(&node->list);
		_ufp_mem_free(node);
	}
}

void ufp_mpool_destroy(struct ufp_mpool *mpool)
{
	ufp_mpool_flush(mpool);
	ufp_mem_destroy(mpool->node);
	munmap(mpool->addr_virt, SIZE_1GB);
	free(mpool);

	return;
}

static int ufp_alloc_rings(struct ufp_dev *dev, struct ufp_iface *iface,
	struct ufp_mpool **mpools)
{
	int i, err;
	unsigned int qps_assigned = 0;

	iface->tx_ring = malloc(sizeof(struct ufp_ring) * iface->num_qps);
	if(!iface->tx_ring)
		goto err_alloc_tx_ring;

	iface->rx_ring = malloc(sizeof(struct ufp_ring) * iface->num_qps);
	if(!iface->rx_ring)
		goto err_alloc_rx_ring;

	for(i = 0; i < iface->num_qps; i++, qps_assigned++){
		err = ufp_alloc_ring(dev, &iface->rx_ring[i],
			iface->size_rx_desc, iface->num_rx_desc, mpools[i]);
		if(err < 0)
			goto err_rx_alloc;

		err = ufp_alloc_ring(dev, &iface->tx_ring[i],
			iface->size_tx_desc, iface->num_tx_desc, mpools[i]);
		if(err < 0)
			goto err_tx_alloc;

		continue;

err_tx_alloc:
		ufp_release_ring(dev, &iface->rx_ring[i],
			iface->size_rx_desc);
err_rx_alloc:
		goto err_desc_assign;
	}

	return 0;

err_desc_assign:
	for(i = 0; i < qps_assigned; i++){
		ufp_release_ring(dev, &iface->tx_ring[i],
			iface->size_tx_desc);
		ufp_release_ring(dev, &iface->rx_ring[i],
			iface->size_rx_desc);
	}
err_alloc_rx_ring:
	free(iface->tx_ring);
err_alloc_tx_ring:
	return -1;
}

static void ufp_release_rings(struct ufp_dev *dev, struct ufp_iface *iface)
{
	int i;

	for(i = 0; i < iface->num_qps; i++){
		ufp_release_ring(dev, &iface->tx_ring[i],
			iface->size_tx_desc);
		ufp_release_ring(dev, &iface->rx_ring[i],
			iface->size_rx_desc);
	}

	free(iface->rx_ring);
	free(iface->tx_ring);

	return;
}

static int ufp_alloc_ring(struct ufp_dev *dev, struct ufp_ring *ring,
	unsigned long size_desc, uint32_t num_desc, struct ufp_mpool *mpool)
{
	unsigned long addr_dma;
	void *addr_virt;
	int err;
	int *slot_index;
	size_t size_desc_align;

	size_desc_align = ALIGN(size_desc, getpagesize());

	addr_virt = ufp_mem_alloc_align(mpool, size_desc_align,
		getpagesize());
	if(!addr_virt)
		goto err_alloc;

	err = ufp_vfio_dma_map(addr_virt, &addr_dma, size_desc_align);
	if(err < 0)
		goto err_dma_map;

	ring->addr_dma = addr_dma;
	ring->addr_virt = addr_virt;

	slot_index = malloc(sizeof(int) * num_desc);
	if(!slot_index){
		goto err_assign;
	}

	ring->next_to_use = 0;
	ring->next_to_clean = 0;
	ring->slot_index = slot_index;
	return 0;

err_assign:
	ufp_vfio_dma_unmap(ring->addr_dma, size_desc_align);
err_dma_map:
	ufp_mem_free(addr_virt);
err_alloc:
	return -1;
}

static void ufp_release_ring(struct ufp_dev *dev, struct ufp_ring *ring,
	unsigned long size_desc)
{
	size_t size_desc_align;

	size_desc_align = ALIGN(size_desc, getpagesize());
	free(ring->slot_index);
	ufp_vfio_dma_unmap(ring->addr_dma, size_desc_align);
	ufp_mem_free(ring->addr_virt);
	return;
}

struct ufp_buf *ufp_alloc_buf(struct ufp_dev **devs, int num_devs,
	uint32_t slot_size, uint32_t buf_count, struct ufp_mpool *mpool)
{
	struct ufp_buf *buf;
	int err, i, num_bufs;
	size_t size_buf_align;

	buf = malloc(sizeof(struct ufp_buf));
	if(!buf)
		goto err_alloc_buf;

	/*
	 * XXX: Should we add buffer padding for memory interleaving?
	 * DPDK does so in rte_mempool.c/optimize_object_size().
	 */
	buf->slot_size = slot_size;
	buf->count = buf_count;
	for(i = 0, num_bufs = 0; i < num_devs; i++){
		num_bufs += devs[i]->num_ifaces * buf->count;
	}
	buf->size = buf->slot_size * num_bufs;
	size_buf_align = ALIGN(buf->size, getpagesize());

	buf->addr_virt = ufp_mem_alloc_align(mpool, size_buf_align,
		getpagesize());
	if(!buf->addr_virt)
		goto err_mem_alloc;

	err = ufp_vfio_dma_map(buf->addr_virt, &buf->addr_dma,
		size_buf_align);
	if(err < 0)
		goto err_dma_map;

	buf->slots = malloc(sizeof(int) * num_bufs);
	if(!buf->slots)
		goto err_alloc_slots;

	for(i = 0; i < num_bufs; i++){
		buf->slots[i] = 0;
	}

	return buf;

err_alloc_slots:
	ufp_vfio_dma_unmap(buf->addr_dma, size_buf_align);
err_dma_map:
	ufp_mem_free(buf->addr_virt);
err_mem_alloc:
	free(buf);
err_alloc_buf:
	return NULL;
}

void ufp_release_buf(struct ufp_buf *buf)
{
	int err;
	size_t size_buf_align;

	free(buf->slots);
	size_buf_align = ALIGN(buf->size, getpagesize());

	err = ufp_vfio_dma_unmap(buf->addr_dma, size_buf_align);
	if(err < 0)
		perror("failed to unmap buf");

	ufp_mem_free(buf->addr_virt);
	free(buf);
	return;
}

static struct ufp_ops *ufp_ops_alloc(struct ufp_dev *dev)
{
	struct ufp_ops *ops;
	int err;

	ops = malloc(sizeof(struct ufp_ops));
	if(!ops)
		goto err_alloc_ops;

	memset(ops, 0, sizeof(struct ufp_ops));
	err = ufp_dev_init(dev, ops);
	if(err)
		goto err_probe_device;

	return ops;

err_probe_device:
	free(ops);
err_alloc_ops:
	return NULL;
}

static void ufp_ops_release(struct ufp_dev *dev, struct ufp_ops *ops)
{
	ufp_dev_destroy(dev, ops);
	free(ops);
	return;
}

struct ufp_dev *ufp_open(const char *name)
{
	struct ufp_dev *dev;
	struct ufp_iface *iface;
	int group_fd, err;

	dev = malloc(sizeof(struct ufp_dev));
	if (!dev)
		goto err_alloc_dev;
	strncpy(dev->name, name, sizeof(dev->name));

	group_fd = ufp_vfio_group_open(dev);
	if(group_fd < 0)
		goto err_group_open;

	err = ufp_vfio_device_open(dev, group_fd);
	if(err < 0)
		goto err_device_open;

	dev->ops = ufp_ops_alloc(dev);
	if(!dev->ops)
		goto err_ops_alloc;

	/* dev->ops->open setup only first iface */
	list_init(&dev->iface);
	iface = malloc(sizeof(struct ufp_iface));
	if(!iface)
		goto err_alloc_iface;
	list_add_last(&dev->iface, &iface->list);
	dev->num_ifaces = 1;

	err = ufp_ifname_base(dev, iface);
	if(err < 0)
		goto err_ifname;

	err = ufp_tun_open(iface);
	if(err < 0)
		goto err_tap_open;

	err = dev->ops->open(dev);
	if(err < 0)
		goto err_ops_open;

	close(group_fd);
	return dev;

err_ops_open:
	ufp_tun_close(iface);
err_tap_open:
err_ifname:
	list_del(&iface->list);
	free(iface);
err_alloc_iface:
	ufp_ops_release(dev, dev->ops);
err_ops_alloc:
	close(dev->fd);
err_device_open:
	close(group_fd);
err_group_open:
	free(dev);
err_alloc_dev:
	return NULL;
}

void ufp_close(struct ufp_dev *dev)
{
	struct ufp_iface *iface, *temp;

	dev->ops->close(dev);

	list_for_each_safe(&dev->iface, iface, list, temp){
		ufp_tun_close(iface);
		list_del(&iface->list);
		free(iface);
	}

	ufp_ops_release(dev, dev->ops);
	munmap(dev->bar, dev->bar_size);
	close(dev->fd);
	free(dev);
	return;
}

static int ufp_up_iface(struct ufp_dev *dev, struct ufp_mpool **mpools,
	struct ufp_iface *iface)
{
	unsigned int	irq_idx = 0,
			rx_irq_done = 0,
			tx_irq_done = 0;
	int i, err;

	err = ufp_alloc_rings(dev, iface, mpools);
	if(err < 0)
		goto err_alloc_rings;

	err = ufp_tun_up(iface);
	if(err < 0)
		goto err_tun_up;

	iface->rx_irq = malloc(sizeof(struct ufp_irq *) * iface->num_qps);
	if(!iface->rx_irq)
		goto err_alloc_rx_irq;

	iface->tx_irq = malloc(sizeof(struct ufp_irq *) * iface->num_qps);
	if(!iface->tx_irq)
		goto err_alloc_tx_irq;

	/* reserve irq_idx = 0 for misc_irq */
	irq_idx++;

	for(i = 0; i < iface->num_qps; i++, rx_irq_done++){
		iface->rx_irq[i] = ufp_irq_open(dev, irq_idx++);
		if(!iface->rx_irq[i])
			goto err_rx_irq;
	}

	for(i = 0; i < iface->num_qps; i++, tx_irq_done++){
		iface->tx_irq[i] = ufp_irq_open(dev, irq_idx++);
		if(!iface->tx_irq[i])
			goto err_tx_irq;
	}

	return 0;

err_tx_irq:
	for(i = 0; i < tx_irq_done; i++){
		ufp_irq_close(iface->tx_irq[i]);
	}
err_rx_irq:
	for(i = 0; i < rx_irq_done; i++){
		ufp_irq_close(iface->rx_irq[i]);
	}
err_alloc_tx_irq:
	free(iface->rx_irq);
err_alloc_rx_irq:
	ufp_tun_down(iface);
err_tun_up:
	ufp_release_rings(dev, iface);
err_alloc_rings:
	return -1;
}

static void ufp_down_iface(struct ufp_dev *dev, struct ufp_iface *iface)
{
	int i;

	for(i = 0; i < iface->num_qps; i++){
		ufp_irq_close(iface->rx_irq[i]);
	}
	for(i = 0; i < iface->num_qps; i++){
		ufp_irq_close(iface->tx_irq[i]);
	}

	free(iface->tx_irq);
	free(iface->rx_irq);

	ufp_tun_down(iface);
	ufp_release_rings(dev, iface);

	return;
}

int ufp_up(struct ufp_dev *dev, struct ufp_mpool **mpools,
	unsigned int num_qps, unsigned int buf_size,
	unsigned int mtu_frame, unsigned int promisc,
	unsigned int rx_budget, unsigned int tx_budget)
{
	struct ufp_iface *iface;
	unsigned int	irq_idx = 0,
			misc_done = 0,
			iface_done = 0;
	int err, i;

	dev->misc_irq = malloc(sizeof(struct ufp_irq *) *
		dev->num_misc_irqs);
	if(!dev->misc_irq)
		goto err_alloc_misc_irq;

	for(i = 0; i < dev->num_misc_irqs; i++, misc_done++){
		dev->misc_irq[i] = ufp_irq_open(dev, irq_idx++);
		if(!dev->misc_irq[i])
			goto err_misc_irq;
	}

	list_for_each(&dev->iface, iface, list){
		iface->buf_size = buf_size;
		iface->mtu_frame = mtu_frame;
		iface->promisc = promisc;
		iface->rx_budget = rx_budget;
		iface->tx_budget = tx_budget;
		iface->num_qps = num_qps;

		err = ufp_up_iface(dev, mpools, iface);
		if(err < 0)
			goto err_iface_up;

		irq_idx += iface->num_qps * 2;
		iface_done++;
	}

	err = ufp_vfio_irq_set(dev, irq_idx);
	if(err < 0)
		goto err_irq_set;

	err = ufp_vfio_irq_vector_get(dev);
	if(err < 0)
		goto err_irq_vector_get;

	err = dev->ops->up(dev);
	if(err < 0)
		goto err_ops_up;

	return 0;

err_ops_up:
err_irq_vector_get:
	ufp_vfio_irq_unset(dev);
err_irq_set:
err_iface_up:
	i = 0;
	list_for_each(&dev->iface, iface, list){
		if(!(i++ < iface_done))
			break;
		ufp_down_iface(dev, iface);
	}
err_misc_irq:
	for(i = 0; i < misc_done; i++){
		ufp_irq_close(dev->misc_irq[i]);
	}
	free(dev->misc_irq);
err_alloc_misc_irq:
	return -1;
}

void ufp_down(struct ufp_dev *dev)
{
	struct ufp_iface *iface;
	int i, err;

	dev->ops->down(dev);

	err = ufp_vfio_irq_unset(dev);
	if(err < 0)
		goto err_irq_unset;

err_irq_unset:
	list_for_each(&dev->iface, iface, list){
		ufp_down_iface(dev, iface);
	}

	for(i = 0; i < dev->num_misc_irqs; i++){
		ufp_irq_close(dev->misc_irq[i]);
	}
	free(dev->misc_irq);
	return;
}

static struct ufp_irq *ufp_irq_open(struct ufp_dev *dev,
	unsigned int entry_idx)
{
	struct ufp_irq *irq;
	int efd;

	efd = eventfd(0, 0);
	if(efd < 0)
		goto err_open_efd;

	irq = malloc(sizeof(struct ufp_irq));
	if(!irq)
		goto err_alloc_handle;

	irq->fd		= efd;
	irq->vector	= 0;
	irq->entry_idx	= entry_idx;

	return irq;

err_alloc_handle:
	close(efd);
err_open_efd:
	return NULL;
}

static void ufp_irq_close(struct ufp_irq *irq)
{
	close(irq->fd);
	free(irq);

	return;
}

static int ufp_irq_setaffinity(struct ufp_irq *irq,
	unsigned int core_id)
{
	FILE *file;
	char filename[FILENAME_SIZE];
	uint32_t mask_low, mask_high;
	int ret;

	mask_low = core_id <= 31 ? 1 << core_id : 0;
	mask_high = core_id <= 31 ? 0 : 1 << (core_id - 31);

	snprintf(filename, sizeof(filename),
		"/proc/irq/%d/smp_affinity", irq->vector);
	file = fopen(filename, "w");
	if(!file){
		printf("failed to open smp_affinity\n");
		goto err_open_proc;
	}

	ret = fprintf(file, "%08x,%08x", mask_high, mask_low);
	if(ret < 0){
		printf("failed to set affinity\n");
		goto err_set_affinity;
	}

	fclose(file);
	return 0;

err_set_affinity:
	fclose(file);
err_open_proc:
	return -1;
}

__attribute__((constructor))
static void ufp_initialize()
{
	int err;

	err = ufp_vfio_container_open();
	if(err < 0)
		goto err_container_open;

	/* TBD: DRIVER_PATH should be configurable */
	err = ufp_dev_load_lib(DRIVER_PATH);
	if(err)
		goto err_load_lib;

	return;

err_load_lib:
	ufp_vfio_container_close();
err_container_open:
	exit(-1);
}

__attribute__((destructor))
static void ufp_finalize()
{
	ufp_dev_unload_lib();
	ufp_vfio_container_close();
	return;
}
