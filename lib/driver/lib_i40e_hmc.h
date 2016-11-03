struct i40e_hmc {
	/* hmc objects */
	struct i40e_hmc_obj hmc_tx;
	struct i40e_hmc_obj hmc_rx;

	struct i40e_hmc_sd_table sd_table;
};

struct i40e_hmc_obj {
	u64 base;	/* base addr in FPM */
	u32 count;	/* maximum count of objects */
	u64 size;	/* size in bytes of one object */
};

struct i40e_hmc_sd_table {
	uint32_t sd_count;
	struct i40e_hmc_sd_entry *sd_entry;
};

struct i40e_hmc_sd_entry {
	struct ufp_i40e_page *pd_addrs;
	struct ufp_i40e_page *pd[I40E_HMC_MAX_BP_COUNT];
};

struct i40e_hmc_ctx_rx {
	u16 head;
	u16 cpuid; /* bigger than needed, see above for reason */
	u64 base;
	u16 qlen;
#define I40E_RXQ_CTX_DBUFF_SHIFT 7
	u16 dbuff; /* bigger than needed, see above for reason */
#define I40E_RXQ_CTX_HBUFF_SHIFT 6
	u16 hbuff; /* bigger than needed, see above for reason */
	u8  dtype;
	u8  dsize;
	u8  crcstrip;
	u8  fc_ena;
	u8  l2tsel;
	u8  hsplit_0;
	u8  hsplit_1;
	u8  showiv;
	u32 rxmax; /* bigger than needed, see above for reason */
	u8  tphrdesc_ena;
	u8  tphwdesc_ena;
	u8  tphdata_ena;
	u8  tphhead_ena;
	u16 lrxqthresh; /* bigger than needed, see above for reason */
	u8  prefena;    /* NOTE: normally must be set to 1 at init */
};

struct i40e_hmc_ctx_tx {
	u16 head;
	u8  new_context;
	u64 base;
	u8  fc_ena;
	u8  timesync_ena;
	u8  fd_ena;
	u8  alt_vlan_ena;
	u16 thead_wb;
	u8  cpuid;
	u8  head_wb_ena;
	u16 qlen;
	u8  tphrdesc_ena;
	u8  tphrpacket_ena;
	u8  tphwdesc_ena;
	u64 head_wb_addr;
	u32 crc;
	u16 rdylist;
	u8  rdylist_act;
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

