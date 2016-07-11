#ifndef _IXGBEVF_MAC_H__
#define _IXGBEVF_MAC_H__

void ufp_ixgbevf_clr_reg(struct ufp_handle *ih);
void ufp_ixgbevf_set_eitr(struct ufp_handle *ih, int vector);
void ufp_ixgbevf_set_ivar(struct ufp_handle *ih,
	int8_t direction, uint8_t queue, uint8_t msix_vector);
int ufp_ixgbevf_update_xcast_mode(struct ufp_handle *ih, int xcast_mode);
void ufp_ixgbevf_set_psrtype(struct ufp_handle *ih);
void ufp_ixgbevf_set_vfmrqc(struct ufp_handle *ih);
int ufp_ixgbevf_set_rlpml(struct ufp_handle *ih);
void ixgbevf_configure_tx_ring(struct ufp_handle *ih,
	uint8_t reg_idx, struct ufp_ring *ring);
static void ufp_ixgbevf_disable_rx_queue(struct ufp_handle *ih, uint8_t reg_idx);
static void ufp_ixgbevf_configure_srrctl(struct ufp_handle *ih, uint8_t reg_idx);
static void ufp_ixgbevf_rx_desc_queue_enable(struct ufp_handle *ih,
	uint8_t reg_idx);
void ufp_ixgbevf_configure_rx_ring(struct ufp_handle *ih,
	uint8_t reg_idx, struct ufp_ring *ring);

#endif /* _IXGBEVF_MAC_H__ */
