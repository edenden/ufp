#ifndef _I40E_AQ_H__
#define _I40E_AQ_H__

/* general information */
#define I40E_AQ_LARGE_BUF		512
#define I40E_ASQ_CMD_TIMEOUT		250  /* msecs */
#define I40E_ADMINQ_DESC_ALIGNMENT	4096
#define I40E_MAX_AQ_BUF_SIZE		4096
#define I40E_AQ_LEN			128

/* command flags and offsets*/
#define I40E_AQ_FLAG_DD_SHIFT	0
#define I40E_AQ_FLAG_CMP_SHIFT	1
#define I40E_AQ_FLAG_ERR_SHIFT	2
#define I40E_AQ_FLAG_VFE_SHIFT	3
#define I40E_AQ_FLAG_LB_SHIFT	9
#define I40E_AQ_FLAG_RD_SHIFT	10
#define I40E_AQ_FLAG_VFC_SHIFT	11
#define I40E_AQ_FLAG_BUF_SHIFT	12
#define I40E_AQ_FLAG_SI_SHIFT	13
#define I40E_AQ_FLAG_EI_SHIFT	14
#define I40E_AQ_FLAG_FE_SHIFT	15

#define I40E_AQ_FLAG_DD		(1 << I40E_AQ_FLAG_DD_SHIFT)  /* 0x1    */
#define I40E_AQ_FLAG_CMP	(1 << I40E_AQ_FLAG_CMP_SHIFT) /* 0x2    */
#define I40E_AQ_FLAG_ERR	(1 << I40E_AQ_FLAG_ERR_SHIFT) /* 0x4    */
#define I40E_AQ_FLAG_VFE	(1 << I40E_AQ_FLAG_VFE_SHIFT) /* 0x8    */
#define I40E_AQ_FLAG_LB		(1 << I40E_AQ_FLAG_LB_SHIFT)  /* 0x200  */
#define I40E_AQ_FLAG_RD		(1 << I40E_AQ_FLAG_RD_SHIFT)  /* 0x400  */
#define I40E_AQ_FLAG_VFC	(1 << I40E_AQ_FLAG_VFC_SHIFT) /* 0x800  */
#define I40E_AQ_FLAG_BUF	(1 << I40E_AQ_FLAG_BUF_SHIFT) /* 0x1000 */
#define I40E_AQ_FLAG_SI		(1 << I40E_AQ_FLAG_SI_SHIFT)  /* 0x2000 */
#define I40E_AQ_FLAG_EI		(1 << I40E_AQ_FLAG_EI_SHIFT)  /* 0x4000 */
#define I40E_AQ_FLAG_FE		(1 << I40E_AQ_FLAG_FE_SHIFT)  /* 0x8000 */

struct i40e_aq_desc {
	uint16_t flags;
	uint16_t opcode;
	uint16_t datalen;
	uint16_t retval;
	uint32_t cookie_high;
	uint32_t cookie_low;
	union {
		struct {
			uint32_t param0;
			uint32_t param1;
			uint32_t param2;
			uint32_t param3;
		} internal;
		struct {
			uint32_t param0;
			uint32_t param1;
			uint32_t addr_high;
			uint32_t addr_low;
		} external;
		uint8_t raw[16];
	} params;
};

struct i40e_aq_ring {
	struct ufp_dma_buf *desc; /* descriptor ring memory */
	struct ufp_dma_buf **bufs;
	uint16_t num_desc;

	uint16_t next_to_use;
	uint16_t next_to_clean;

	/* used for queue tracking */
	uint32_t head;
	uint32_t tail;
	uint32_t len;
	uint32_t bah;
	uint32_t bal;
};

struct i40e_aq {
	struct i40e_aq_ring *tx_ring;
	struct i40e_aq_ring *rx_ring;
	struct list_head session;
};

struct i40e_aq_session {
	struct list_node list;
	int retval;
	union {
		uint16_t seid_offset;
		uint32_t read_val;
	} data;
};

int i40e_aq_init(struct ufp_dev *dev);
void i40e_aq_destroy(struct ufp_dev *dev);
struct i40e_aq_session *i40e_aq_session_create(struct ufp_dev *dev);
void i40e_aq_session_delete(struct i40e_aq_session *session);
void i40e_aq_asq_assign(struct ufp_dev *dev, uint16_t opcode, uint16_t flags,
	void *cmd, uint16_t cmd_size, void *data, uint16_t data_size,
	uint64_t cookie);
void i40e_aq_arq_assign(struct ufp_dev *dev);
void i40e_aq_asq_clean(struct ufp_dev *dev);
void i40e_aq_arq_clean(struct ufp_dev *dev);

#endif /* _I40E_AQ_H__ */
