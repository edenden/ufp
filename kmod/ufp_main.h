#ifndef _UFP_MAIN_H
#define _UFP_MAIN_H

#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/semaphore.h>

/* common prefix used by pr_<> macros */
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define EVENTFD_INCREMENT	1

struct ufp_device {
	struct list_head	areas;
	unsigned int		started;
	struct miscdevice	miscdev;
	struct semaphore	sem;
	atomic_t		refcount;

	struct pci_dev		*pdev;
	unsigned long		iobase;
	unsigned long		iolen;
	u8 __iomem		*hw_addr; /* unused */

	struct msix_entry	*msix_entries;
	struct ufp_irq		**irqs;
	u32			num_irqs;
};

struct ufp_irq {
	struct eventfd_ctx	*efd_ctx;
	struct msix_entry	*msix_entry;
};

#define I40E_DEV_ID_SFP_XL710		0x1572
#define IXGBE_DEV_ID_82599_VF		0x10ED
#define IXGBE_DEV_ID_X540_VF		0x1515
#define IXGBE_DEV_ID_X550_VF		0x1565
#define IXGBE_DEV_ID_X550EM_X_VF	0x15A8

int ufp_start(struct ufp_device *device, u32 num_irqs);
void ufp_stop(struct ufp_device *device);
int ufp_irq_bind(struct ufp_device *device, u32 entry_idx,
	int event_fd, u32 *vector);
int ufp_device_inuse(struct ufp_device *device);
void ufp_device_get(struct ufp_device *device);
void ufp_device_put(struct ufp_device *device);

#endif /* _UFP_MAIN_H */
