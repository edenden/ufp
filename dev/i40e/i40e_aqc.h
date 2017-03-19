#ifndef _I40E_AQC_H__
#define _I40E_AQC_H__

enum i40e_admin_queue_opc {
	/* aq commands */
	i40e_aq_opc_firmware_version	= 0x0001,
	i40e_aq_opc_driver_version	= 0x0002,
	i40e_aq_opc_queue_shutdown	= 0x0003,

	/* LAA */
	i40e_aq_opc_macaddr_read	= 0x0107,

	/* PXE */
	i40e_aq_opc_clear_pxemode	= 0x0110,

	/* internal switch commands */
	i40e_aq_opc_get_swconf		= 0x0200,
	i40e_aq_opc_set_swconf		= 0x0205,
	i40e_aq_opc_rxctl_read		= 0x0206,
	i40e_aq_opc_rxctl_write		= 0x0207,
	i40e_aq_opc_update_vsi		= 0x0211,
	i40e_aq_opc_get_vsi		= 0x0212,
	i40e_aq_opc_promisc_mode	= 0x0254,

	/* phy commands */
	i40e_aq_opc_set_phyintmask	= 0x0613,

	/* LLDP commands */
	i40e_aq_opc_stop_lldp		= 0x0A05,

	/* Tunnel commands */
	i40e_aq_opc_set_rsskey		= 0x0B02,
	i40e_aq_opc_set_rsslut		= 0x0B03,
};

struct i40e_aq_cmd_firmware_version {
	uint32_t	rom_ver;
	uint32_t	fw_build;
	uint16_t	fw_major;
	uint16_t	fw_minor;
	uint16_t	api_major;
	uint16_t	api_minor;
};

struct i40e_aq_cmd_driver_version {
	uint8_t		driver_major_ver;
	uint8_t		driver_minor_ver;
	uint8_t		driver_build_ver;
	uint8_t		driver_subbuild_ver;
	uint8_t		reserved[4];
	uint32_t	address_high;
	uint32_t	address_low;
};

struct i40e_aq_cmd_queue_shutdown {
	uint32_t	driver_unloading;
#define I40E_AQ_DRIVER_UNLOADING	0x1
	uint8_t		reserved[12];
};

struct i40e_aq_cmd_macaddr_read {
	uint16_t	command_flags;
#define I40E_AQC_LAN_ADDR_VALID		0x10
#define I40E_AQC_SAN_ADDR_VALID		0x20
#define I40E_AQC_PORT_ADDR_VALID	0x40
#define I40E_AQC_WOL_ADDR_VALID		0x80
#define I40E_AQC_MC_MAG_EN_VALID	0x100
#define I40E_AQC_ADDR_VALID_MASK	0x1F0
	uint8_t		reserved[6];
	uint32_t	addr_high;
	uint32_t	addr_low;
};

struct i40e_aq_buf_macaddr_read {
	uint8_t		pf_lan_mac[6];
	uint8_t		pf_san_mac[6];
	uint8_t		port_mac[6];
	uint8_t		pf_wol_mac[6];
};

struct i40e_aq_cmd_clear_pxemode {
	uint8_t		rx_cnt;
	uint8_t		reserved[15];
};

struct i40e_aq_cmd_get_swconf {
	uint16_t	seid_offset;
	uint8_t		reserved[6];
	uint32_t	addr_high;
	uint32_t	addr_low;
};

struct i40e_aq_buf_get_swconf_header {
	uint16_t	num_reported;
	uint16_t	num_total;
	uint8_t		reserved[12];
};

struct i40e_aq_buf_get_swconf_elem {
	uint8_t		element_type;
#define I40E_AQ_SW_ELEM_TYPE_MAC	1
#define I40E_AQ_SW_ELEM_TYPE_PF		2
#define I40E_AQ_SW_ELEM_TYPE_VF		3
#define I40E_AQ_SW_ELEM_TYPE_EMP	4
#define I40E_AQ_SW_ELEM_TYPE_BMC	5
#define I40E_AQ_SW_ELEM_TYPE_PV		16
#define I40E_AQ_SW_ELEM_TYPE_VEB	17
#define I40E_AQ_SW_ELEM_TYPE_PA		18
#define I40E_AQ_SW_ELEM_TYPE_VSI	19
	uint8_t		revision;
#define I40E_AQ_SW_ELEM_REV_1		1
	uint16_t	seid;
	uint16_t	uplink_seid;
	uint16_t	downlink_seid;
	uint8_t		reserved[3];
	uint8_t		connection_type;
#define I40E_AQ_CONN_TYPE_REGULAR	0x1
#define I40E_AQ_CONN_TYPE_DEFAULT	0x2
#define I40E_AQ_CONN_TYPE_CASCADED	0x3
	uint16_t	scheduler_id;
	uint16_t	element_info;
};

struct i40e_aq_cmd_set_swconf {
	uint16_t	flags;
#define I40E_AQ_SET_SWITCH_CFG_PROMISC	0x0001
#define I40E_AQ_SET_SWITCH_CFG_L2_FILTER 0x0002
	uint16_t	valid_flags;
	uint8_t		reserved[12];
};

#define i40e_aq_cmd_rxctl_read i40e_aq_cmd_rxctl_write
struct i40e_aq_cmd_rxctl_write {
	uint32_t reserved1;
	uint32_t address;
	uint32_t reserved2;
	uint32_t value;
};

#define i40e_aq_cmd_get_vsi i40e_aq_cmd_update_vsi
struct i40e_aq_cmd_update_vsi {
	uint16_t	uplink_seid;
	uint8_t		connection_type;
#define I40E_AQ_VSI_CONN_TYPE_NORMAL	0x1
#define I40E_AQ_VSI_CONN_TYPE_DEFAULT	0x2
#define I40E_AQ_VSI_CONN_TYPE_CASCADED	0x3
	uint8_t		reserved1;
	uint8_t		vf_id;
	uint8_t		reserved2;
	uint16_t	vsi_flags;
#define I40E_AQ_VSI_TYPE_SHIFT		0x0
#define I40E_AQ_VSI_TYPE_MASK		(0x3 << I40E_AQ_VSI_TYPE_SHIFT)
#define I40E_AQ_VSI_TYPE_VF		0x0
#define I40E_AQ_VSI_TYPE_VMDQ2		0x1
#define I40E_AQ_VSI_TYPE_PF		0x2
#define I40E_AQ_VSI_TYPE_EMP_MNG	0x3
#define I40E_AQ_VSI_FLAG_CASCADED_PV	0x4
	uint32_t	addr_high;
	uint32_t	addr_low;
};

#define i40e_aq_cmd_get_vsi_resp i40e_aq_cmd_update_vsi_resp
struct i40e_aq_cmd_update_vsi_resp {
	uint16_t	seid;
	uint16_t	vsi_number;
	uint16_t	vsi_used;
	uint16_t	vsi_free;
	uint32_t	addr_high;
	uint32_t	addr_low;
};

struct i40e_aq_buf_vsi_data {
	/* first 96 byte are written by SW */
	uint16_t	valid_sections;
#define I40E_AQ_VSI_PROP_SWITCH_VALID	0x0001
#define I40E_AQ_VSI_PROP_SECURITY_VALID	0x0002
#define I40E_AQ_VSI_PROP_VLAN_VALID	0x0004
#define I40E_AQ_VSI_PROP_CAS_PV_VALID	0x0008
#define I40E_AQ_VSI_PROP_INGRESS_UP_VALID 0x0010
#define I40E_AQ_VSI_PROP_EGRESS_UP_VALID 0x0020
#define I40E_AQ_VSI_PROP_QUEUE_MAP_VALID 0x0040
#define I40E_AQ_VSI_PROP_QUEUE_OPT_VALID 0x0080
#define I40E_AQ_VSI_PROP_OUTER_UP_VALID	0x0100
#define I40E_AQ_VSI_PROP_SCHED_VALID	0x0200
	/* switch section */
	uint16_t	switch_id; /* 12bit id combined with flags below */
#define I40E_AQ_VSI_SW_ID_SHIFT		0x0000
#define I40E_AQ_VSI_SW_ID_MASK		(0xFFF << I40E_AQ_VSI_SW_ID_SHIFT)
#define I40E_AQ_VSI_SW_ID_FLAG_NOT_STAG	0x1000
#define I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB	0x2000
#define I40E_AQ_VSI_SW_ID_FLAG_LOCAL_LB	0x4000
	uint8_t		sw_reserved[2];
	/* security section */
	uint8_t		sec_flags;
#define I40E_AQ_VSI_SEC_FLAG_ALLOW_DEST_OVRD 0x01
#define I40E_AQ_VSI_SEC_FLAG_ENABLE_VLAN_CHK 0x02
#define I40E_AQ_VSI_SEC_FLAG_ENABLE_MAC_CHK 0x04
	uint8_t		sec_reserved;
	/* VLAN section */
	uint16_t	pvid; /* VLANS include priority bits */
	uint16_t	fcoe_pvid;
	uint8_t		port_vlan_flags;
#define I40E_AQ_VSI_PVLAN_MODE_SHIFT	0x00
#define I40E_AQ_VSI_PVLAN_MODE_MASK	(0x03 << I40E_AQ_VSI_PVLAN_MODE_SHIFT)
#define I40E_AQ_VSI_PVLAN_MODE_TAGGED	0x01
#define I40E_AQ_VSI_PVLAN_MODE_UNTAGGED	0x02
#define I40E_AQ_VSI_PVLAN_MODE_ALL	0x03
#define I40E_AQ_VSI_PVLAN_INSERT_PVID	0x04
#define I40E_AQ_VSI_PVLAN_EMOD_SHIFT	0x03
#define I40E_AQ_VSI_PVLAN_EMOD_MASK	(0x3 << I40E_AQ_VSI_PVLAN_EMOD_SHIFT)
#define I40E_AQ_VSI_PVLAN_EMOD_STR_BOTH	0x0
#define I40E_AQ_VSI_PVLAN_EMOD_STR_UP	0x08
#define I40E_AQ_VSI_PVLAN_EMOD_STR	0x10
#define I40E_AQ_VSI_PVLAN_EMOD_NOTHING	0x18
	uint8_t		pvlan_reserved[3];
	/* ingress egress up sections */
	uint32_t	ingress_table; /* bitmap, 3 bits per up */
#define I40E_AQ_VSI_UP_TABLE_UP0_SHIFT	0
#define I40E_AQ_VSI_UP_TABLE_UP0_MASK	(0x7 << I40E_AQ_VSI_UP_TABLE_UP0_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP1_SHIFT	3
#define I40E_AQ_VSI_UP_TABLE_UP1_MASK	(0x7 << I40E_AQ_VSI_UP_TABLE_UP1_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP2_SHIFT	6
#define I40E_AQ_VSI_UP_TABLE_UP2_MASK	(0x7 << I40E_AQ_VSI_UP_TABLE_UP2_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP3_SHIFT	9
#define I40E_AQ_VSI_UP_TABLE_UP3_MASK	(0x7 << I40E_AQ_VSI_UP_TABLE_UP3_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP4_SHIFT	12
#define I40E_AQ_VSI_UP_TABLE_UP4_MASK	(0x7 << I40E_AQ_VSI_UP_TABLE_UP4_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP5_SHIFT	15
#define I40E_AQ_VSI_UP_TABLE_UP5_MASK	(0x7 << I40E_AQ_VSI_UP_TABLE_UP5_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP6_SHIFT	18
#define I40E_AQ_VSI_UP_TABLE_UP6_MASK	(0x7 << I40E_AQ_VSI_UP_TABLE_UP6_SHIFT)
#define I40E_AQ_VSI_UP_TABLE_UP7_SHIFT	21
#define I40E_AQ_VSI_UP_TABLE_UP7_MASK	(0x7 << I40E_AQ_VSI_UP_TABLE_UP7_SHIFT)
	uint32_t	egress_table; /* same defines as for ingress table */
	/* cascaded PV section */
	uint16_t	cas_pv_tag;
	uint8_t		cas_pv_flags;
#define I40E_AQ_VSI_CAS_PV_TAGX_SHIFT	0x00
#define I40E_AQ_VSI_CAS_PV_TAGX_MASK	(0x03 << I40E_AQ_VSI_CAS_PV_TAGX_SHIFT)
#define I40E_AQ_VSI_CAS_PV_TAGX_LEAVE	0x00
#define I40E_AQ_VSI_CAS_PV_TAGX_REMOVE	0x01
#define I40E_AQ_VSI_CAS_PV_TAGX_COPY	0x02
#define I40E_AQ_VSI_CAS_PV_INSERT_TAG	0x10
#define I40E_AQ_VSI_CAS_PV_ETAG_PRUNE	0x20
#define I40E_AQ_VSI_CAS_PV_ACCEPT_HOST_TAG 0x40
	uint8_t		cas_pv_reserved;
	/* queue mapping section */
	uint16_t	mapping_flags;
#define I40E_AQ_VSI_QUE_MAP_CONTIG	0x0
#define I40E_AQ_VSI_QUE_MAP_NONCONTIG	0x1
	uint16_t	queue_mapping[16];
#define I40E_AQ_VSI_QUEUE_SHIFT		0x0
#define I40E_AQ_VSI_QUEUE_MASK		(0x7FF << I40E_AQ_VSI_QUEUE_SHIFT)
	uint16_t	tc_mapping[8];
#define I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT	0
#define I40E_AQ_VSI_TC_QUE_OFFSET_MASK	(0x1FF << I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT)
#define I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT	9
#define I40E_AQ_VSI_TC_QUE_NUMBER_MASK	(0x7 << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT)
	/* queueing option section */
	uint8_t		queueing_opt_flags;
#define I40E_AQ_VSI_QUE_OPT_MULTICAST_UDP_ENA 0x04
#define I40E_AQ_VSI_QUE_OPT_UNICAST_UDP_ENA 0x08
#define I40E_AQ_VSI_QUE_OPT_TCP_ENA	0x10
#define I40E_AQ_VSI_QUE_OPT_FCOE_ENA	0x20
#define I40E_AQ_VSI_QUE_OPT_RSS_LUT_PF	0x00
#define I40E_AQ_VSI_QUE_OPT_RSS_LUT_VSI	0x40
	uint8_t		queueing_opt_reserved[3];
	/* scheduler section */
	uint8_t		up_enable_bits;
	uint8_t		sched_reserved;
	/* outer up section */
	uint32_t	outer_up_table; /* same structure and defines as ingress tbl */
	uint8_t		cmd_reserved[8];
	/* last 32 bytes are written by FW */
	uint16_t	qs_handle[8];
#define I40E_AQ_VSI_QS_HANDLE_INVALID   0xFFFF
	uint16_t	stat_counter_idx;
	uint16_t	sched_id;
	uint8_t		resp_reserved[12];
};

struct i40e_aq_cmd_promisc_mode {
	uint16_t	promiscuous_flags;
	uint16_t	valid_flags;
/* flags used for both fields above */
#define I40E_AQC_SET_VSI_PROMISC_UNICAST	0x01
#define I40E_AQC_SET_VSI_PROMISC_MULTICAST	0x02
#define I40E_AQC_SET_VSI_PROMISC_BROADCAST	0x04
#define I40E_AQC_SET_VSI_DEFAULT		0x08
#define I40E_AQC_SET_VSI_PROMISC_VLAN		0x10
#define I40E_AQC_SET_VSI_PROMISC_TX		0x8000
	uint16_t	seid;
#define I40E_AQC_VSI_PROM_CMD_SEID_MASK		0x3FF
	uint16_t	vlan_tag;
#define I40E_AQC_SET_VSI_VLAN_MASK		0x0FFF
#define I40E_AQC_SET_VSI_VLAN_VALID		0x8000
	uint8_t		reserved[8];
};

struct i40e_aq_cmd_set_phyintmask {
	uint8_t		reserved[8];
	uint16_t	event_mask;
#define I40E_AQ_EVENT_LINK_UPDOWN		0x0002
#define I40E_AQ_EVENT_MEDIA_NA			0x0004
#define I40E_AQ_EVENT_LINK_FAULT		0x0008
#define I40E_AQ_EVENT_PHY_TEMP_ALARM		0x0010
#define I40E_AQ_EVENT_EXCESSIVE_ERRORS		0x0020
#define I40E_AQ_EVENT_SIGNAL_DETECT		0x0040
#define I40E_AQ_EVENT_AN_COMPLETED		0x0080
#define I40E_AQ_EVENT_MODULE_QUAL_FAIL		0x0100
#define I40E_AQ_EVENT_PORT_TX_SUSPENDED		0x0200
	uint8_t		reserved1[6];
};

struct i40e_aq_cmd_stop_lldp {
	uint8_t		command;
#define I40E_AQ_LLDP_AGENT_STOP			0x0
#define I40E_AQ_LLDP_AGENT_SHUTDOWN		0x1
	uint8_t		reserved[15];
};

struct i40e_aq_cmd_set_rsskey {
#define I40E_AQC_SET_RSS_KEY_VSI_VALID		(0x1 << 15)
#define I40E_AQC_SET_RSS_KEY_VSI_ID_SHIFT	0
#define I40E_AQC_SET_RSS_KEY_VSI_ID_MASK	(0x3FF << I40E_AQC_SET_RSS_KEY_VSI_ID_SHIFT)
	uint16_t	vsi_id;
	uint8_t		reserved[6];
	uint32_t	addr_high;
	uint32_t	addr_low;
};

struct  i40e_aq_cmd_set_rsslut {
#define I40E_AQC_SET_RSS_LUT_VSI_VALID		(0x1 << 15)
#define I40E_AQC_SET_RSS_LUT_VSI_ID_SHIFT	0
#define I40E_AQC_SET_RSS_LUT_VSI_ID_MASK	(0x3FF << I40E_AQC_SET_RSS_LUT_VSI_ID_SHIFT)
	uint16_t	vsi_id;
#define I40E_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT	0
#define I40E_AQC_SET_RSS_LUT_TABLE_TYPE_MASK	(0x1 << I40E_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT)
#define I40E_AQC_SET_RSS_LUT_TABLE_TYPE_VSI	0
#define I40E_AQC_SET_RSS_LUT_TABLE_TYPE_PF	1
	uint16_t	flags;
	uint8_t		reserved[4];
	uint32_t	addr_high;
	uint32_t	addr_low;
};

int i40e_aqc_wait_cmd(struct ufp_dev *dev,
	struct i40e_aq_session *session);

void i40e_aqc_req_firmware_version(struct ufp_dev *dev,
	struct i40e_aq_session *session);
void i40e_aqc_req_driver_version(struct ufp_dev *dev,
	struct i40e_aq_session *session);
void i40e_aqc_req_queue_shutdown(struct ufp_dev *dev,
	struct i40e_aq_session *session);
void i40e_aqc_req_macaddr_read(struct ufp_dev *dev,
	struct i40e_aq_session *session);
void i40e_aqc_req_clear_pxemode(struct ufp_dev *dev,
	struct i40e_aq_session *session);
void i40e_aqc_req_get_swconf(struct ufp_dev *dev,
	struct i40e_aq_session *session);
void i40e_aqc_req_set_swconf(struct ufp_dev *dev,
	uint16_t flags, uint16_t valid_flags,
	struct i40e_aq_session *session);
void i40e_aqc_req_rxctl_write(struct ufp_dev *dev,
	uint32_t reg_addr, uint32_t reg_val,
	struct i40e_aq_session *session);
void i40e_aqc_req_rxctl_read(struct ufp_dev *dev,
	uint32_t reg_addr,
	struct i40e_aq_session *session);
void i40e_aqc_req_update_vsi(struct ufp_dev *dev,
	struct ufp_iface *iface, struct i40e_aq_buf_vsi_data *buf,
	struct i40e_aq_session *session);
void i40e_aqc_req_get_vsi(struct ufp_dev *dev,
	struct ufp_iface *iface,
	struct i40e_aq_session *session);
void i40e_aqc_req_promisc_mode(struct ufp_dev *dev,
	struct ufp_iface *iface, uint16_t promisc_flags,
	struct i40e_aq_session *session);
void i40e_aqc_req_set_phyintmask(struct ufp_dev *dev,
	uint16_t mask,
	struct i40e_aq_session *session);
void i40e_aqc_req_stop_lldp(struct ufp_dev *dev,
	struct i40e_aq_session *session);
void i40e_aqc_req_set_rsskey(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *key, uint16_t key_size,
	struct i40e_aq_session *session);
void i40e_aqc_req_set_rsslut(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *lut, uint16_t lut_size,
	struct i40e_aq_session *session);

void i40e_aqc_resp_firmware_version(struct ufp_dev *dev,
	void *cmd_ptr);
void i40e_aqc_resp_macaddr_read(struct ufp_dev *dev,
	void *cmd_ptr, void *buf_ptr,
	struct i40e_aq_session *session);
void i40e_aqc_resp_clear_pxemode(struct ufp_dev *dev);
void i40e_aqc_resp_get_swconf(struct ufp_dev *dev,
	void *cmd_ptr, void *buf_ptr,
	struct i40e_aq_session *session);
void i40e_aqc_resp_rxctl_read(struct ufp_dev *dev,
	void *cmd_ptr,
	struct i40e_aq_session *session);
void i40e_aqc_resp_get_vsi(struct ufp_dev *dev,
	void *cmd_ptr, void *buf_ptr,
	struct i40e_aq_session *session);

#endif /* _I40E_AQC_H__ */
