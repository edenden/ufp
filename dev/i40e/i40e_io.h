#ifndef _I40E_IO_H__
#define _I40E_IO_H__

/* Supported Rx Buffer Sizes (a multiple of 128) */
#define I40E_RXBUFFER_256	256
#define I40E_RXBUFFER_2048	2048
#define I40E_RXBUFFER_3072	3072 /* For FCoE MTU of 2158 */
#define I40E_RXBUFFER_4096	4096
#define I40E_RXBUFFER_8192	8192
#define I40E_MAX_RXBUFFER	9728 /* largest size for single descriptor */

/* Interrupt Throttling and Rate Limiting Goodies */
#define I40E_MAX_ITR		0x0FF0	/* reg uses 2 usec resolution */
#define I40E_MIN_ITR		0x0001	/* reg uses 2 usec resolution */
#define I40E_ITR_100K		0x0005
#define I40E_ITR_50K		0x000A
#define I40E_ITR_20K		0x0019
#define I40E_ITR_18K		0x001B
#define I40E_ITR_8K		0x003E
#define I40E_ITR_4K		0x007A
#define I40E_MAX_INTRL		0x3B	/* reg uses 4 usec resolution */
#define I40E_ITR_RX_DEF		I40E_ITR_20K
#define I40E_ITR_TX_DEF		I40E_ITR_20K
#define I40E_ITR_DYNAMIC	0x8000	/* use top bit as a flag */
#define I40E_MIN_INT_RATE	250	/* ~= 1000000 / (I40E_MAX_ITR * 2) */
#define I40E_MAX_INT_RATE	500000	/* == 1000000 / (I40E_MIN_ITR * 2) */
#define I40E_DEFAULT_IRQ_WORK	256
#define ITR_TO_REG(setting) \
	((setting & ~I40E_ITR_DYNAMIC) >> 1)
#define ITR_IS_DYNAMIC(setting) \
	(!!(setting & I40E_ITR_DYNAMIC))
#define ITR_REG_TO_USEC(itr_reg) \
	(itr_reg << 1)
/* 0x40 is the enable bit for interrupt rate limiting, and must be set if
 * the value of the rate limit is non-zero
 */
#define INTRL_ENA 0x40
#define INTRL_REG_TO_USEC(intrl) \
	((intrl & ~INTRL_ENA) << 2)
#define INTRL_USEC_TO_REG(set) \
	((set) ? ((set) >> 2) | INTRL_ENA : 0)
#define I40E_INTRL_8K		125	/* 8000 ints/sec */
#define I40E_INTRL_62K		16	/* 62500 ints/sec */
#define I40E_INTRL_83K		12	/* 83333 ints/sec */

#define I40E_QUEUE_END_OF_LIST	0x7FF
#define I40E_QUEUE_WAIT_RETRY_LIMIT 10

/* this enum matches hardware bits and is meant to be used by DYN_CTLN
 * registers and QINT registers or more generally anywhere in the manual
 * mentioning ITR_INDX, ITR_NONE cannot be used as an index 'n' into any
 * register but instead is a special value meaning "don't update" ITR0/1/2.
 */
enum i40e_dyn_idx_t {
	I40E_IDX_ITR0 = 0,
	I40E_IDX_ITR1 = 1,
	I40E_IDX_ITR2 = 2,
	/* ITR_NONE must not be used as an index */
	I40E_ITR_NONE = 3
};

enum i40e_queue_type {
	I40E_QUEUE_TYPE_RX = 0,
	I40E_QUEUE_TYPE_TX,
	I40E_QUEUE_TYPE_PE_CEQ,
	I40E_QUEUE_TYPE_UNKNOWN
};

/*
 * See 8.3.2.1 - Receive Descriptor - Read Format
 */
struct i40e_16byte_rx_desc {
	uint64_t pkt_addr; /* Packet buffer address */
	uint64_t hdr_addr; /* Header buffer address */
};
/* writeback format of struct i40e_16byte_rx_desc */
struct i40e_16byte_rx_desc_wb {
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
};

#define i40e_rx_desc i40e_16byte_rx_desc
#define i40e_rx_desc_wb i40e_16byte_rx_desc_wb

enum i40e_rx_desc_status_bits {
	/* Note: These are predefined bit offsets */
	I40E_RX_DESC_STATUS_DD_SHIFT		= 0,
	I40E_RX_DESC_STATUS_EOF_SHIFT		= 1,
	I40E_RX_DESC_STATUS_L2TAG1P_SHIFT	= 2,
	I40E_RX_DESC_STATUS_L3L4P_SHIFT		= 3,
	I40E_RX_DESC_STATUS_CRCP_SHIFT		= 4,
	I40E_RX_DESC_STATUS_TSYNINDX_SHIFT	= 5, /* 2 BITS */
	I40E_RX_DESC_STATUS_TSYNVALID_SHIFT	= 7,
	I40E_RX_DESC_STATUS_EXT_UDP_0_SHIFT	= 8,

	I40E_RX_DESC_STATUS_UMBCAST_SHIFT	= 9, /* 2 BITS */
	I40E_RX_DESC_STATUS_FLM_SHIFT		= 11,
	I40E_RX_DESC_STATUS_FLTSTAT_SHIFT	= 12, /* 2 BITS */
	I40E_RX_DESC_STATUS_LPBK_SHIFT		= 14,
	I40E_RX_DESC_STATUS_IPV6EXADD_SHIFT	= 15,
	I40E_RX_DESC_STATUS_RESERVED2_SHIFT	= 16, /* 2 BITS */
	I40E_RX_DESC_STATUS_INT_UDP_0_SHIFT	= 18,
	I40E_RX_DESC_STATUS_LAST /* this entry must be last!!! */
};

#define I40E_RXD_QW1_ERROR_SHIFT 19
#define I40E_RXD_QW1_ERROR_MASK \
	(0xFFUL << I40E_RXD_QW1_ERROR_SHIFT)
#define I40E_RXD_QW1_LENGTH_PBUF_SHIFT 38
#define I40E_RXD_QW1_LENGTH_PBUF_MASK \
	(0x3FFFULL << I40E_RXD_QW1_LENGTH_PBUF_SHIFT)

/*
 * See 8.4.2.1 - General Descriptors
 */
struct i40e_tx_desc {
	uint64_t buffer_addr; /* Address of descriptor's data buf */
	uint64_t cmd_type_offset_bsz;
};

enum i40e_tx_desc_dtype_value {
	I40E_TX_DESC_DTYPE_DATA			= 0x0,
	/* same as Context desc */
	I40E_TX_DESC_DTYPE_NOP			= 0x1,
	I40E_TX_DESC_DTYPE_CONTEXT		= 0x1,
	I40E_TX_DESC_DTYPE_FCOE_CTX		= 0x2,
	I40E_TX_DESC_DTYPE_FILTER_PROG		= 0x8,
	I40E_TX_DESC_DTYPE_DDP_CTX		= 0x9,
	I40E_TX_DESC_DTYPE_FLEX_DATA		= 0xB,
	I40E_TX_DESC_DTYPE_FLEX_CTX_1		= 0xC,
	I40E_TX_DESC_DTYPE_FLEX_CTX_2		= 0xD,
	I40E_TX_DESC_DTYPE_DESC_DONE		= 0xF
};

enum i40e_tx_desc_cmd_bits {
	I40E_TX_DESC_CMD_EOP			= 0x0001,
	I40E_TX_DESC_CMD_RS			= 0x0002,
	I40E_TX_DESC_CMD_ICRC			= 0x0004,
	I40E_TX_DESC_CMD_IL2TAG1		= 0x0008,
	I40E_TX_DESC_CMD_DUMMY			= 0x0010,
	I40E_TX_DESC_CMD_IIPT_NONIP		= 0x0000, /* 2 BITS */
	I40E_TX_DESC_CMD_IIPT_IPV6		= 0x0020, /* 2 BITS */
	I40E_TX_DESC_CMD_IIPT_IPV4		= 0x0040, /* 2 BITS */
	I40E_TX_DESC_CMD_IIPT_IPV4_CSUM		= 0x0060, /* 2 BITS */
	I40E_TX_DESC_CMD_FCOET			= 0x0080,
	I40E_TX_DESC_CMD_L4T_EOFT_UNK		= 0x0000, /* 2 BITS */
	I40E_TX_DESC_CMD_L4T_EOFT_TCP		= 0x0100, /* 2 BITS */
	I40E_TX_DESC_CMD_L4T_EOFT_SCTP		= 0x0200, /* 2 BITS */
	I40E_TX_DESC_CMD_L4T_EOFT_UDP		= 0x0300, /* 2 BITS */
	I40E_TX_DESC_CMD_L4T_EOFT_EOF_N		= 0x0000, /* 2 BITS */
	I40E_TX_DESC_CMD_L4T_EOFT_EOF_T		= 0x0100, /* 2 BITS */
	I40E_TX_DESC_CMD_L4T_EOFT_EOF_NI	= 0x0200, /* 2 BITS */
	I40E_TX_DESC_CMD_L4T_EOFT_EOF_A		= 0x0300, /* 2 BITS */
};

#define I40E_TXD_QW1_DTYPE_SHIFT 0
#define I40E_TXD_QW1_DTYPE_MASK \
	(0xFUL << I40E_TXD_QW1_DTYPE_SHIFT)
#define I40E_TXD_QW1_CMD_SHIFT 4
#define I40E_TXD_QW1_CMD_MASK \
	(0x3FFUL << I40E_TXD_QW1_CMD_SHIFT)
#define I40E_TXD_QW1_OFFSET_SHIFT 16
#define I40E_TXD_QW1_OFFSET_MASK \
	(0x3FFFFULL << I40E_TXD_QW1_OFFSET_SHIFT)
#define I40E_TXD_QW1_TX_BUF_SZ_SHIFT 34
#define I40E_TXD_QW1_TX_BUF_SZ_MASK \
	(0x3FFFULL << I40E_TXD_QW1_TX_BUF_SZ_SHIFT)
#define I40E_TXD_QW1_L2TAG1_SHIFT 48
#define I40E_TXD_QW1_L2TAG1_MASK \
	(0xFFFFULL << I40E_TXD_QW1_L2TAG1_SHIFT)

#define I40E_RX_DESC(R, i)                      \
	(&(((struct i40e_rx_desc *)((R)->addr_virt))[i]))
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
