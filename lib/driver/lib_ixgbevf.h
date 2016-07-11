#ifndef _IXGBEVF_H__
#define _IXGBEVF_H__

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

#define IXGBE_VF_INIT_TIMEOUT		200 /* Number of retries to clear RSTI */

/* SRRCTL bit definitions */
#define IXGBE_SRRCTL_BSIZEPKT_SHIFT	10	/* so many KBs */
#define IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT	2	/* 64byte resolution (>> 6)
						 * + at bit 8 offset (<< 8)
						 *  = (<< 2) */

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
#define IXGBE_TXDCTL_SWFLSH		0x04000000 /* Tx Desc. wr-bk flushing */

/* Receive Config masks */
#define IXGBE_RXDCTL_ENABLE		0x02000000 /* Ena specific Rx Queue */

/* 82599 EITR is only 12 bits, with the lower 3 always zero */
/*
 * 82598 EITR is 16 bits but set the limits based on the max
 * supported by all ixgbe hardware
 */
#define IXGBE_MAX_EITR			0x00000FF8
#define IXGBE_EITR_CNT_WDIS		0x80000000

#define DMA_64BIT_MASK		0xffffffffffffffffULL
#define DMA_BIT_MASK(n)		(((n) == 64) ? \
				DMA_64BIT_MASK : ((1ULL<<(n))-1))

#define msleep(t, n)		(t)->tv_sec = 0; \
				(t)->tv_nsec = ((n) * 1000000); \
				nanosleep((t), NULL);
#define usleep(t, n)		(t)->tv_sec = 0; \
				(t)->tv_nsec = ((n) * 1000); \
				nanosleep((t), NULL);

struct ufp_ixgbevf_data {
	int32_t		mc_filter_type;
	int		api_version;
};

int ufp_ixgbevf_init(struct ufp_handle *ih, struct ufp_ops *ops);
void ufp_ixgbevf_destroy(struct ufp_ops *ops);

#endif /* _IXGBEVF_H__ */
