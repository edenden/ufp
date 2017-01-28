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

#endif /* _I40E_IO_H__ */
