struct i40e_aq_cmd_macaddr {
	__le16  command_flags;
#define I40E_AQC_LAN_ADDR_VALID		0x10
#define I40E_AQC_SAN_ADDR_VALID		0x20
#define I40E_AQC_PORT_ADDR_VALID	0x40
#define I40E_AQC_WOL_ADDR_VALID		0x80
#define I40E_AQC_MC_MAG_EN_VALID	0x100
#define I40E_AQC_ADDR_VALID_MASK	0x1F0
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

struct i40e_aqc_vsi_properties_data {
	/* first 96 byte are written by SW */
	__le16  valid_sections;
#define I40E_AQ_VSI_PROP_SWITCH_VALID	   0x0001
#define I40E_AQ_VSI_PROP_SECURITY_VALID	 0x0002
#define I40E_AQ_VSI_PROP_VLAN_VALID	     0x0004
#define I40E_AQ_VSI_PROP_CAS_PV_VALID	   0x0008
#define I40E_AQ_VSI_PROP_INGRESS_UP_VALID       0x0010
#define I40E_AQ_VSI_PROP_EGRESS_UP_VALID	0x0020
#define I40E_AQ_VSI_PROP_QUEUE_MAP_VALID	0x0040
#define I40E_AQ_VSI_PROP_QUEUE_OPT_VALID	0x0080
#define I40E_AQ_VSI_PROP_OUTER_UP_VALID	 0x0100
#define I40E_AQ_VSI_PROP_SCHED_VALID	    0x0200
	/* switch section */
	__le16  switch_id; /* 12bit id combined with flags below */
#define I40E_AQ_VSI_SW_ID_SHIFT	 0x0000
#define I40E_AQ_VSI_SW_ID_MASK	  (0xFFF << I40E_AQ_VSI_SW_ID_SHIFT)
#define I40E_AQ_VSI_SW_ID_FLAG_NOT_STAG 0x1000
#define I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB 0x2000
#define I40E_AQ_VSI_SW_ID_FLAG_LOCAL_LB 0x4000
	u8      sw_reserved[2];
	/* security section */
	u8      sec_flags;
#define I40E_AQ_VSI_SEC_FLAG_ALLOW_DEST_OVRD    0x01
#define I40E_AQ_VSI_SEC_FLAG_ENABLE_VLAN_CHK    0x02
#define I40E_AQ_VSI_SEC_FLAG_ENABLE_MAC_CHK     0x04
	u8      sec_reserved;
	/* VLAN section */
	__le16  pvid; /* VLANS include priority bits */
	__le16  fcoe_pvid;
	u8      port_vlan_flags;
#define I40E_AQ_VSI_PVLAN_MODE_SHIFT    0x00
#define I40E_AQ_VSI_PVLAN_MODE_MASK     (0x03 << \
					 I40E_AQ_VSI_PVLAN_MODE_SHIFT)
#define I40E_AQ_VSI_PVLAN_MODE_TAGGED   0x01
#define I40E_AQ_VSI_PVLAN_MODE_UNTAGGED 0x02
#define I40E_AQ_VSI_PVLAN_MODE_ALL      0x03
#define I40E_AQ_VSI_PVLAN_INSERT_PVID   0x04
#define I40E_AQ_VSI_PVLAN_EMOD_SHIFT    0x03
#define I40E_AQ_VSI_PVLAN_EMOD_MASK     (0x3 << \
					 I40E_AQ_VSI_PVLAN_EMOD_SHIFT)
#define I40E_AQ_VSI_PVLAN_EMOD_STR_BOTH 0x0
#define I40E_AQ_VSI_PVLAN_EMOD_STR_UP   0x08
#define I40E_AQ_VSI_PVLAN_EMOD_STR      0x10
#define I40E_AQ_VSI_PVLAN_EMOD_NOTHING  0x18
	u8      pvlan_reserved[3];
	/* ingress egress up sections */
	__le32  ingress_table; /* bitmap, 3 bits per up */
#define I40E_AQ_VSI_UP_TABLE_UP0_SHIFT  0
#define I40E_AQ_VSI_UP_TABLE_UP0_MASK   (0x7 << \
					 I40E_AQ_VSI_UP_TABLE_UP0_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP1_SHIFT  3
#define I40E_AQ_VSI_UP_TABLE_UP1_MASK   (0x7 << \
					 I40E_AQ_VSI_UP_TABLE_UP1_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP2_SHIFT  6
#define I40E_AQ_VSI_UP_TABLE_UP2_MASK   (0x7 << \
					 I40E_AQ_VSI_UP_TABLE_UP2_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP3_SHIFT  9
#define I40E_AQ_VSI_UP_TABLE_UP3_MASK   (0x7 << \
					 I40E_AQ_VSI_UP_TABLE_UP3_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP4_SHIFT  12
#define I40E_AQ_VSI_UP_TABLE_UP4_MASK   (0x7 << \
					 I40E_AQ_VSI_UP_TABLE_UP4_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP5_SHIFT  15
#define I40E_AQ_VSI_UP_TABLE_UP5_MASK   (0x7 << \
					 I40E_AQ_VSI_UP_TABLE_UP5_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP6_SHIFT  18
#define I40E_AQ_VSI_UP_TABLE_UP6_MASK   (0x7 << \
					 I40E_AQ_VSI_UP_TABLE_UP6_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP7_SHIFT  21
#define I40E_AQ_VSI_UP_TABLE_UP7_MASK   (0x7 << \
					 I40E_AQ_VSI_UP_TABLE_UP7_SHIFT)
	__le32  egress_table;   /* same defines as for ingress table */
	/* cascaded PV section */
	__le16  cas_pv_tag;
	u8      cas_pv_flags;
#define I40E_AQ_VSI_CAS_PV_TAGX_SHIFT	   0x00
#define I40E_AQ_VSI_CAS_PV_TAGX_MASK	    (0x03 << \
						 I40E_AQ_VSI_CAS_PV_TAGX_SHIFT)
#define I40E_AQ_VSI_CAS_PV_TAGX_LEAVE	   0x00
#define I40E_AQ_VSI_CAS_PV_TAGX_REMOVE	  0x01
#define I40E_AQ_VSI_CAS_PV_TAGX_COPY	    0x02
#define I40E_AQ_VSI_CAS_PV_INSERT_TAG	   0x10
#define I40E_AQ_VSI_CAS_PV_ETAG_PRUNE	   0x20
#define I40E_AQ_VSI_CAS_PV_ACCEPT_HOST_TAG      0x40
	u8      cas_pv_reserved;
	/* queue mapping section */
	__le16  mapping_flags;
#define I40E_AQ_VSI_QUE_MAP_CONTIG      0x0
#define I40E_AQ_VSI_QUE_MAP_NONCONTIG   0x1
	__le16  queue_mapping[16];
#define I40E_AQ_VSI_QUEUE_SHIFT	 0x0
#define I40E_AQ_VSI_QUEUE_MASK	  (0x7FF << I40E_AQ_VSI_QUEUE_SHIFT)
	__le16  tc_mapping[8];
#define I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT 0
#define I40E_AQ_VSI_TC_QUE_OFFSET_MASK  (0x1FF << \
					 I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT)
#define I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT 9
#define I40E_AQ_VSI_TC_QUE_NUMBER_MASK  (0x7 << \
					 I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT)
	/* queueing option section */
	u8      queueing_opt_flags;
#define I40E_AQ_VSI_QUE_OPT_MULTICAST_UDP_ENA   0x04
#define I40E_AQ_VSI_QUE_OPT_UNICAST_UDP_ENA     0x08
#define I40E_AQ_VSI_QUE_OPT_TCP_ENA     0x10
#define I40E_AQ_VSI_QUE_OPT_FCOE_ENA    0x20
#define I40E_AQ_VSI_QUE_OPT_RSS_LUT_PF  0x00
#define I40E_AQ_VSI_QUE_OPT_RSS_LUT_VSI 0x40
	u8      queueing_opt_reserved[3];
	/* scheduler section */
	u8      up_enable_bits;
	u8      sched_reserved;
	/* outer up section */
	__le32  outer_up_table; /* same structure and defines as ingress tbl */
	u8      cmd_reserved[8];
	/* last 32 bytes are written by FW */
	__le16  qs_handle[8];
#define I40E_AQ_VSI_QS_HANDLE_INVALID   0xFFFF
	__le16  stat_counter_idx;
	__le16  sched_id;
	u8      resp_reserved[12];
};
