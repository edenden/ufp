#ifndef _UFP_MAIN_H
#define _UFP_MAIN_H

#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/semaphore.h>

/* common prefix used by pr_<> macros */
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define EVENTFD_INCREMENT	1

enum ufp_irq_type {
	IXMAP_IRQ_RX = 0,
	IXMAP_IRQ_TX,
};

struct ufp_device_list_head {
	struct list_head	head;
	struct semaphore	sem;
};

struct ufp_device {
	struct list_head	list;
	struct list_head	areas;
	unsigned int		id;
	unsigned int		up;

	struct miscdevice	miscdev;

	struct semaphore	sem;
	atomic_t		refcount;

	dma_addr_t		dma_mask;
	struct pci_dev		*pdev;
	unsigned long		iobase;
	unsigned long		iolen;
	u8 __iomem		*hw_addr; /* unused */

	struct msix_entry	*msix_entries;
	u32			num_q_vectors;
	struct ufp_irq		**rx_irq;
	struct ufp_irq		**tx_irq;

	u32			num_rx_queues;
	u32			num_tx_queues;
};

struct ufp_irq {
	struct eventfd_ctx	*efd_ctx;
	struct msix_entry	*msix_entry;
};

#define IXGBE_DEV_ID_82599_VF                   0x10ED
#define IXGBE_DEV_ID_X540_VF                    0x1515
#define IXGBE_DEV_ID_X550_VF                    0x1565
#define IXGBE_DEV_ID_X550EM_X_VF                0x15A8

int ufp_up(struct ufp_device *device);
int ufp_down(struct ufp_device *device);
int ufp_irq_assign(struct ufp_device *device, enum ufp_irq_type type,
	u32 queue_idx, int event_fd, u32 *vector, u16 *entry);
int ufp_device_inuse(struct ufp_device *device);
void ufp_device_get(struct ufp_device *device);
void ufp_device_put(struct ufp_device *device);

#endif /* _UFP_MAIN_H */
