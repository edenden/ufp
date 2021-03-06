#ifndef _IXGBEVF_H__
#define _IXGBEVF_H__

#define IXGBE_VF_IRQ_CLEAR_MASK	7
#define IXGBE_VF_MAX_TX_QUEUES	8
#define IXGBE_VF_MAX_RX_QUEUES	8
#define IXGBEVF_X550_VFRETA_SIZE 64 /* 64 entries */
#define IXGBEVF_VFRSSRK_REGS	10 /* 10 registers for RSS key */
#define IXGBEVF_MAX_TXD		4096
#define IXGBEVF_MIN_TXD		64
#define IXGBEVF_MAX_RXD		4096
#define IXGBEVF_MIN_RXD		64
#define IXGBEVF_RX_HDR_SIZE	256
#define IXGBEVF_RX_BUFSZ	2048
#define VLAN_ETH_FRAME_LEN	1518
#define IXGBE_MAX_JUMBO_FRAME_SIZE 16128

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
#define IXGBE_VFRETA(x)		(0x3200 + ((x) * 4))

#define IXGBE_VF_INIT_TIMEOUT	200 /* Number of retries to clear RSTI */

/* PSRTYPE bit definitions */
#define IXGBE_PSRTYPE_TCPHDR		0x00000010
#define IXGBE_PSRTYPE_UDPHDR		0x00000020
#define IXGBE_PSRTYPE_IPV4HDR		0x00000100
#define IXGBE_PSRTYPE_IPV6HDR		0x00000200
#define IXGBE_PSRTYPE_L2HDR		0x00001000

/* SRRCTL bit definitions */
#define IXGBE_SRRCTL_BSIZEPKT_SHIFT	10	/* so many KBs */
#define IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT	2	/* 64byte resolution (>> 6)
						 * + at bit 8 offset (<< 8)
						 *  = (<< 2) */
#define IXGBE_SRRCTL_DROP_EN		0x10000000
#define IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF 0x02000000

/* Direct Cache Access (DCA) definitions */
#define IXGBE_DCA_RXCTRL_DESC_RRO_EN	(1 << 9) /* Rx rd Desc Relax Order */
#define IXGBE_DCA_RXCTRL_DATA_WRO_EN	(1 << 13) /* Rx wr data Relax Order */
#define IXGBE_DCA_RXCTRL_HEAD_WRO_EN	(1 << 15) /* Rx wr header RO */

#define IXGBE_DCA_TXCTRL_DESC_RRO_EN	(1 << 9) /* Tx rd Desc Relax Order */
#define IXGBE_DCA_TXCTRL_DESC_WRO_EN	(1 << 11) /* Tx Desc writeback RO bit */
#define IXGBE_DCA_TXCTRL_DATA_RRO_EN	(1 << 13) /* Tx rd data Relax Order */

/* CTRL Bit Masks */
#define IXGBE_CTRL_RST			0x04000000 /* Reset (SW) */

/* Transmit Config masks */
#define IXGBE_TXDCTL_ENABLE		0x02000000 /* Ena specific Tx Queue */
#define IXGBE_TXDCTL_SWFLSH		0x04000000 /* Tx Desc. wr-bk flushing */

/* Receive Config masks */
#define IXGBE_RXDCTL_ENABLE		0x02000000 /* Ena specific Rx Queue */
#define IXGBE_RXDCTL_RLPML_EN		0x00008000
#define IXGBE_RXDCTL_VME		0x40000000 /* VLAN mode enable */

/* 82599 EITR is only 12 bits, with the lower 3 always zero */
/*
 * 82598 EITR is 16 bits but set the limits based on the max
 * supported by all ixgbe hardware
 */
#define IXGBE_MAX_EITR			0x00000FF8
#define IXGBE_EITR_CNT_WDIS		0x80000000

/* Interrupt Vector Allocation Registers */
#define IXGBE_IVAR_ALLOC_VAL		0x80 /* Interrupt Allocation valid */

#define DMA_64BIT_MASK		0xffffffffffffffffULL
#define DMA_BIT_MASK(n)		(((n) == 64) ? \
				DMA_64BIT_MASK : ((1ULL<<(n))-1))

#define msleep(t, n)		(t)->tv_sec = 0; \
				(t)->tv_nsec = ((n) * 1000000); \
				nanosleep((t), NULL);
#define usleep(t, n)		(t)->tv_sec = 0; \
				(t)->tv_nsec = ((n) * 1000); \
				nanosleep((t), NULL);

enum ixgbevf_xcast_modes {
	IXGBEVF_XCAST_MODE_NONE = 0,
	IXGBEVF_XCAST_MODE_MULTI,
	IXGBEVF_XCAST_MODE_ALLMULTI,
};

struct ufp_ixgbevf_data {
	int32_t		mc_filter_type;
	int		api_version;
	uint32_t	mbx_timeout;
	uint32_t	mbx_udelay;
	uint32_t	mbx_v2p_mailbox;
	uint16_t	mbx_size;
};

/* Receive Descriptor - Advanced */
union ufp_ixgbevf_rx_desc {
	struct {
		uint64_t pkt_addr; /* Packet buffer address */
		uint64_t hdr_addr; /* Header buffer address */
	} read;
	struct {
		struct {
			union {
				uint32_t data;
				struct {
					uint16_t pkt_info; /* RSS, Pkt type */
					uint16_t hdr_info; /* Splithdr, hdrlen */
				} hs_rss;
			} lo_dword;
			union {
				uint32_t rss; /* RSS Hash */
				struct {
					uint16_t ip_id; /* IP id */
					uint16_t csum; /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			uint32_t status_error; /* ext status/error */
			uint16_t length; /* Packet length */
			uint16_t vlan; /* VLAN tag */
		} upper;
	} wb;  /* writeback */
};

/* Transmit Descriptor - Advanced */
union ufp_ixgbevf_tx_desc {
	struct {
		uint64_t buffer_addr; /* Address of descriptor's data buf */
		uint32_t cmd_type_len;
		uint32_t olinfo_status;
	} read;
	struct {
		uint64_t rsvd; /* Reserved */
		uint32_t nxtseq_seed;
		uint32_t status;
	} wb;
};

/* Multiple Receive Queue Control */
#define IXGBE_MRQC_RSSEN		0x00000001 /* RSS Enable */
#define IXGBE_MRQC_RSS_FIELD_IPV4_TCP	0x00010000
#define IXGBE_MRQC_RSS_FIELD_IPV4	0x00020000
#define IXGBE_MRQC_RSS_FIELD_IPV6	0x00100000
#define IXGBE_MRQC_RSS_FIELD_IPV6_TCP	0x00200000

/* Transmit Descriptor bit definitions */
#define IXGBE_TXD_CMD_EOP		0x01000000 /* End of Packet */
#define IXGBE_TXD_CMD_IFCS		0x02000000 /* Insert FCS (Ethernet CRC) */
#define IXGBE_TXD_CMD_RS		0x08000000 /* Report Status */
#define IXGBE_TXD_CMD_DEXT		0x20000000 /* Desc extension (0 = legacy) */
#define IXGBE_TXD_STAT_DD		0x00000001 /* Descriptor Done */

#define IXGBE_ADVTXD_DTYP_DATA		0x00300000 /* Adv Data Descriptor */
#define IXGBE_ADVTXD_DCMD_DEXT		IXGBE_TXD_CMD_DEXT /* Desc ext 1=Adv */
#define IXGBE_ADVTXD_DCMD_IFCS		IXGBE_TXD_CMD_IFCS /* Insert FCS */
#define IXGBE_ADVTXD_CC			0x00000080 /* Check Context */
#define IXGBE_ADVTXD_PAYLEN_SHIFT	14 /* Adv desc PAYLEN shift */

/* Receive Descriptor bit definitions */
#define IXGBE_RXD_STAT_DD		0x01 /* Descriptor Done */
#define IXGBE_RXD_STAT_EOP		0x02 /* End of Packet */

#define IXGBE_RXDADV_ERR_CE		0x01000000 /* CRC Error */
#define IXGBE_RXDADV_ERR_LE		0x02000000 /* Length Error */
#define IXGBE_RXDADV_ERR_PE		0x08000000 /* Packet Error */
#define IXGBE_RXDADV_ERR_OSE		0x10000000 /* Oversize Error */
#define IXGBE_RXDADV_ERR_USE		0x20000000 /* Undersize Error */

#define IXGBE_RXDADV_ERR_FRAME_ERR_MASK ( \
					IXGBE_RXDADV_ERR_CE | \
					IXGBE_RXDADV_ERR_LE | \
					IXGBE_RXDADV_ERR_PE | \
					IXGBE_RXDADV_ERR_OSE | \
					IXGBE_RXDADV_ERR_USE)

#define IXGBE_RX_DESC(R, i)     \
	(&(((union ufp_ixgbevf_rx_desc *)((R)->addr_virt))[i]))
#define IXGBE_TX_DESC(R, i)     \
	(&(((union ufp_ixgbevf_tx_desc *)((R)->addr_virt))[i]))

int ufp_ixgbevf_init(struct ufp_ops *ops);
void ufp_ixgbevf_destroy(struct ufp_ops *ops);

#endif /* _IXGBEVF_H__ */
