#ifndef _LIBUFP_MAIN_H
#define _LIBUFP_MAIN_H

#include <config.h>
#include <net/if.h>

#define ALIGN(x,a)		__ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))

#define FILENAME_SIZE 256
#define IXMAP_IFNAME "ixgbe"
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

#define IXMAP_SLOT_INFLIGHT 0x1

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

	struct ufp_ring		*tx_ring;
	struct ufp_ring		*rx_ring;

	uint32_t		num_tx_desc;
	uint32_t		num_rx_desc;
	uint32_t		rx_budget;
	uint32_t		tx_budget;

	uint32_t		num_queues;
	uint32_t		num_interrupt_rate;
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
	void			*irqreg[2];
	struct ufp_ring		*rx_ring;
	struct ufp_ring		*tx_ring;
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
};

enum {
	IXGBE_DMA_CACHE_DEFAULT = 0,
	IXGBE_DMA_CACHE_DISABLE,
	IXGBE_DMA_CACHE_WRITECOMBINE
};

enum ufp_irq_type {
	IXMAP_IRQ_RX = 0,
	IXMAP_IRQ_TX,
};

/* Receive Descriptor - Advanced */
union ufp_adv_rx_desc {
	struct {
		uint64_t pkt_addr; /* Packet buffer address */
		uint64_t hdr_addr; /* Header buffer address */
	} read;
	struct {
		struct {
			union {
				uint32_t data;
				struct {
					uint16_t pkt_info; /* RSS, Pkt type */
					uint16_t hdr_info; /* Splithdr, hdrlen */
				} hs_rss;
			} lo_dword;
			union {
				uint32_t rss; /* RSS Hash */
				struct {
					uint16_t ip_id; /* IP id */
					uint16_t csum; /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			uint32_t status_error; /* ext status/error */
			uint16_t length; /* Packet length */
			uint16_t vlan; /* VLAN tag */
		} upper;
	} wb;  /* writeback */
};

/* Transmit Descriptor - Advanced */
union ufp_adv_tx_desc {
	struct {
		uint64_t buffer_addr; /* Address of descriptor's data buf */
		uint32_t cmd_type_len;
		uint32_t olinfo_status;
	} read;
	struct {
		uint64_t rsvd; /* Reserved */
		uint32_t nxtseq_seed;
		uint32_t status;
	} wb;
};

#define UFP_INFO                _IOW('E', 201, int)
/* MAC and PHY info */
struct ufp_info_req {
        __u64                   mmio_base;
        __u64                   mmio_size;

        __u16                   device_id;
        __u16                   vendor_id;
};

#define UFP_ALLOCATE		_IOW('E', 202, int)
struct ufp_alloc_req {
        __u32                   num_rx_queues;
        __u32                   num_tx_queues;
};

#define UFP_RELEASE		_IOW('E', 203, int)

#define UFP_MAP			_IOW('U', 210, int)
struct ufp_map_req {
        __u64                   addr_virtual;
        __u64                   addr_dma;
        __u64                   size;
        __u8                    cache;
};

#define UFP_UNMAP               _IOW('U', 211, int)
struct ufp_unmap_req {
        __u64                   addr_dma;
};

#define UFP_IRQ         _IOW('E', 220, int)
struct ufp_irq_req {
        enum ufp_irq_type       type;
        __u32                   queue_idx;
        __s32                   event_fd;

        __u32                   vector;
        __u16                   entry;
};

inline uint32_t ufp_readl(const volatile void *addr);
inline void ufp_writel(uint32_t b, volatile void *addr);
inline uint32_t ufp_read_reg(struct ufp_handle *ih, uint32_t reg);
inline void ufp_write_reg(struct ufp_handle *ih, uint32_t reg, uint32_t value);
inline void ufp_write_flush(struct ufp_handle *ih);

#endif /* _LIBUFP_MAIN_H */
