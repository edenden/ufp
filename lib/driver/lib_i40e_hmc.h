struct i40e_hmc_info {
        /* equals to pci func num for PF and dynamically allocated for VFs */
        u8 fn_id;

        /* hmc objects */
        struct i40e_hmc_obj_info hmc_tx;
	struct i40e_hmc_obj_info hmc_rx;

        struct i40e_hmc_sd_table sd_table;
};

struct i40e_hmc_obj_info {
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

#define I40E_HMC_STORE(_struct, _ele)           \
        offsetof(struct _struct, _ele),         \
        FIELD_SIZEOF(struct _struct, _ele)

struct i40e_context_ele {
        u16 offset;
        u16 size_of;
        u16 width;
        u16 lsb;
};

/* LAN Tx Queue Context */
static struct i40e_context_ele i40e_hmc_txq_ce_info[] = {
                                             /* Field      Width    LSB */
        {I40E_HMC_STORE(i40e_hmc_obj_txq, head),           13,      0 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, new_context),     1,     30 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, base),           57,     32 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, fc_ena),        1,     89 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, timesync_ena),    1,     90 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, fd_ena),        1,     91 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, alt_vlan_ena),    1,     92 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, cpuid),          8,     96 },
/* line 1 */
        {I40E_HMC_STORE(i40e_hmc_obj_txq, thead_wb),       13,  0 + 128 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, head_wb_ena),     1, 32 + 128 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, qlen),           13, 33 + 128 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, tphrdesc_ena),    1, 46 + 128 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, tphrpacket_ena),  1, 47 + 128 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, tphwdesc_ena),    1, 48 + 128 },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, head_wb_addr),   64, 64 + 128 },
/* line 7 */
        {I40E_HMC_STORE(i40e_hmc_obj_txq, crc),     32,  0 + (7 * 128) },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, rdylist),     10, 84 + (7 * 128) },
        {I40E_HMC_STORE(i40e_hmc_obj_txq, rdylist_act),     1, 94 + (7 * 128) },
        { 0 }
};

/* LAN Rx Queue Context */
static struct i40e_context_ele i40e_hmc_rxq_ce_info[] = {
                                         /* Field      Width    LSB */
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, head),       13,     0   },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, cpuid),      8,      13  },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, base),       57,     32  },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, qlen),       13,     89  },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, dbuff),      7,      102 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, hbuff),      5,      109 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, dtype),      2,      114 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, dsize),      1,      116 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, crcstrip),     1,    117 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, fc_ena),       1,    118 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, l2tsel),       1,    119 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, hsplit_0),     4,    120 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, hsplit_1),     2,    124 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, showiv),       1,    127 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, rxmax),       14,    174 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, tphrdesc_ena), 1,    193 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, tphwdesc_ena), 1,    194 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, tphdata_ena),  1,    195 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, tphhead_ena),  1,    196 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, lrxqthresh),   3,    198 },
        { I40E_HMC_STORE(i40e_hmc_obj_rxq, prefena),      1,    201 },
        { 0 }
};
