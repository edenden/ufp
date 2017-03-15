#ifndef _LIBUFP_MAIN_H
#define _LIBUFP_MAIN_H

#include <config.h>
#include <net/if.h>
#include <stdint.h>
#include <linux/types.h>
#include <net/ethernet.h>
#include <time.h>
#include "lib_list.h"

#define DRIVER_PATH		"/usr/local/lib/dev/"
#define ALIGN(x,a)		__ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))

#define FILENAME_SIZE 256
#define SIZE_1GB (1ul << 30)
#define UFP_RXBUF_SIZE 2048

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

#define msleep(n) ({			\
	struct timespec ts;		\
	ts.tv_sec = 0;			\
	ts.tv_nsec = ((n) * 1000000);	\
	nanosleep(&ts, NULL);		\
})
#define usleep(n) ({			\
	struct timespec ts;		\
	ts.tv_sec = 0;			\
	ts.tv_nsec = ((n) * 1000);	\
	nanosleep(&ts, NULL);		\
})

#define BIT(nr) \
	(1UL << (nr))
#define BIT_ULL(n) \
	(1ULL << (n))

#define upper32(n) ((uint32_t)(((n) >> 16) >> 16))
#define lower32(n) ((uint32_t)(n))

#ifdef DEBUG
#define ufp_print(args...) printf("ixgbe: " args)
#else
#define ufp_print(args...)
#endif

#define UFP_READ32(dev, reg) \
	ufp_readl((dev)->bar + (reg))
#define UFP_WRITE32(dev, reg, value) \
	ufp_writel((value), (dev)->bar + (reg))

struct ufp_mpool {
	struct ufp_mnode	*node;
	void			*addr_virt;
};

struct ufp_ring {
	void			*addr_virt;
	unsigned long		addr_dma;

	uint8_t			*tail;
	uint16_t		next_to_use;
	uint16_t		next_to_clean;
	int32_t			*slot_index;
};

#define UFP_SLOT_INFLIGHT 0x1

struct ufp_buf {
	void			*addr_virt;
	unsigned long		addr_dma;
	uint32_t		slot_size;
	uint64_t		size;
	uint32_t		count;
	int32_t			*slots;
};

struct ufp_iface {
	struct ufp_ring		*tx_ring;
	struct ufp_ring		*rx_ring;

	struct ufp_irq		**rx_irq;
	struct ufp_irq		**tx_irq;

	uint32_t		num_tx_desc;
	unsigned long		size_tx_desc;
	uint32_t		num_rx_desc;
	unsigned long		size_rx_desc;

	uint32_t		rx_budget;
	uint32_t		tx_budget;

	uint32_t		num_qps;
	uint32_t		irq_rate;
	uint32_t		promisc;
	uint32_t		mtu_frame;
	uint8_t			mac_addr[ETH_ALEN];

	int			*tap_fds;
	int			tap_index;

	char			name[IFNAMSIZ];
	void			*drv_data;
	struct list_node	list;
};

struct ufp_dev {
	int			fd;
	void			*bar;
	unsigned long		bar_size;
	char			name[IFNAMSIZ];
	struct ufp_ops		*ops;

	struct list_head	iface;
	uint16_t		num_ifaces;
	uint32_t		num_misc_irqs;
	struct ufp_irq		**misc_irq;
	uint16_t		device_id;
	uint16_t		vendor_id;

	void			*drv_data;
};

struct ufp_irq {
	int			fd;
	uint16_t		entry_idx;
	uint32_t		vector;
};

struct ufp_port {
	/* struct dev specific parameters */
	void			*bar;
	struct ufp_ops		*ops;
	uint32_t		dev_idx;

	/* struct iface specific parameters */
	struct ufp_ring		*rx_ring;
	struct ufp_ring		*tx_ring;
	struct ufp_irq		*rx_irq;
	struct ufp_irq		*tx_irq;
	uint32_t		mtu_frame;
	uint32_t		num_tx_desc;
	uint32_t		num_rx_desc;
	uint32_t		num_qps;
	uint32_t		rx_budget;
	uint32_t		tx_budget;
	uint8_t			mac_addr[ETH_ALEN];
	int			tap_fd;
	int			tap_index;

	/* original parameters */
	uint32_t		rx_slot_next;
	uint32_t		rx_slot_offset;
	uint32_t		tx_suspended;
	unsigned long		count_rx_alloc_failed;
	unsigned long		count_rx_clean_total;
	unsigned long		count_tx_xmit_failed;
	unsigned long		count_tx_clean_total;
};

struct ufp_plane {
	struct ufp_port 	*ports;
	uint16_t		num_ports;
};

struct ufp_packet {
	void			*slot_buf;
	unsigned int		slot_size;
	int			slot_index;
	unsigned int		flag;
};

#define UFP_PACKET_ERROR	0x00000001
#define UFP_PACKET_EOP		0x00000002

struct ufp_ops {
	/* For configuration */
	int	(*open)(struct ufp_dev *dev);
	void	(*close)(struct ufp_dev *dev);
	int	(*up)(struct ufp_dev *dev);
	void	(*down)(struct ufp_dev *dev);

	/* For forwarding */
	void	(*unmask_queues)(void *bar, uint16_t entry_idx);
	void	(*fill_rx_desc)(struct ufp_ring *rx_ring, uint16_t index,
			uint64_t addr_dma);
	int	(*fetch_rx_desc)(struct ufp_ring *rx_ring, uint16_t index,
			struct ufp_packet *packet);
	void	(*fill_tx_desc)(struct ufp_ring *tx_ring, uint16_t index,
			uint64_t addr_dma, struct ufp_packet *packet);
	int	(*fetch_tx_desc)(struct ufp_ring *tx_ring, uint16_t index);
};

enum ufp_irq_type {
	UFP_IRQ_RX = 0,
	UFP_IRQ_TX,
};

inline uint32_t ufp_readl(const volatile void *addr);
inline void ufp_writel(uint32_t b, volatile void *addr);

#endif /* _LIBUFP_MAIN_H */
