/* general information */
#define I40E_AQ_LARGE_BUF       512
#define I40E_ASQ_CMD_TIMEOUT    250  /* msecs */

struct i40e_aq_desc {
	__le16 flags;
	__le16 opcode;
	__le16 datalen;
	__le16 retval;
	__le32 cookie_high;
	__le32 cookie_low;
	union { 
		struct {
			__le32 param0;
			__le32 param1;
			__le32 param2;
			__le32 param3;
		} internal;
		struct {
			__le32 param0;
			__le32 param1;
			__le32 addr_high;
			__le32 addr_low;
		} external;
		u8 raw[16];
	} params;
};

struct i40e_aq_ring {
	struct i40e_page *desc; /* descriptor ring memory */
	struct i40e_page **bufs;
	uint16_t num_desc;

	uint16_t next_to_use;
	uint16_t next_to_clean;

	/* used for queue tracking */
	u32 head;
	u32 tail;
	u32 len;
	u32 bah;
	u32 bal;
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

#endif /* _I40E_ADMINQ_H_ */
