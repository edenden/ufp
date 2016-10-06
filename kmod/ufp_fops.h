#ifndef _UFP_FOPS_H
#define _UFP_FOPS_H

#define MISCDEV_NAME_SIZE	32

#define UFP_INFO		_IOW('E', 201, int)
/* MAC and PHY info */
struct ufp_info_req {
	__u64			mmio_base;
	__u64			mmio_size;

	__u16			device_id;
	__u16			vendor_id;
};

#define UFP_START		_IOW('E', 202, int)
struct ufp_start_req {
	__u32			num_irqs;
};

#define UFP_STOP		_IOW('E', 203, int)

#define UFP_MAP			_IOW('U', 210, int)
struct ufp_map_req {
	__u64			addr_virt;
	__u64			addr_dma;
	__u64			size;
	__u8			cache;
};

#define UFP_UNMAP		_IOW('U', 211, int)
struct ufp_unmap_req {
	__u64			addr_dma;
};

#define UFP_IRQBIND		_IOW('E', 220, int)
struct ufp_irqbind_req {
	__u32			entry_idx;
	__s32			event_fd;

	__u32			vector;
};

int ufp_miscdev_register(struct ufp_device *device);
void ufp_miscdev_deregister(struct ufp_device *device);

#endif /* _UFP_FOPS_H */
