#ifndef _IXGBEVF_MAC_H__
#define _IXGBEVF_MAC_H__

void ufp_ixgbevf_clr_reg(struct ufp_handle *ih);
int ufp_ixgbevf_negotiate_api(struct ufp_handle *ih);
int ufp_ixgbevf_get_queues(struct ufp_handle *ih, uint32_t num_queues_req);
void ufp_ixgbevf_set_eitr(struct ufp_handle *ih, int vector, uint32_t rate);
void ufp_ixgbevf_set_ivar(struct ufp_handle *ih,
	int8_t direction, uint8_t queue, uint8_t msix_vector);
int ufp_ixgbevf_update_xcast_mode(struct ufp_handle *ih, int xcast_mode);
void ufp_ixgbevf_set_psrtype(struct ufp_handle *ih);
void ufp_ixgbevf_set_vfmrqc(struct ufp_handle *ih);
int ufp_ixgbevf_set_rlpml(struct ufp_handle *ih, uint32_t mtu_frame);
void ufp_ixgbevf_configure_tx_ring(struct ufp_handle *ih,
	uint8_t reg_idx, struct ufp_ring *ring);
void ufp_ixgbevf_configure_rx_ring(struct ufp_handle *ih,
	uint8_t reg_idx, struct ufp_ring *ring);

#endif /* _IXGBEVF_MAC_H__ */
