#ifndef _UFP_HW_H__
#define _UFP_HW_H__

#define IXGBE_VF_IRQ_CLEAR_MASK	7
#define IXGBE_VF_MAX_TX_QUEUES	8
#define IXGBE_VF_MAX_RX_QUEUES	8

/* DCB define */
#define IXGBE_VF_MAX_TRAFFIC_CLASS	8

#define IXGBE_VFCTRL		0x00000
#define IXGBE_VFSTATUS		0x00008
#define IXGBE_VFLINKS		0x00010
#define IXGBE_VFFRTIMER		0x00048
#define IXGBE_VFRXMEMWRAP	0x03190
#define IXGBE_VTEICR		0x00100
#define IXGBE_VTEICS		0x00104
#define IXGBE_VTEIMS		0x00108
#define IXGBE_VTEIMC		0x0010C
#define IXGBE_VTEIAC		0x00110
#define IXGBE_VTEIAM		0x00114
#define IXGBE_VTEITR(x)		(0x00820 + (4 * (x)))
#define IXGBE_VTIVAR(x)		(0x00120 + (4 * (x)))
#define IXGBE_VTIVAR_MISC	0x00140
#define IXGBE_VTRSCINT(x)	(0x00180 + (4 * (x)))
/* define IXGBE_VFPBACL  still says TBD in EAS */
#define IXGBE_VFRDBAL(x)	(0x01000 + (0x40 * (x)))
#define IXGBE_VFRDBAH(x)	(0x01004 + (0x40 * (x)))
#define IXGBE_VFRDLEN(x)	(0x01008 + (0x40 * (x)))
#define IXGBE_VFRDH(x)		(0x01010 + (0x40 * (x)))
#define IXGBE_VFRDT(x)		(0x01018 + (0x40 * (x)))
#define IXGBE_VFRXDCTL(x)	(0x01028 + (0x40 * (x)))
#define IXGBE_VFSRRCTL(x)	(0x01014 + (0x40 * (x)))
#define IXGBE_VFRSCCTL(x)	(0x0102C + (0x40 * (x)))
#define IXGBE_VFPSRTYPE		0x00300
#define IXGBE_VFTDBAL(x)	(0x02000 + (0x40 * (x)))
#define IXGBE_VFTDBAH(x)	(0x02004 + (0x40 * (x)))
#define IXGBE_VFTDLEN(x)	(0x02008 + (0x40 * (x)))
#define IXGBE_VFTDH(x)		(0x02010 + (0x40 * (x)))
#define IXGBE_VFTDT(x)		(0x02018 + (0x40 * (x)))
#define IXGBE_VFTXDCTL(x)	(0x02028 + (0x40 * (x)))
#define IXGBE_VFTDWBAL(x)	(0x02038 + (0x40 * (x)))
#define IXGBE_VFTDWBAH(x)	(0x0203C + (0x40 * (x)))
#define IXGBE_VFDCA_RXCTRL(x)	(0x0100C + (0x40 * (x)))
#define IXGBE_VFDCA_TXCTRL(x)	(0x0200c + (0x40 * (x)))
#define IXGBE_VFGPRC		0x0101C
#define IXGBE_VFGPTC		0x0201C
#define IXGBE_VFGORC_LSB	0x01020
#define IXGBE_VFGORC_MSB	0x01024
#define IXGBE_VFGOTC_LSB	0x02020
#define IXGBE_VFGOTC_MSB	0x02024
#define IXGBE_VFMPRC		0x01034
#define IXGBE_VFMRQC		0x3000
#define IXGBE_VFRSSRK(x)	(0x3100 + ((x) * 4))
#define IXGBE_VFRETA(x)	(0x3200 + ((x) * 4))

struct ufp_hw;

struct ufp_mac_operations {
	/* MAC */
	s32 (*reset_hw)(struct ufp_hw *);
	s32 (*stop_adapter)(struct ufp_hw *);

	/* Link */
	s32 (*check_link)(struct ufp_hw *, u32 *, u32 *);

	/* RAR, Multicast, VLAN */
	s32 (*set_rar)(struct ufp_hw *, u32, u8 *, u32, u32);
	s32 (*set_uc_addr)(struct ufp_hw *, u32, u8 *);
	s32 (*update_mc_addr_list)(struct ufp_hw *, u8 *, u32, ufp_mc_addr_itr);
	s32 (*update_xcast_mode)(struct ufp_hw *, struct net_device *, int);
	s32 (*set_vfta)(struct ufp_hw *, u32, u32);
	s32 (*set_rlpml)(struct ufp_hw *, u16);
};

enum ufp_mac_type {
	ixgbe_mac_unknown = 0,
	ixgbe_mac_82598EB,
	ixgbe_mac_82599EB,
	ixgbe_mac_82599_vf,
	ixgbe_mac_X540,
	ixgbe_mac_X540_vf,
	ixgbe_mac_X550,
	ixgbe_mac_X550EM_x,
	ixgbe_mac_X550_vf,
	ixgbe_mac_X550EM_x_vf,
	ixgbe_num_macs
};

struct ufp_mac_info {
	struct ufp_mac_operations ops;
	u8 perm_addr[6];

	enum ufp_mac_type type;

	s32  mc_filter_type;

	u32 get_link_status;
	u32  max_tx_queues;
	u32  max_rx_queues;
	u32  max_msix_vectors;
};

struct ufp_mbx_operations {
	void (*init_params)(struct ufp_hw *hw);
	s32  (*read)(struct ufp_hw *, u32 *, u16,  u16);
	s32  (*write)(struct ufp_hw *, u32 *, u16, u16);
	s32  (*read_posted)(struct ufp_hw *, u32 *, u16,  u16);
	s32  (*write_posted)(struct ufp_hw *, u32 *, u16, u16);
	s32  (*check_for_msg)(struct ufp_hw *, u16);
	s32  (*check_for_ack)(struct ufp_hw *, u16);
	s32  (*check_for_rst)(struct ufp_hw *, u16);
};

struct ufp_mbx_stats {
	u32 msgs_tx;
	u32 msgs_rx;

	u32 acks;
	u32 reqs;
	u32 rsts;
};

struct ufp_mbx_info {
	struct ufp_mbx_operations ops;
	struct ufp_mbx_stats stats;
	u32 timeout;
	u32 udelay;
	u32 v2p_mailbox;
	u16 size;
};

struct ufp_hw {
	void *back;
	u8 __iomem *hw_addr;
	struct ufp_mac_info mac;
	struct ufp_mbx_info mbx;
	u16 device_id;
	u16 subsystem_vendor_id;
	u16 subsystem_device_id;
	u16 vendor_id;
	u8  revision_id;
	int api_version;
};

#endif /* _UFP_HW_H__ */
