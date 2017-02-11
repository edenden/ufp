#ifndef _I40E_HMC_H__
#define _I40E_HMC_H__

#define I40E_HMC_MAX_BP_COUNT		512
#define I40E_HMC_DIRECT_BP_SIZE		0x200000 /* 2M */
#define I40E_HMC_PAGED_BP_SIZE		4096
#define I40E_HMC_L2OBJ_BASE_ALIGNMENT	512

struct i40e_hmc_obj {
	uint64_t base;	/* base addr in FPM */
	uint32_t count;	/* maximum count of objects */
	uint64_t size;	/* size in bytes of one object */
};

struct i40e_hmc_sd_entry {
	struct i40e_page *pd_addrs;
	struct i40e_page *pd[I40E_HMC_MAX_BP_COUNT];
};

struct i40e_hmc_sd_table {
	uint32_t sd_count;
	struct i40e_hmc_sd_entry *sd_entry;
};

struct i40e_hmc {
	/* hmc objects */
	struct i40e_hmc_obj obj_tx;
	struct i40e_hmc_obj obj_rx;

	struct i40e_hmc_sd_table sd_table;
};

enum i40e_sd_entry_type {
	I40E_SD_TYPE_INVALID = 0,
	I40E_SD_TYPE_PAGED   = 1,
	I40E_SD_TYPE_DIRECT  = 2
};

struct i40e_hmc_ctx_rx {
	uint16_t head;
	uint16_t cpuid; /* bigger than needed, see above for reason */
	uint64_t base;
	uint16_t qlen;
#define I40E_RXQ_CTX_DBUFF_SHIFT 7
	uint16_t dbuff; /* bigger than needed, see above for reason */
#define I40E_RXQ_CTX_HBUFF_SHIFT 6
	uint16_t hbuff; /* bigger than needed, see above for reason */
	uint8_t  dtype;
	uint8_t  dsize;
	uint8_t  crcstrip;
	uint8_t  fc_en;
	uint8_t  l2tsel;
	uint8_t  hsplit_0;
	uint8_t  hsplit_1;
	uint8_t  showiv;
	uint32_t rxmax; /* bigger than needed, see above for reason */
	uint8_t  tphrdesc_en;
	uint8_t  tphwdesc_en;
	uint8_t  tphdata_en;
	uint8_t  tphhead_en;
	uint16_t lrxqthresh; /* bigger than needed, see above for reason */
	uint8_t  pref_en;    /* NOTE: normally must be set to 1 at init */
};

 #define I40E_HMC_CE_RX {\
	/* Field					Width	LSB */	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, head),	13,	0 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, cpuid),	8,	13 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, base),	57,	32 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, qlen),	13,	89 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, dbuff),	7,	102 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, hbuff),	5,	109 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, dtype),	2,	114 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, dsize),	1,	116 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, crcstrip),	1,	117 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, fc_en),	1,	118 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, l2tsel),	1,	119 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, hsplit_0),	4,	120 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, hsplit_1),	2,	124 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, showiv),	1,	127 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, rxmax),	14,	174 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, tphrdesc_en),	1,	193 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, tphwdesc_en),	1,	194 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, tphdata_en),	1,	195 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, tphhead_en),	1,	196 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, lrxqthresh),	3,	198 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_rx, pref_en),	1,	201 },	\
	{ 0 } }


struct i40e_hmc_ctx_tx {
	uint16_t head;
	uint8_t  new_context;
	uint64_t base;
	uint8_t  fc_en;
	uint8_t  timesync_en;
	uint8_t  fd_en;
	uint8_t  alt_vlan_en;
	uint16_t thead_wb;
	uint8_t  cpuid;
	uint8_t  head_wb_en;
	uint16_t qlen;
	uint8_t  tphrdesc_en;
	uint8_t  tphrpkt_en;
	uint8_t  tphwdesc_en;
	uint64_t headwb_addr;
	uint32_t crc;
	uint16_t rdylist;
	uint8_t  rdylist_act;
};

#define I40E_HMC_CE_TX {\
	/* Field					Width	LSB */	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, head),	13,	0 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, new_context),	1,	30 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, base),	57,	32 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, fc_en),	1,	89 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, timesync_en),	1,	90 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, fd_en),	1,	91 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, alt_vlan_en),	1,	92 },	\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, cpuid),	8,	96 },	\
	/* line 1 */							\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, thead_wb),			\
		13,	0 + 128 },					\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, head_wb_en),			\
		1,	32 + 128 },					\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, qlen),			\
		13,	33 + 128 },					\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, tphrdesc_en),			\
		1,	46 + 128 },					\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, tphrpkt_en),			\
		1,	47 + 128 },					\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, tphwdesc_en),			\
		1,	48 + 128 },					\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, headwb_addr),			\
		64,	64 + 128 },					\
	/* line 7 */							\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, crc),				\
		32,	0 + (7 * 128) },				\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, rdylist),			\
		10,	84 + (7 * 128) },				\
	{ I40E_HMC_FIELD(i40e_hmc_ctx_tx, rdylist_act),			\
		1,	94 + (7 * 128) },				\
	{ 0 } }


struct i40e_hmc_ce {
	uint16_t offset;
	uint16_t size_of;
	uint16_t width;
	uint16_t lsb;
};

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
#define I40E_HMC_FIELD(_struct, _ele) \
	offsetof(struct _struct, _ele), \
	FIELD_SIZEOF(struct _struct, _ele)

int i40e_hmc_init(struct ufp_dev *dev);
void i40e_hmc_destroy(struct ufp_dev *dev);
int i40e_hmc_set_ctx_tx(struct ufp_dev *dev, struct i40e_hmc_ctx_tx *ctx,
	uint16_t qp_idx);
int i40e_hmc_set_ctx_rx(struct ufp_dev *dev, struct i40e_hmc_ctx_rx *ctx,
	uint16_t qp_idx);

#endif /* _I40E_HMC_H__ */
