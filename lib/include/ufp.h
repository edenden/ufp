#ifndef _UFP_H
#define _UFP_H

#include <ufp_list.h>

struct ufp_mpool;
struct ufp_dev;
struct ufp_iface;
struct ufp_irq;
struct ufp_buf;
struct ufp_plane;

struct ufp_packet {
        void                    *slot_buf;
        unsigned int            slot_size;
        int                     slot_index;
        unsigned int            flag;
};

#define UFP_PACKET_ERROR	0x00000001
#define UFP_PACKET_NOTEOP	0x00000002

enum ufp_irq_type {
	UFP_IRQ_RX = 0,
	UFP_IRQ_TX,
};

/* MAIN */
struct ufp_plane *ufp_plane_alloc(struct ufp_dev **devs, int num_devs,
	struct ufp_buf *buf, unsigned int thread_id, unsigned int core_id);
void ufp_plane_release(struct ufp_plane *plane);
struct ufp_mpool *ufp_mpool_init();
void ufp_mpool_destroy(struct ufp_mpool *mpool);
struct ufp_buf *ufp_alloc_buf(struct ufp_dev **devs, int num_devs,
	uint32_t buf_size, uint32_t buf_count, struct ufp_mpool *mpool);
void ufp_release_buf(struct ufp_dev **devs, int num_devs,
	struct ufp_buf *buf);
struct ufp_dev *ufp_open(const char *name);
void ufp_close(struct ufp_dev *dev);
int ufp_up(struct ufp_dev *dev, struct ufp_mpool **mpools,
	unsigned int num_qps, unsigned int mtu_frame,
	unsigned int promisc, unsigned int rx_budget,
	unsigned int tx_budget);
void ufp_down(struct ufp_dev *dev);

/* MEM */
void *ufp_mem_alloc(struct ufp_mpool *mpool, unsigned int size);
void ufp_mem_free(void *addr_free);

/* IO */
void ufp_irq_unmask_queues(struct ufp_plane *plane,
	unsigned int port_idx, struct ufp_irq *irq);
void ufp_rx_assign(struct ufp_plane *plane, unsigned int port_idx,
	struct ufp_buf *buf);
void ufp_tx_assign(struct ufp_plane *plane, unsigned int port_idx,
	struct ufp_buf *buf, struct ufp_packet *packet);
void ufp_tx_xmit(struct ufp_plane *plane, unsigned int port_idx);
unsigned int ufp_rx_clean(struct ufp_plane *plane, unsigned int port_idx,
	struct ufp_buf *buf, struct ufp_packet *packet);
void ufp_tx_clean(struct ufp_plane *plane, unsigned int port_idx,
	struct ufp_buf *buf);
inline int ufp_slot_assign(struct ufp_buf *buf,
	struct ufp_plane *plane, unsigned int port_idx);
inline void ufp_slot_release(struct ufp_buf *buf,
	int slot_index);
inline void *ufp_slot_addr_virt(struct ufp_buf *buf,
	uint16_t slot_index);
inline unsigned int ufp_slot_size(struct ufp_buf *buf);

/* API */
void *ufp_macaddr_default(struct ufp_iface *iface);
unsigned int ufp_mtu_get(struct ufp_iface *iface);
char *ufp_ifname_get(struct ufp_dev *dev);
void *ufp_macaddr(struct ufp_plane *plane,
	unsigned int port_idx);
unsigned short ufp_portnum(struct ufp_plane *plane);
int ufp_irq_fd(struct ufp_plane *plane, unsigned int port_idx,
	enum ufp_irq_type type);
struct ufp_irq *ufp_irq(struct ufp_plane *plane,
	unsigned int port_idx, enum ufp_irq_type type);
unsigned long ufp_count_rx_alloc_failed(struct ufp_plane *plane,
	unsigned int port_index);
unsigned long ufp_count_rx_clean_total(struct ufp_plane *plane,
	unsigned int port_index);
unsigned long ufp_count_tx_xmit_failed(struct ufp_plane *plane,
	unsigned int port_index);
unsigned long ufp_count_tx_clean_total(struct ufp_plane *plane,
	unsigned int port_index);

#endif /* _UFP_H */
