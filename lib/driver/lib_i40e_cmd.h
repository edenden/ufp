struct i40e_aq_cmd_macaddr {
        __le16  command_flags;
#define I40E_AQC_LAN_ADDR_VALID         0x10
#define I40E_AQC_SAN_ADDR_VALID         0x20
#define I40E_AQC_PORT_ADDR_VALID        0x40
#define I40E_AQC_WOL_ADDR_VALID         0x80
#define I40E_AQC_MC_MAG_EN_VALID        0x100
#define I40E_AQC_ADDR_VALID_MASK        0x1F0
        u8      reserved[6];
        __le32  addr_high;
        __le32  addr_low;
};

struct i40e_aq_buf_mac_addr {
        u8 pf_lan_mac[6];
        u8 pf_san_mac[6];
        u8 port_mac[6];
        u8 pf_wol_mac[6];
};

struct i40e_aq_cmd_clear_pxe {
        u8      rx_cnt;
        u8      reserved[15];
};

struct i40e_aq_cmd_getconf {
	__le16  seid_offset;
	u8      reserved[6];
	__le32  addr_high;
	__le32  addr_low;
};

