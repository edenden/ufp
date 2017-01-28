#ifndef _I40E_HMC_H__
#define _I40E_HMC_H__

struct i40e_hmc {
	/* hmc objects */
	struct i40e_hmc_obj hmc_tx;
	struct i40e_hmc_obj hmc_rx;

	struct i40e_hmc_sd_table sd_table;
};

struct i40e_hmc_obj {
	uint64_t base;	/* base addr in FPM */
	uint32_t count;	/* maximum count of objects */
	uint64_t size;	/* size in bytes of one object */
};

struct i40e_hmc_sd_table {
	uint32_t sd_count;
	struct i40e_hmc_sd_entry *sd_entry;
};

struct i40e_hmc_sd_entry {
	struct i40e_page *pd_addrs;
	struct i40e_page *pd[I40E_HMC_MAX_BP_COUNT];
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
	uint8_t  fc_ena;
	uint8_t  l2tsel;
	uint8_t  hsplit_0;
	uint8_t  hsplit_1;
	uint8_t  showiv;
	uint32_t rxmax; /* bigger than needed, see above for reason */
	uint8_t  tphrdesc_ena;
	uint8_t  tphwdesc_ena;
	uint8_t  tphdata_ena;
	uint8_t  tphhead_ena;
	uint16_t lrxqthresh; /* bigger than needed, see above for reason */
	uint8_t  prefena;    /* NOTE: normally must be set to 1 at init */
};

struct i40e_hmc_ctx_tx {
	uint16_t head;
	uint8_t  new_context;
	uint64_t base;
	uint8_t  fc_ena;
	uint8_t  timesync_ena;
	uint8_t  fd_ena;
	uint8_t  alt_vlan_ena;
	uint16_t thead_wb;
	uint8_t  cpuid;
	uint8_t  head_wb_ena;
	uint16_t qlen;
	uint8_t  tphrdesc_ena;
	uint8_t  tphrpacket_ena;
	uint8_t  tphwdesc_ena;
	uint64_t head_wb_addr;
	uint32_t crc;
	uint16_t rdylist;
	uint8_t  rdylist_act;
};

struct i40e_hmc_ce {
	uint16_t offset;
	uint16_t size_of;
	uint16_t width;
	uint16_t lsb;
};

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
