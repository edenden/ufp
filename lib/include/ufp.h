#ifndef _UFP_H
#define _UFP_H

struct ufp_handle;
struct ufp_irq_handle;
struct ufp_desc;
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
struct ufp_plane *ufp_plane_alloc(struct ufp_handle **ih_list,
	struct ufp_buf *buf, int ih_num, unsigned int thread_id,
	unsigned int core_id);
void ufp_plane_release(struct ufp_plane *plane, int ih_num);
struct ufp_desc *ufp_desc_alloc(struct ufp_handle **ih_list, int ih_num,
	int thread_id);
void ufp_desc_release(struct ufp_handle **ih_list, int ih_num,
	int thread_id, struct ufp_desc *desc);
struct ufp_buf *ufp_buf_alloc(struct ufp_handle **ih_list,
	int ih_num, uint32_t count, uint32_t buf_size);
void ufp_buf_release(struct ufp_buf *buf,
	struct ufp_handle **ih_list, int ih_num);
struct ufp_handle *ufp_open(const char *ifname,
	unsigned int num_qps_req, unsigned int num_rx_desc,
	unsigned int num_tx_desc);
void ufp_close(struct ufp_handle *ih);
int ufp_up(struct ufp_handle *ih, unsigned int irq_rate,
	unsigned int mtu_frame, unsigned int promisc,
	unsigned int rx_budget, unsigned int tx_budget);
void ufp_down(struct ufp_handle *ih);

/* MEM */
void *ufp_mem_alloc(struct ufp_desc *desc,
	unsigned int size);
void ufp_mem_free(void *addr_free);

/* RTX */
void ufp_irq_unmask_queues(struct ufp_plane *plane,
	unsigned int port_index, struct ufp_irq_handle *irqh);
void ufp_rx_assign(struct ufp_plane *plane, unsigned int port_index,
	struct ufp_buf *buf);
void ufp_tx_assign(struct ufp_plane *plane, unsigned int port_index,
	struct ufp_buf *buf, struct ufp_packet *packet);
void ufp_tx_xmit(struct ufp_plane *plane, unsigned int port_index);
unsigned int ufp_rx_clean(struct ufp_plane *plane, unsigned int port_index,
	struct ufp_buf *buf, struct ufp_packet *packet);
void ufp_tx_clean(struct ufp_plane *plane, unsigned int port_index,
	struct ufp_buf *buf);
inline int ufp_slot_assign(struct ufp_buf *buf,
	struct ufp_plane *plane, unsigned int port_index);
inline void ufp_slot_release(struct ufp_buf *buf,
	int slot_index);
inline void *ufp_slot_addr_virt(struct ufp_buf *buf,
	uint16_t slot_index);
inline unsigned int ufp_slot_size(struct ufp_buf *buf);

/* API */
unsigned int ufp_bufsize_get(struct ufp_handle *ih);
void *ufp_macaddr_default(struct ufp_handle *ih);
unsigned int ufp_mtu_get(struct ufp_handle *ih);
char *ufp_ifname_get(struct ufp_handle *ih);
void *ufp_macaddr(struct ufp_plane *plane,
	unsigned int port_index);
int ufp_irq_fd(struct ufp_plane *plane, unsigned int port_index,
	enum ufp_irq_type type);
struct ufp_irq_handle *ufp_irq_handle(struct ufp_plane *plane,
	unsigned int port_index, enum ufp_irq_type type);
unsigned long ufp_count_rx_alloc_failed(struct ufp_plane *plane,
	unsigned int port_index);
unsigned long ufp_count_rx_clean_total(struct ufp_plane *plane,
	unsigned int port_index);
unsigned long ufp_count_tx_xmit_failed(struct ufp_plane *plane,
	unsigned int port_index);
unsigned long ufp_count_tx_clean_total(struct ufp_plane *plane,
	unsigned int port_index);


#endif /* _UFP_H */
