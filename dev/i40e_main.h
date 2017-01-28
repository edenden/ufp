#ifndef _I40E_MAIN_H__
#define _I40E_MAIN_H__

#define I40E_DEV_ID_SFP_XL710		0x1572
#define I40E_DEV_ID_QEMU		0x1574
#define I40E_DEV_ID_KX_B		0x1580
#define I40E_DEV_ID_KX_C		0x1581
#define I40E_DEV_ID_QSFP_A		0x1583
#define I40E_DEV_ID_QSFP_B		0x1584
#define I40E_DEV_ID_QSFP_C		0x1585
#define I40E_DEV_ID_10G_BASE_T		0x1586
#define I40E_DEV_ID_20G_KR2		0x1587
#define I40E_DEV_ID_20G_KR2_A		0x1588
#define I40E_DEV_ID_10G_BASE_T4		0x1589
#define I40E_DEV_ID_25G_B		0x158A
#define I40E_DEV_ID_25G_SFP28		0x158B
#define I40E_DEV_ID_KX_X722		0x37CE
#define I40E_DEV_ID_QSFP_X722		0x37CF
#define I40E_DEV_ID_SFP_X722		0x37D0
#define I40E_DEV_ID_1G_BASE_T_X722	0x37D1
#define I40E_DEV_ID_10G_BASE_T_X722	0x37D2
#define I40E_DEV_ID_SFP_I_X722		0x37D3
#define I40E_DEV_ID_QSFP_I_X722		0x37D4

enum i40e_mac_type {
	I40E_MAC_UNKNOWN = 0,
	I40E_MAC_X710,
	I40E_MAC_XL710,
	I40E_MAC_VF,
	I40E_MAC_X722,
	I40E_MAC_X722_VF,
	I40E_MAC_GENERIC,
};

struct i40e_elem {
	uint8_t			type;
#define I40E_AQ_SW_ELEM_TYPE_MAC	1
#define I40E_AQ_SW_ELEM_TYPE_PF		2
#define I40E_AQ_SW_ELEM_TYPE_VF		3
#define I40E_AQ_SW_ELEM_TYPE_EMP	4
#define I40E_AQ_SW_ELEM_TYPE_BMC	5
#define I40E_AQ_SW_ELEM_TYPE_PV		16
#define I40E_AQ_SW_ELEM_TYPE_VEB	17
#define I40E_AQ_SW_ELEM_TYPE_PA		18
#define I40E_AQ_SW_ELEM_TYPE_VSI	19
	uint16_t		seid;
	uint16_t		seid_uplink;
	uint16_t		seid_downlink;
	struct i40e_elem	*next;
};

struct i40e_dev {
	uint8_t			pf_id;  
	enum i40e_mac_type	mac_type;

	uint8_t			pf_lan_mac[6];
	struct i40e_aq		aq;
	struct i40e_hmc		hmc;
	struct i40e_elem	*elem;
};

struct i40e_iface {
	enum i40e_vsi_type	type;
	uint16_t		seid;
	uint16_t		id;
	uint16_t		base_qp;
	uint16_t		qs_handles[8];
};

#define msleep(n) ({			\
	struct timespec ts;		\
	ts.tv_sec = 0;			\
	ts.tv_nsec = ((n) * 1000000);	\
	nanosleep(&ts, NULL);		\
})
#define usleep(n) ({			\
	struct timespec ts;		\
	ts.tv_sec = 0;			\
	ts.tv_nsec = ((n) * 1000);	\
	nanosleep(&ts, NULL);		\
})
#define i40e_flush(dev) \
	UFP_READ32((dev), I40E_GLGEN_STAT)

#endif /* _I40E_MAIN_H__ */
