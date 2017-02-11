#ifndef _I40E_AQ_H__
#define _I40E_AQ_H__

/* general information */
#define I40E_AQ_LARGE_BUF		512
#define I40E_ASQ_CMD_TIMEOUT		250  /* msecs */
#define I40E_ADMINQ_DESC_ALIGNMENT	4096
#define I40E_MAX_AQ_BUF_SIZE		4096
#define I40E_AQ_LEN			256

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
	struct i40e_page *desc; /* descriptor ring memory */
	struct i40e_page **bufs;
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

enum i40e_admin_queue_opc {
	/* aq commands */
	i40e_aqc_opc_get_version	= 0x0001,
	i40e_aqc_opc_driver_version     = 0x0002,
	i40e_aqc_opc_queue_shutdown     = 0x0003,
	i40e_aqc_opc_set_pf_context     = 0x0004,

	/* resource ownership */
	i40e_aqc_opc_request_resource   = 0x0008,
	i40e_aqc_opc_release_resource   = 0x0009,

	i40e_aqc_opc_list_func_capabilities     = 0x000A,
	i40e_aqc_opc_list_dev_capabilities      = 0x000B,

	/* Proxy commands */
	i40e_aqc_opc_set_proxy_config	   = 0x0104,
	i40e_aqc_opc_set_ns_proxy_table_entry   = 0x0105,

	/* LAA */
	i40e_aqc_opc_mac_address_read   = 0x0107,
	i40e_aqc_opc_mac_address_write  = 0x0108,

	/* PXE */
	i40e_aqc_opc_clear_pxe_mode     = 0x0110,

	/* WoL commands */
	i40e_aqc_opc_set_wol_filter     = 0x0120,
	i40e_aqc_opc_get_wake_reason    = 0x0121,
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
