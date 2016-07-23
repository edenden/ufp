#ifndef _LIBUFP_MAIN_H
#define _LIBUFP_MAIN_H

/*#include <config.h>*/
#include <net/if.h>
#include <stdint.h>
#include <linux/types.h>
#include <net/ethernet.h>

#define IXGBE_DEV_ID_82599_VF		0x10ED
#define IXGBE_DEV_ID_X540_VF		0x1515
#define IXGBE_DEV_ID_X550_VF		0x1565
#define IXGBE_DEV_ID_X550EM_X_VF	0x15A8

#define ALIGN(x,a)		__ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))

#define FILENAME_SIZE 256
#define UFP_IFNAME "ixgbe"
#define SIZE_1GB (1ul << 30)

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

#define CONFIG_X86_L1_CACHE_SHIFT \
				(6)
#define L1_CACHE_SHIFT		(CONFIG_X86_L1_CACHE_SHIFT)
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)

#ifdef DEBUG
#define ufp_print(args...) printf("ixgbe: " args)
#else
#define ufp_print(args...)
#endif

struct ufp_ring {
	void		*addr_virt;
	unsigned long	addr_dma;

	uint8_t		*tail;
	uint16_t	next_to_use;
	uint16_t	next_to_clean;
	int32_t		*slot_index;
};

struct ufp_desc {
	void			*addr_virt;
	struct ufp_mnode	*node;
	int			core_id;
};

#define UFP_SLOT_INFLIGHT 0x1

struct ufp_buf {
	void			*addr_virt;
	unsigned long		*addr_dma;
	uint32_t		buf_size;
	uint32_t		count;
	int32_t			*slots;
};

struct ufp_handle {
 	int			fd;
	void			*bar;
	unsigned long		bar_size;

	struct ufp_ops		*ops;
	struct ufp_ring		*tx_ring;
	struct ufp_ring		*rx_ring;

	uint32_t		num_tx_desc;
	unsigned long		size_tx_desc;
	uint32_t		num_rx_desc;
	unsigned long		size_rx_desc;

	uint32_t		rx_budget;
	uint32_t		tx_budget;

	uint32_t		num_queues;
	uint32_t		irq_rate;
	uint32_t		promisc;
	uint32_t		mtu_frame;
	uint32_t		buf_size;
	uint8_t			mac_addr[ETH_ALEN];
	char			interface_name[IFNAMSIZ];
};

struct ufp_irq_handle {
	int			fd;
	uint64_t		qmask;
	uint32_t		vector;
};

struct ufp_port {
	void			*bar;
	struct ufp_ring		*rx_ring;
	struct ufp_ring		*tx_ring;
	struct ufp_ops		*ops;
	struct ufp_irq_handle	*rx_irq;
	struct ufp_irq_handle	*tx_irq;
	uint32_t		rx_slot_next;
	uint32_t		rx_slot_offset;
	uint32_t		tx_suspended;
	uint32_t		mtu_frame;
	uint32_t		num_tx_desc;
	uint32_t		num_rx_desc;
	uint32_t		num_queues;
	uint32_t		rx_budget;
	uint32_t		tx_budget;
	uint8_t			mac_addr[ETH_ALEN];
	const char		*interface_name;

	unsigned long		count_rx_alloc_failed;
	unsigned long		count_rx_clean_total;
	unsigned long		count_tx_xmit_failed;
	unsigned long		count_tx_clean_total;
};

struct ufp_plane {
	struct ufp_port 	*ports;
};

struct ufp_packet {
	void			*slot_buf;
	unsigned int		slot_size;
	int			slot_index;
	unsigned int		flag;
};

struct ufp_ops {
	/* For configuration */
	int			(*reset_hw)(struct ufp_handle *);
	int			(*set_device_params)(struct ufp_handle *,
					uint32_t, uint32_t, uint32_t);
	int			(*configure_irq)(struct ufp_handle *,
					uint32_t);
	int			(*configure_tx)(struct ufp_handle *);
	int			(*configure_rx)(struct ufp_handle *,
					uint32_t, uint32_t);
	int			(*stop_adapter)(struct ufp_handle *);

	/* For forwarding */
	void			(*unmask_queues)(void *, uint64_t);
	void			(*set_rx_desc)(struct ufp_ring *, uint16_t,
					uint64_t);
	int			(*check_rx_desc)(struct ufp_ring *, uint16_t);
	void			(*get_rx_desc)(struct ufp_ring *, uint16_t,
					struct ufp_packet *);
	void			(*set_tx_desc)(struct ufp_ring *, uint16_t,
					uint64_t, struct ufp_packet *);
	int			(*check_tx_desc)(struct ufp_ring *, uint16_t);

	void			*data;
	uint16_t		device_id;
};

enum {
	IXGBE_DMA_CACHE_DEFAULT = 0,
	IXGBE_DMA_CACHE_DISABLE,
	IXGBE_DMA_CACHE_WRITECOMBINE
};

enum ufp_irq_type {
	UFP_IRQ_RX = 0,
	UFP_IRQ_TX,
};

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
	__u32			num_rx_queues;
	__u32			num_tx_queues;
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
	enum ufp_irq_type	type;
	__u32			queue_idx;
	__s32			event_fd;

	__u32			vector;
	__u16			entry;
};

inline uint32_t ufp_readl(const volatile void *addr);
inline void ufp_writel(uint32_t b, volatile void *addr);
inline uint32_t ufp_read_reg(struct ufp_handle *ih, uint32_t reg);
inline void ufp_write_reg(struct ufp_handle *ih, uint32_t reg, uint32_t value);
inline void ufp_write_flush(struct ufp_handle *ih);

#endif /* _LIBUFP_MAIN_H */
