#ifndef _UFP_MAIN_H
#define _UFP_MAIN_H

#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/semaphore.h>

/* common prefix used by pr_<> macros */
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define IXGBE_IVAR_ALLOC_VAL		0x80 /* Interrupt Allocation valid */
#define EVENTFD_INCREMENT		1
#define IXGBEVF_MAX_RX_DESC_POLL	10

enum ufp_irq_type {
	IXMAP_IRQ_RX = 0,
	IXMAP_IRQ_TX,
};

struct ufp_mac_info;
struct ufp_mbx_info;

struct ufp_hw {
	u8 __iomem		*hw_addr;
	struct ufp_mac_info	*mac;
	struct ufp_mbx_info	*mbx;
	u16			device_id;
	u16			subsystem_vendor_id;
	u16			subsystem_device_id;
	u16			vendor_id;
	u8			revision_id;
	int			api_version;
};

struct ufp_port {
	struct list_head	list;
	struct list_head	areas;
	unsigned int		id;
	uint8_t			up;

	struct miscdevice	miscdev;

	struct semaphore	sem;
	atomic_t		refcount;

	uint64_t		dma_mask;
	struct pci_dev		*pdev;
	unsigned long		iobase;
	unsigned long		iolen;
	struct ufp_hw		*hw;
	char			eeprom_id[32];

	struct msix_entry	*msix_entries;
	uint32_t		num_q_vectors;
	struct ufp_irq		**rx_irq;
	struct ufp_irq		**tx_irq;

	uint32_t		num_rx_queues;
	uint32_t		num_tx_queues;
	uint16_t		num_interrupt_rate;

	spinlock_t		mbx_lock;
};

struct ufp_irq {
	struct eventfd_ctx	*efd_ctx;
	struct msix_entry	*msix_entry;
};

#define IXGBE_DEV_ID_82599_VF                   0x10ED
#define IXGBE_DEV_ID_X540_VF                    0x1515
#define IXGBE_DEV_ID_X550_VF                    0x1565
#define IXGBE_DEV_ID_X550EM_X_VF                0x15A8

uint32_t ufp_read_reg(struct ufp_hw *hw, uint32_t reg);
void ufp_write_reg(struct ufp_hw *hw, uint32_t reg,
	uint32_t value);
void ufp_write_flush(struct ufp_hw *hw);
int ufp_up(struct ufp_port *port);
int ufp_down(struct ufp_port *port);
void ufp_reset(struct ufp_port *port);
int ufp_irq_assign(struct ufp_port *port, enum ufp_irq_type type,
	uint32_t queue_idx, int event_fd, uint32_t *vector, uint16_t *entry);
int ufp_port_inuse(struct ufp_port *port);
void ufp_port_get(struct ufp_port *port);
void ufp_port_put(struct ufp_port *port);

#endif /* _UFP_MAIN_H */
