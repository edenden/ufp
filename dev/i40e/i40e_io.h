#ifndef _I40E_IO_H__
#define _I40E_IO_H__

/*
 * See 8.3.2.1 - Receive Descriptor - Read Format
 */
union i40e_16byte_rx_desc {
	struct {
		uint64_t pkt_addr; /* Packet buffer address */
		uint64_t hdr_addr; /* Header buffer address */
	} read;
	struct {
		struct {
			struct {
				union {
					uint16_t mirroring_status;
					uint16_t fcoe_ctx_id;
				} mirr_fcoe;
				uint16_t l2tag1;
			} lo_dword;
			union {
				uint32_t rss; /* RSS Hash */
				uint32_t fd_id; /* Flow director filter id */
				uint32_t fcoe_param; /* FCoE DDP Context id */
			} hi_dword;
		} qword0;
		struct {
			/* ext status/error/pktype/length */
			uint64_t status_error_len;
		} qword1;
	} wb;  /* writeback */
};

/*
 * See 8.4.2.1 - General Descriptors
 */
struct i40e_tx_desc {
	uint64_t buffer_addr; /* Address of descriptor's data buf */
	uint64_t cmd_type_offset_bsz;
};

#define I40E_RX_DESC(R, i)                      \
	(&(((union i40e_rx_desc *)((R)->addr_virt))[i]))
#define I40E_TX_DESC(R, i)                      \
	(&(((struct i40e_tx_desc *)((R)->addr_virt))[i]))

int i40e_vsi_update(struct ufp_dev *dev, struct ufp_iface *iface);
int i40e_vsi_rss_config(struct ufp_dev *dev, struct ufp_iface *iface);
int i40e_vsi_promisc_mode(struct ufp_dev *dev, struct ufp_iface *iface);
int i40e_vsi_configure_tx(struct ufp_dev *dev, struct ufp_iface *iface);
int i40e_vsi_configure_rx(struct ufp_dev *dev, struct ufp_iface *iface);
void i40e_vsi_configure_irq(struct ufp_dev *dev, struct ufp_iface *iface);
void i40e_vsi_shutdown_irq(struct ufp_dev *dev, struct ufp_iface *iface);
void i40e_vsi_start_irq(struct ufp_dev *dev, struct ufp_iface *iface);
void i40e_vsi_stop_irq(struct ufp_dev *dev, struct ufp_iface *iface);
void i40e_update_enable_itr(void *bar, uint16_t entry_idx);
int i40e_vsi_start_rx(struct ufp_dev *dev, struct ufp_iface *iface);
int i40e_vsi_stop_rx(struct ufp_dev *dev, struct ufp_iface *iface);
int i40e_vsi_start_tx(struct ufp_dev *dev, struct ufp_iface *iface);
int i40e_vsi_stop_tx(struct ufp_dev *dev, struct ufp_iface *iface);
int i40e_rx_desc_fetch(struct ufp_ring *rx_ring, uint16_t index,
	struct ufp_packet *packet);
int i40e_tx_desc_fetch(struct ufp_ring *tx_ring, uint16_t index);
void i40e_rx_desc_fill(struct ufp_ring *rx_ring, uint16_t index,
	uint64_t addr_dma);
void i40e_tx_desc_fill(struct ufp_ring *tx_ring, uint16_t index,
	uint64_t addr_dma, struct ufp_packet *packet);

#endif /* _I40E_IO_H__ */
