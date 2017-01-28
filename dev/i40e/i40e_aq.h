#ifndef _I40E_AQ_H__
#define _I40E_AQ_H__

/* general information */
#define I40E_AQ_LARGE_BUF       512
#define I40E_ASQ_CMD_TIMEOUT    250  /* msecs */

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
	uint32_t flag;
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
void i40e_aq_asq_assign(struct ufp_dev *dev, uint16_t opcode, uint16_t flags,
	void *cmd, uint16_t cmd_size, void *data, uint16_t data_size);
void i40e_aq_arq_assign(struct ufp_dev *dev);
void i40e_aq_asq_clean(struct ufp_dev *dev);
void i40e_aq_arq_clean(struct ufp_dev *dev);

#endif /* _I40E_AQ_H__ */
