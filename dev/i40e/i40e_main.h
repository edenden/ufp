#ifndef _I40E_MAIN_H__
#define _I40E_MAIN_H__

#define I40E_PF_RESET_WAIT_COUNT	200
#define I40E_QUEUE_END_OF_LIST		0x7FF

/* Interrupt Throttling and Rate Limiting Goodies */
#define I40E_MAX_ITR			0x0FF0  /* reg uses 2 usec resolution */
#define I40E_MIN_ITR			0x0001  /* reg uses 2 usec resolution */
#define I40E_ITR_100K			0x0005
#define I40E_ITR_50K			0x000A
#define I40E_ITR_20K			0x0019
#define I40E_ITR_18K			0x001B
#define I40E_ITR_8K			0x003E
#define I40E_ITR_4K			0x007A

/* this enum matches hardware bits and is meant to be used by DYN_CTLN
 * registers and QINT registers or more generally anywhere in the manual
 * mentioning ITR_INDX, ITR_NONE cannot be used as an index 'n' into any
 * register but instead is a special value meaning "don't update" ITR0/1/2.
 */
enum i40e_dyn_idx_t {
	I40E_IDX_ITR0 = 0,
	I40E_IDX_ITR1 = 1,
	I40E_IDX_ITR2 = 2,
	I40E_ITR_NONE = 3	/* ITR_NONE must not be used as an index */
};

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
	struct list_node	list;
};

struct i40e_dev {
	uint8_t			pf_id;
	enum i40e_mac_type	mac_type;

	uint8_t			pf_lan_mac[6];
	struct i40e_aq		aq;
	struct i40e_hmc		hmc;
	struct list_head	elem;
};

struct i40e_iface {
	enum i40e_vsi_type	type;
	uint16_t		seid;
	uint16_t		id;
	uint16_t		base_qp;
	uint16_t		qs_handles[8];
};

struct i40e_page {
	void			*addr_virt;
	unsigned long		addr_dma;
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

int i40e_open(struct ufp_dev *dev);
void i40e_close(struct ufp_dev *dev);
int i40e_up(struct ufp_dev *dev, struct ufp_iface *iface);
int i40e_down(struct ufp_dev *dev, struct ufp_iface *iface);
struct i40e_page *i40e_page_alloc(struct ufp_dev *dev);
void i40e_page_release(struct ufp_dev *dev, struct i40e_page *page);
int i40e_wait_cmd(struct ufp_dev *dev);
int i40e_setup_misc_irq(struct ufp_dev *dev);
void i40e_shutdown_misc_irq(struct ufp_dev *dev);
void i40e_start_misc_irq(struct ufp_dev *dev);
void i40e_stop_misc_irq(struct ufp_dev *dev);

#endif /* _I40E_MAIN_H__ */
