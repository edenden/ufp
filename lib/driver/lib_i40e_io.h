#ifndef _I40E_IO_H__
#define _I40E_IO_H__

/*
 * See 8.3.2.1 - Receive Descriptor - Read Format
 */
union i40e_16byte_rx_desc {
	struct {
		__le64 pkt_addr; /* Packet buffer address */
		__le64 hdr_addr; /* Header buffer address */
	} read;
	struct {
		struct {
			struct {
				union {
					__le16 mirroring_status;
					__le16 fcoe_ctx_id;
				} mirr_fcoe;
				__le16 l2tag1;
			} lo_dword;
			union {
				__le32 rss; /* RSS Hash */
				__le32 fd_id; /* Flow director filter id */
				__le32 fcoe_param; /* FCoE DDP Context id */
			} hi_dword;
		} qword0;
		struct {
			/* ext status/error/pktype/length */
			__le64 status_error_len;
		} qword1;
	} wb;  /* writeback */
};

#define I40E_RX_DESC(R, i)                      \
	(&(((union i40e_rx_desc *)((R)->addr_virt))[i]))
#define I40E_TX_DESC(R, i)                      \
	(&(((struct i40e_tx_desc *)((R)->addr_virt))[i]))

#endif /* _I40E_IO_H__ */
