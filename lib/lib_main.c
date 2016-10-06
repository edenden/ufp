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
#include "lib_mem.h"
#include "lib_ixgbevf.h"

static int ufp_dma_map(struct ufp_handle *ih, void *addr_virt,
	unsigned long *addr_dma, unsigned long size);
static int ufp_dma_unmap(struct ufp_handle *ih, unsigned long addr_dma);
static struct ufp_ops *ufp_ops_alloc(uint16_t device_id);
static void ufp_ops_release(struct ufp_ops *ops);
static struct ufp_irq_handle *ufp_irq_open(struct ufp_handle *ih,
	enum ufp_irq_type type, unsigned int irq_idx, unsigned int core_id)
static void ufp_irq_close(struct ufp_irq_handle *irqh);
static int ufp_irq_setaffinity(unsigned int vector, unsigned int core_id);

inline uint32_t ufp_readl(const volatile void *addr)
{
	return htole32( *(volatile uint32_t *) addr );
}

inline void ufp_writel(uint32_t b, volatile void *addr)
{
	*(volatile uint32_t *) addr = htole32(b);
	return;
}

inline uint32_t ufp_read_reg(struct ufp_handle *ih, uint32_t reg)
{
	uint32_t value = ufp_readl(ih->bar + reg);
	return value;
}

inline void ufp_write_reg(struct ufp_handle *ih, uint32_t reg, uint32_t value)
{
	ufp_writel(value, ih->bar + reg);
	return;
}

inline void ufp_write_flush(struct ufp_handle *ih)
{
	ufp_read_reg(ih, 0x00008);
	return;
}

struct ufp_plane *ufp_plane_alloc(struct ufp_handle **ih_list,
	struct ufp_buf *buf, int ih_num, unsigned int thread_id,
	unsigned int core_id)
{
	struct ufp_plane *plane;
	int i, ports_assigned = 0;

	plane = malloc(sizeof(struct ufp_plane));
	if(!plane)
		goto err_plane_alloc;

	plane->ports = malloc(sizeof(struct ufp_port) * ih_num);
	if(!plane->ports){
		printf("failed to allocate port for each plane\n");
		goto err_alloc_ports;
	}

	for(i = 0; i < ih_num; i++, ports_assigned++){
		plane->ports[i].bar = ih_list[i]->bar;
		plane->ports[i].rx_ring = &(ih_list[i]->rx_ring[thread_id]);
		plane->ports[i].tx_ring = &(ih_list[i]->tx_ring[thread_id]);
		plane->ports[i].ops = ih_list[i]->ops;
		plane->ports[i].rx_slot_next = 0;
		plane->ports[i].rx_slot_offset = i * buf->count;
		plane->ports[i].tx_suspended = 0;
		plane->ports[i].num_rx_desc = ih_list[i]->num_rx_desc;
		plane->ports[i].num_tx_desc = ih_list[i]->num_tx_desc;
		plane->ports[i].num_qps = ih_list[i]->num_qps;
		plane->ports[i].rx_budget = ih_list[i]->rx_budget;
		plane->ports[i].tx_budget = ih_list[i]->tx_budget;
		plane->ports[i].mtu_frame = ih_list[i]->mtu_frame;
		plane->ports[i].count_rx_alloc_failed = 0;
		plane->ports[i].count_rx_clean_total = 0;
		plane->ports[i].count_tx_xmit_failed = 0;
		plane->ports[i].count_tx_clean_total = 0;

		memcpy(plane->ports[i].mac_addr, ih_list[i]->mac_addr, ETH_ALEN);

		plane->ports[i].rx_irq = ufp_irq_open(ih_list[i],
						UFP_IRQ_RX, thread_id, core_id);
		if(!plane->ports[i].rx_irq)
			goto err_alloc_irq_rx;

		plane->ports[i].tx_irq = ufp_irq_open(ih_list[i],
						UFP_IRQ_TX, thread_id, core_id);
		if(!plane->ports[i].tx_irq)
			goto err_alloc_irq_tx;

		continue;

err_alloc_irq_tx:
		ufp_irq_close(plane->ports[i].rx_irq);
err_alloc_irq_rx:
		goto err_alloc_plane;
	}

	return plane;

err_alloc_plane:
	for(i = 0; i < ports_assigned; i++){
		ufp_irq_close(plane->ports[i].rx_irq);
		ufp_irq_close(plane->ports[i].tx_irq);
	}
	free(plane->ports);
err_alloc_ports:
	free(plane);
err_plane_alloc:
	return NULL;
}

void ufp_plane_release(struct ufp_plane *plane, int ih_num)
{
	int i;

	for(i = 0; i < ih_num; i++){
		ufp_irq_close(plane->ports[i].rx_irq);
		ufp_irq_close(plane->ports[i].tx_irq);
	}

	free(plane->ports);
	free(plane);

	return;
}

struct ufp_desc *ufp_desc_alloc(struct ufp_handle **ih_list, int ih_num,
	int thread_id)
{
	struct ufp_desc *desc;
	unsigned long size, size_tx_desc, size_rx_desc, size_mem;
	void *addr_virt, *addr_mem;
	int i, ret;
	int desc_assigned = 0;

	desc = malloc(sizeof(struct ufp_desc));
	if(!desc)
		goto err_alloc_desc;

	size = SIZE_1GB;
	addr_virt = mmap(NULL, size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
	if(addr_virt == MAP_FAILED){
		goto err_mmap;
	}

	desc->addr_virt = addr_virt;

	for(i = 0; i < ih_num; i++, desc_assigned++){
		int *slot_index;
		struct ufp_handle *ih;
		unsigned long addr_dma;

		ih = ih_list[i];

		/* PCI-E can't cross 4K boundary */
		size_rx_desc = ALIGN(ih->size_rx_desc, 4096);
		size_tx_desc = ALIGN(ih->size_tx_desc, 4096);

		/* Rx descripter ring allocation */
		ret = ufp_dma_map(ih, addr_virt, &addr_dma, size_rx_desc);
		if(ret < 0){
			goto err_rx_dma_map;
		}

		ih->rx_ring[thread_id].addr_dma = addr_dma;
		ih->rx_ring[thread_id].addr_virt = addr_virt;

		slot_index = malloc(sizeof(int) * ih->num_rx_desc);
		if(!slot_index){
			goto err_rx_assign;
		}

		ih->rx_ring[thread_id].next_to_use = 0;
		ih->rx_ring[thread_id].next_to_clean = 0;
		ih->rx_ring[thread_id].slot_index = slot_index;

		addr_virt += size_rx_desc;

		/* Tx descripter ring allocation */
		ret = ufp_dma_map(ih, addr_virt, &addr_dma, size_tx_desc);
		if(ret < 0){
			goto err_tx_dma_map;
		}

		ih->tx_ring[thread_id].addr_dma = addr_dma;
		ih->tx_ring[thread_id].addr_virt = addr_virt;

		slot_index = malloc(sizeof(int) * ih->num_tx_desc);
		if(!slot_index){
			goto err_tx_assign;
		}

		ih->tx_ring[thread_id].next_to_use = 0;
		ih->tx_ring[thread_id].next_to_clean = 0;
		ih->tx_ring[thread_id].slot_index = slot_index;

		addr_virt += size_rx_desc;

		continue;

err_tx_assign:
		ufp_dma_unmap(ih, ih->tx_ring[thread_id].addr_dma);
err_tx_dma_map:
		free(ih->rx_ring[thread_id].slot_index);
err_rx_assign:
		ufp_dma_unmap(ih, ih->rx_ring[thread_id].addr_dma);
err_rx_dma_map:
		goto err_desc_assign;
	}

	addr_mem	= (void *)ALIGN((unsigned long)addr_virt, L1_CACHE_BYTES);
	size_mem	= size - (addr_mem - desc->addr_virt);
	desc->node	= ufp_mem_init(addr_mem, size_mem);
	if(!desc->node)
		goto err_mem_init;

	return desc;

err_mem_init:
err_desc_assign:
	for(i = 0; i < desc_assigned; i++){
		struct ufp_handle *ih;

		ih = ih_list[i];
		free(ih->tx_ring[thread_id].slot_index);
		ufp_dma_unmap(ih, ih->tx_ring[thread_id].addr_dma);
		free(ih->rx_ring[thread_id].slot_index);
		ufp_dma_unmap(ih, ih->rx_ring[thread_id].addr_dma);
	}
	munmap(desc->addr_virt, size);
err_mmap:
	free(desc);
err_alloc_desc:
	return NULL;
}

void ufp_desc_release(struct ufp_handle **ih_list, int ih_num,
	int thread_id, struct ufp_desc *desc)
{
	int i;

	ufp_mem_destroy(desc->node);

	for(i = 0; i < ih_num; i++){
		struct ufp_handle *ih;

		ih = ih_list[i];
		free(ih->tx_ring[thread_id].slot_index);
		ufp_dma_unmap(ih, ih->tx_ring[thread_id].addr_dma);
		free(ih->rx_ring[thread_id].slot_index);
		ufp_dma_unmap(ih, ih->rx_ring[thread_id].addr_dma);
	}

	munmap(desc->addr_virt, SIZE_1GB);
	free(desc);
	return;
}

struct ufp_buf *ufp_buf_alloc(struct ufp_handle **ih_list,
	int ih_num, uint32_t count, uint32_t buf_size)
{
	struct ufp_buf *buf;
	void	*addr_virt;
	unsigned long addr_dma, size;
	int *slots;
	int ret, i, mapped_ports = 0;

	buf = malloc(sizeof(struct ufp_buf));
	if(!buf)
		goto err_alloc_buf;

	buf->addr_dma = malloc(sizeof(unsigned long) * ih_num);
	if(!buf->addr_dma)
		goto err_alloc_buf_addr_dma;

	/*
	 * XXX: Should we add buffer padding for memory interleaving?
	 * DPDK does so in rte_mempool.c/optimize_object_size().
	 */
	size = buf_size * (ih_num * count);
	addr_virt = mmap(NULL, size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
	if(addr_virt == MAP_FAILED)
		goto err_mmap;

	for(i = 0; i < ih_num; i++, mapped_ports++){
		ret = ufp_dma_map(ih_list[i], addr_virt, &addr_dma, size);
		if(ret < 0)
			goto err_ufp_dma_map;

		buf->addr_dma[i] = addr_dma;
	}

	slots = malloc(sizeof(int) * (count * ih_num));
	if(!slots)
		goto err_alloc_slots;

	buf->addr_virt = addr_virt;
	buf->buf_size = buf_size;
	buf->count = count;
	buf->slots = slots;

	for(i = 0; i < buf->count * ih_num; i++){
		buf->slots[i] = 0;
	}

	return buf;

err_alloc_slots:
err_ufp_dma_map:
	for(i = 0; i < mapped_ports; i++){
		ufp_dma_unmap(ih_list[i], buf->addr_dma[i]);
	}
	munmap(addr_virt, size);
err_mmap:
	free(buf->addr_dma);
err_alloc_buf_addr_dma:
	free(buf);
err_alloc_buf:
	return NULL;
}

void ufp_buf_release(struct ufp_buf *buf,
	struct ufp_handle **ih_list, int ih_num)
{
	int i, ret;
	unsigned long size;

	free(buf->slots);

	for(i = 0; i < ih_num; i++){
		ret = ufp_dma_unmap(ih_list[i], buf->addr_dma[i]);
		if(ret < 0)
			perror("failed to unmap buf");
	}

	size = buf->buf_size * (ih_num * buf->count);
	munmap(buf->addr_virt, size);
	free(buf->addr_dma);
	free(buf);

	return;
}

static int ufp_dma_map(struct ufp_handle *ih, void *addr_virt,
	unsigned long *addr_dma, unsigned long size)
{
	struct ufp_map_req req_map;

	req_map.addr_virt = (unsigned long)addr_virt;
	req_map.addr_dma = 0;
	req_map.size = size;
	req_map.cache = IXGBE_DMA_CACHE_DISABLE;

	if(ioctl(ih->fd, UFP_MAP, (unsigned long)&req_map) < 0)
		return -1;

	*addr_dma = req_map.addr_dma;
	return 0;
}

static int ufp_dma_unmap(struct ufp_handle *ih, unsigned long addr_dma)
{
	struct ufp_unmap_req req_unmap;

	req_unmap.addr_dma = addr_dma;

	if(ioctl(ih->fd, UFP_UNMAP, (unsigned long)&req_unmap) < 0)
		return -1;

	return 0;
}

static struct ufp_ops *ufp_ops_alloc(uint16_t device_id)
{
	struct ufp_ops *ops;
	int err;

	ops = malloc(sizeof(struct ufp_ops));
	if(!ops)
		goto err_alloc_ops;

	memset(ops, 0, sizeof(struct ufp_ops));

	switch(device_id){
	case IXGBE_DEV_ID_82599_VF:
	case IXGBE_DEV_ID_X540_VF:
	case IXGBE_DEV_ID_X550_VF:
	case IXGBE_DEV_ID_X550EM_X_VF:
		err = ufp_ixgbevf_init(ops);
		if(err)
			goto err_init_device;
		break;
	default:
		goto err_init_device;
	}

	ops->device_id = device_id;
	return ops;

err_init_device:
	free(ops);
err_alloc_ops:
	return NULL;
}

static void ufp_ops_release(struct ufp_ops *ops)
{
	switch(ops->device_id){
	case IXGBE_DEV_ID_82599_VF:
	case IXGBE_DEV_ID_X540_VF:
	case IXGBE_DEV_ID_X550_VF:
	case IXGBE_DEV_ID_X550EM_X_VF:
		ufp_ixgbevf_destroy(ops);
		break;
	default:
		break;
	}

	free(ops);
	return;
}

struct ufp_handle *ufp_open(const char *ifname,
	unsigned int num_qps_req, unsigned int num_rx_desc,
	unsigned int num_tx_desc)
{
	struct ufp_handle *ih;
	char filename[FILENAME_SIZE];
	struct ufp_info_req req;
	int err;

	ih = malloc(sizeof(struct ufp_handle));
	if (!ih)
		goto err_alloc_ih;
	memset(ih, 0, sizeof(struct ufp_handle));

	snprintf(filename, sizeof(filename), "/dev/ufp/%s", ifname);
	ih->fd = open(filename, O_RDWR);
	if (ih->fd < 0)
		goto err_open;

	/* Get device information */
	memset(&req, 0, sizeof(struct ufp_info_req));
	err = ioctl(ih->fd, UFP_INFO, (unsigned long)&req);
	if(err < 0)
		goto err_ioctl_info;

	ih->bar_size = req.mmio_size;

	/* Map PCI config register space */
	ih->bar = mmap(NULL, ih->bar_size,
		PROT_READ | PROT_WRITE, MAP_SHARED, ih->fd, 0);
	if(ih->bar == MAP_FAILED)
		goto err_mmap;

	ih->ops = ufp_ops_alloc(req.device_id);
	if(!ih->ops)
		goto err_ops_alloc;

	err = ih->ops->reset_hw(ih);
	if(err < 0)
		goto err_ops_reset_hw;

	err = ih->ops->set_device_params(ih, num_qps_req,
		num_rx_desc, num_tx_desc);
	if(err < 0)
		goto err_ops_get_device_params;

	ih->rx_ring = malloc(sizeof(struct ufp_ring) * ih->num_qps);
	if(!ih->rx_ring)
		goto err_alloc_rx_ring;

	ih->tx_ring = malloc(sizeof(struct ufp_ring) * ih->num_qps);
	if(!ih->tx_ring)
		goto err_alloc_tx_ring;

	strncpy(ih->ifname, ifname, sizeof(ih->ifname));

	return ih;

err_alloc_tx_ring:
	free(ih->rx_ring);
err_alloc_rx_ring:

err_ops_get_device_params:
err_ops_reset_hw:
	ufp_ops_release(ih->ops);
err_ops_alloc:
	munmap(ih->bar, ih->bar_size);
err_mmap:
err_ioctl_info:
	close(ih->fd);
err_open:
	free(ih);
err_alloc_ih:
	return NULL;
}

void ufp_close(struct ufp_handle *ih)
{
	free(ih->tx_ring);
	free(ih->rx_ring);

	ufp_ops_release(ih->ops);
	munmap(ih->bar, ih->bar_size);
	close(ih->fd);
	free(ih);

	return;
}

int ufp_up(struct ufp_handle *ih, unsigned int irq_rate,
	unsigned int mtu_frame, unsigned int promisc,
	unsigned int rx_budget, unsigned int tx_budget)
{
	struct ufp_start_req req;
	int err;

	memset(&req, 0, sizeof(struct ufp_start_req));
	req.num_irqs = (ih->num_qps * 2) + ih->num_misc_irqs;
	if(ioctl(ih->fd, UFP_START, (unsigned long)&req) < 0)
		goto err_ioctl_start;

	err = ih->ops->configure_irq(ih, irq_rate);
	if(err < 0)
		goto err_ops_configure_irq;

	err = ih->ops->configure_tx(ih);
	if(err < 0)
		goto err_configure_tx;

	err = ih->ops->configure_rx(ih, mtu_frame, !!promisc);
	if(err < 0)
		goto err_configure_rx;

	ih->rx_budget = rx_budget;
	ih->tx_budget = tx_budget;

	return err;

err_configure_rx:
err_configure_tx:
err_ops_configure_irq:
err_ioctl_start:
	return -1;
}

void ufp_down(struct ufp_handle *ih)
{
	int err;

	err = ih->ops->stop_adapter(ih);
	if(err < 0)
		goto err_stop_adapter;

	if(ioctl(ih->fd, UFP_STOP, 0) < 0)
		goto err_ioctl_stop;

err_ioctl_stop:
err_stop_adapter:
	return;
}

static struct ufp_irq_handle *ufp_irq_open(struct ufp_handle *ih,
	enum ufp_irq_type type, unsigned int irq_idx, unsigned int core_id)
{
	struct ufp_irq_handle *irqh;
	uint64_t qmask;
	int efd, ret;
	struct ufp_irqbind_req req;

	efd = eventfd(0, 0);
	if(efd < 0)
		goto err_open_efd;

	req.event_fd	= efd;

	switch(type){
	case UFP_IRQ_RX:
		req.entry_idx = irq_idx;
		break;
	case UFP_IRQ_TX:
		req.entry_idx = irq_idx + ih->num_qps;
		break;
	case UFP_IRQ_MISC:
		req.entry_idx = irq_idx + (ih->num_qps * 2);
		break;
	default:
		goto err_invalid_type;
	}

	ret = ioctl(ih->fd, UFP_IRQBIND, (unsigned long)&req);
	if(ret < 0){
		printf("failed to UFP_IRQ\n");
		goto err_irq_bind;
	}

	ret = ufp_irq_setaffinity(req.vector, core_id);
	if(ret < 0){
		printf("failed to set afinity\n");
		goto err_irq_setaffinity;
	}

	switch(type){
	case UFP_IRQ_RX:
		qmask = 1 << irq_idx;
		break;
	case UFP_IRQ_TX:
		qmask = 1 << (irq_idx + ih->num_qps);
		break;
	default:
		goto err_undefined_type;
	}

	irqh = malloc(sizeof(struct ufp_irq_handle));
	if(!irqh)
		goto err_alloc_handle;

	irqh->fd		= efd;
	irqh->qmask		= qmask;
	irqh->vector		= req.vector;

	return irqh;

err_alloc_handle:
err_undefined_type:
err_irq_setaffinity:
err_irq_bind:
err_invalid_type:
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

static int ufp_irq_setaffinity(unsigned int vector, unsigned int core_id)
{
	FILE *file;
	char filename[FILENAME_SIZE];
	uint32_t mask_low, mask_high;
	int ret;

	mask_low = core_id <= 31 ? 1 << core_id : 0;
	mask_high = core_id <= 31 ? 0 : 1 << (core_id - 31);

	snprintf(filename, sizeof(filename),
		"/proc/irq/%d/smp_affinity", vector);
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

