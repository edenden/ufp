#ifndef _UFP_FOPS_H
#define _UFP_FOPS_H

#define MISCDEV_NAME_SIZE	32

#define IXMAP_INFO		_IOW('E', 201, int)
/* MAC and PHY info */
struct ufp_info_req {
	unsigned long		mmio_base;
	unsigned long		mmio_size;

	__u32			num_rx_queues;
	__u32			num_tx_queues;
};

#define IXMAP_UP		_IOW('E', 202, int)
struct ufp_up_req {
	__u32			num_rx_queues;
	__u32			num_tx_queues;
};

#define IXMAP_DOWN		_IOW('E', 203, int)
#define IXMAP_RESET		_IOW('E', 204, int)

#define IXMAP_MAP		_IOW('U', 210, int)
struct ufp_map_req {
	unsigned long		addr_virtual;
	unsigned long		addr_dma;
	unsigned long		size;
	uint8_t			cache;
};

#define IXMAP_UNMAP		_IOW('U', 211, int)
struct ufp_unmap_req {
	unsigned long		addr_dma;
};

#define IXMAP_IRQ		_IOW('E', 220, int)
struct ufp_irq_req {
	enum ufp_irq_type	type;
	__u32			queue_idx;
	int			event_fd;

	__u32			vector;
	__u16			entry;
};

int ufp_miscdev_register(struct ufp_port *port);
void ufp_miscdev_deregister(struct ufp_port *port);

#endif /* _UFP_FOPS_H */
