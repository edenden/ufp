#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

static int ufp_dma_map(struct ufp_dev *dev, void *addr_virt,
	unsigned long *addr_dma, unsigned long size);
static int ufp_dma_unmap(struct ufp_dev *dev, unsigned long addr_dma);
static struct ufp_ops *ufp_ops_alloc(struct ufp_dev *dev);
static void ufp_ops_release(struct ufp_dev *dev, struct ufp_ops *ops);
static struct ufp_irq_handle *ufp_irq_open(struct ufp_dev *dev,
	unsigned int entry_idx);
static void ufp_irq_close(struct ufp_irq_handle *irqh);
static int ufp_irq_setaffinity(unsigned int vector, unsigned int core_id);

inline uint32_t ufp_readl(const volatile void *addr)
{
	return htole32(*(volatile uint32_t *)addr);
}

inline void ufp_writel(uint32_t b, volatile void *addr)
{
	*(volatile uint32_t *)addr = htole32(b);
	return;
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
	void *addr_virt;
	unsigned long size;

	mpool = malloc(sizeof(struct ufp_mpool));
	if(!mpool)
		goto err_alloc_mpool;

	size = SIZE_1GB;
	mpool->addr_virt = mmap(NULL, size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
	if(mpool->addr_virt == MAP_FAILED){
		goto err_mmap;
	}

	mpool->node = ufp_mem_init(addr_virt, size);
	if(!mpool->node)
		goto err_mem_init;

	return mpool;

err_mem_init:
	munmap(mpool->addr_virt, SIZE_1GB);
err_mmap:
	free(mpool);
err_alloc_mpool:
	return NULL;
}

void ufp_mpool_destroy(struct ufp_mpool *mpool)
{
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
	if(!iface->rx_ring)
		goto err_alloc_tx_ring;

	iface->rx_ring = malloc(sizeof(struct ufp_ring) * iface->num_qps);
	if(!iface->tx_ring)
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
		ufp_release_ring(dev, &iface->rx_ring[i]);
err_rx_alloc:
		goto err_desc_assign;
	}

	return 0;

err_desc_assign:
	for(i = 0; i < qps_assigned; i++){
		ufp_release_ring(dev, &iface->tx_ring[i]);
		ufp_release_ring(dev, &iface->rx_ring[i]);
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
		free(iface->tx_ring[i].slot_index);
		ufp_dma_unmap(dev, iface->tx_ring[i].addr_dma);
		ufp_mem_free(iface->tx_ring[i].addr_virt);
		free(iface->rx_ring[i].slot_index);
		ufp_dma_unmap(dev, iface->rx_ring[i].addr_dma);
		ufp_mem_free(iface->rx_ring[i].addr_virt);
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

	addr_virt = ufp_mem_alloc(mpool, size_desc);
	if(!addr_virt)
		goto err_alloc;

	err = ufp_dma_map(dev, addr_virt, &addr_dma, size_desc);
	if(err < 0){
		goto err_dma_map;
	}

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
	ufp_dma_unmap(dev, ring->addr_dma);
err_dma_map:
	ufp_mem_free(addr_virt);
err_alloc:
	return -1;
}


static void ufp_release_ring(struct ufp_dev *dev, struct ufp_ring *ring)
{
	free(ring->slot_index);
	ufp_dma_unmap(dev, iface->tx_ring[i].addr_dma);
	ufp_mem_free(ring->addr_virt);

	return;
}

struct ufp_buf *ufp_alloc_buf(struct ufp_dev **devs, int num_devs,
	uint32_t buf_size, uint32_t buf_count, struct ufp_mpool *mpool)
{
	struct ufp_buf *buf;
	void *addr_virt;
	unsigned long addr_dma, size;
	int *slots;
	int err, i, num_bufs, mapped_devs = 0;

	buf = malloc(sizeof(struct ufp_buf));
	if(!buf)
		goto err_alloc_buf;

	buf->addr_dma = malloc(sizeof(unsigned long) * num_devs);
	if(!buf->addr_dma)
		goto err_alloc_buf_addr_dma;

	/*
	 * XXX: Should we add buffer padding for memory interleaving?
	 * DPDK does so in rte_mempool.c/optimize_object_size().
	 */
	buf->buf_size = buf_size;
	buf->count = buf_count;
	for(i = 0, num_bufs = 0; i < num_devs; i++){
		num_bufs += devs[i].num_ifaces * buf->count;
	}
	size = buf->buf_size * num_bufs;
	buf->addr_virt = ufp_mem_alloc(mpool, size);
	if(!buf->addr_virt)
		goto err_mem_alloc;

	for(i = 0; i < num_devs; i++, mapped_devs++){
		err = ufp_dma_map(devs[i], buf->addr_virt, &addr_dma, size);
		if(err < 0)
			goto err_ufp_dma_map;

		buf->addr_dma[i] = addr_dma;
	}

	buf->slots = malloc(sizeof(int) * num_bufs);
	if(!buf->slots)
		goto err_alloc_slots;

	for(i = 0; i < num_bufs; i++){
		buf->slots[i] = 0;
	}

	return buf;

err_alloc_slots:
err_ufp_dma_map:
	for(i = 0; i < mapped_devs; i++){
		ufp_dma_unmap(devs[i], buf->addr_dma[i]);
	}
	ufp_mem_free(buf->addr_virt);
err_mem_alloc:
	free(buf->addr_dma);
err_alloc_buf_addr_dma:
	free(buf);
err_alloc_buf:
	return NULL;
}

void ufp_release_buf(struct ufp_dev **devs, int num_devs,
	struct ufp_buf *buf)
{
	int i, err;

	free(buf->slots);

	for(i = 0; i < num_devs; i++){
		err = ufp_dma_unmap(devs[i], buf->addr_dma[i]);
		if(err < 0)
			perror("failed to unmap buf");
	}

	ufp_mem_free(buf->addr_virt);
	free(buf->addr_dma);
	free(buf);

	return;
}

static int ufp_dma_map(struct ufp_dev *dev, void *addr_virt,
	unsigned long *addr_dma, unsigned long size)
{
	struct ufp_map_req req_map;

	req_map.addr_virt = (unsigned long)addr_virt;
	req_map.addr_dma = 0;
	req_map.size = size;
	req_map.cache = IXGBE_DMA_CACHE_DISABLE;

	if(ioctl(dev->fd, UFP_MAP, (unsigned long)&req_map) < 0)
		return -1;

	*addr_dma = req_map.addr_dma;
	return 0;
}

static int ufp_dma_unmap(struct ufp_dev *dev, unsigned long addr_dma)
{
	struct ufp_unmap_req req_unmap;

	req_unmap.addr_dma = addr_dma;

	if(ioctl(dev->fd, UFP_UNMAP, (unsigned long)&req_unmap) < 0)
		return -1;

	return 0;
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
	char filename[FILENAME_SIZE];
	struct ufp_info_req req;
	struct ufp_iface *iface;
	int err;

	dev = malloc(sizeof(struct ufp_dev));
	if (!dev)
		goto err_alloc_dev;

	strncpy(dev->name, name, sizeof(dev->name));
	snprintf(filename, sizeof(filename), "/dev/ufp/%s", dev->name);
	dev->fd = open(filename, O_RDWR);
	if (dev->fd < 0)
		goto err_open;

	/* Get device information */
	memset(&req, 0, sizeof(struct ufp_info_req));
	err = ioctl(dev->fd, UFP_INFO, (unsigned long)&req);
	if(err < 0)
		goto err_ioctl_info;

	dev->bar_size = req.mmio_size;

	/* Map PCI config register space */
	dev->bar = mmap(NULL, dev->bar_size,
		PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, 0);
	if(ih->bar == MAP_FAILED)
		goto err_mmap;

	dev->device_id = req.device_id;
	dev->vendor_id = req.vendor_id;
	dev->ops = ufp_ops_alloc(dev);
	if(!dev->ops)
		goto err_ops_alloc;

	/* dev->ops setup only first iface */
	list_init(&dev->iface);
	iface = malloc(sizeof(struct ufp_iface));
	if(!iface)
		goto err_alloc_iface;
	list_add_last(&dev->iface, &iface->list);

	err = dev->ops->open(dev);
	if(err < 0)
		goto err_ops_open;

	err = ufp_alloc_rings(dev, dev->iface, mpools);
	if(err < 0)
		goto err_alloc_rings;

	return dev;

err_alloc_rings:
	dev->ops->close(dev);
err_ops_open:
	free(dev->iface);
err_alloc_iface:
	ufp_ops_release(dev, dev->ops);
err_ops_alloc:
	munmap(dev->bar, dev->bar_size);
err_mmap:
err_ioctl_info:
	close(dev->fd);
err_open:
	free(dev->iface);
err_alloc_iface:
	free(dev);
err_alloc_dev:
	return NULL;
}

void ufp_close(struct ufp_dev *dev)
{
	struct ufp_iface *iface, *temp;

	list_for_each_safe(&dev->iface, iface, list, temp){
		list_del(&iface->list);
		ufp_release_rings(dev, iface);
		free(iface);
	}

	dev->ops->close(dev);
	ufp_ops_release(dev, dev->ops);
	munmap(dev->bar, dev->bar_size);
	close(dev->fd);
	free(dev);

	return;
}

int ufp_up(struct ufp_dev *dev, unsigned int num_qps,
	unsigned int mtu_frame, unsigned int promisc,
	unsigned int rx_budget, unsigned int tx_budget)
{
	struct ufp_iface *iface;
	struct ufp_start_req req;
	int err;

	memset(&req, 0, sizeof(struct ufp_start_req));
	list_for_each(&dev->iface, iface, list){
		iface->mtu_frame = mtu_frame;
		iface->promisc = promisc;
		iface->rx_budget = rx_budget;
		iface->tx_budget = tx_budget;
		iface->num_qps = num_qps;
		req.num_irqs += (iface->num_qps * 2);
	}
	req.num_irqs += dev->num_misc_irqs
	if(ioctl(dev->fd, UFP_START, (unsigned long)&req) < 0)
		goto err_ioctl_start;

	err = dev->ops->up(dev);
	if(err < 0)
		goto err_ops_up;

	return 0;

err_ops_up:
	ioctl(dev->fd, UFP_STOP, 0);
err_ioctl_start:
	return -1;
}

void ufp_down(struct ufp_dev *dev)
{
	int err;

	err = dev->ops->down(dev);
	if(err < 0)
		goto err_ops_down;

	if(ioctl(dev->fd, UFP_STOP, 0) < 0)
		goto err_ioctl_stop;

err_ioctl_stop:
err_stop_adapter:
	return;
}

static struct ufp_irq_handle *ufp_irq_open(struct ufp_dev *dev,
	unsigned int entry_idx)
{
	struct ufp_irq_handle *irqh;
	int efd, err;
	struct ufp_irqbind_req req;

	efd = eventfd(0, 0);
	if(efd < 0)
		goto err_open_efd;

	req.event_fd	= efd;
	req.entry_idx	= entry_idx;

	err = ioctl(dev->fd, UFP_IRQBIND, (unsigned long)&req);
	if(err < 0){
		printf("failed to UFP_IRQ\n");
		goto err_irq_bind;
	}

	irqh = malloc(sizeof(struct ufp_irq_handle));
	if(!irqh)
		goto err_alloc_handle;

	irqh->fd		= efd;
	irqh->vector		= req.vector;
	irqh->entry_idx		= entry_idx;
	return irqh;

err_alloc_handle:
err_irq_bind:
	close(efd);
err_open_efd:
	return NULL;
}

static void ufp_irq_close(struct ufp_irq_handle *irqh)
{
	close(irqh->fd);
	free(irqh);

	return;
}

static int ufp_irq_setaffinity(struct ufp_irq_handle *irqh, unsigned int core_id)
{
	FILE *file;
	char filename[FILENAME_SIZE];
	uint32_t mask_low, mask_high;
	int ret;

	mask_low = core_id <= 31 ? 1 << core_id : 0;
	mask_high = core_id <= 31 ? 0 : 1 << (core_id - 31);

	snprintf(filename, sizeof(filename),
		"/proc/irq/%d/smp_affinity", irqh->vector);
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

	err = ufp_dev_load_lib(DRIVER_PATH);
	if(err)
		goto err_load_lib;

	return;

err_load_lib:
	exit(-1);
}

__attribute__((destructor))
static void ufp_finalize()
{
	ufp_dev_unload_lib();
}
