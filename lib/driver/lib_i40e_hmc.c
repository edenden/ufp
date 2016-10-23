
u64 i40e_calculate_l2fpm_size(u32 txq_num, u32 rxq_num)
{
	u64 fpm_size = 0;

	fpm_size = txq_num * I40E_HMC_OBJ_SIZE_TXQ;
	fpm_size = ALIGN(fpm_size, I40E_HMC_L2OBJ_BASE_ALIGNMENT);

	fpm_size += rxq_num * I40E_HMC_OBJ_SIZE_RXQ;
	fpm_size = ALIGN(fpm_size, I40E_HMC_L2OBJ_BASE_ALIGNMENT);

	return fpm_size;
}

int ufp_i40e_hmc_init(struct ufp_hadle *ih, u32 txq_num, u32 rxq_num)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_hmc_info *hmc;
	struct i40e_hmc_obj_info *obj, *full_obj;
	u64 l2fpm_size;
	uint32_t page_size = sysconf(_SC_PAGESIZE);

	hmc = &data->hmc;
	hmc->hmc_fn_id = hw->pf_id;

	/* allocate memory for hmc_obj */
	hmc->hmc_obj = malloc(sizeof(struct i40e_hmc_obj_info) * I40E_HMC_LAN_MAX);
	if(!hmc->hmc_obj)
		goto alloc_hmc_obj;

	/* allocate memory for SD entry table */
	hmc->first_sd_index = 0;
	hmc->sd_table.ref_cnt = 0;
	l2fpm_size = i40e_calculate_l2fpm_size(txq_num, rxq_num);

	hmc->sd_table.sd_cnt = (u32)(l2fpm_size + page_size - 1) / page_size;
	hmc->sd_table.sd_entry =
		malloc(sizeof(struct i40e_hmc_sd_entry) * hw->hmc.sd_table.sd_cnt);
	if(!hmc->sd_table.sd_entry)
		goto err_alloc_sd_entry;

	/* The full object will be used to create the LAN HMC SD */
	full_obj = &hw->hmc.hmc_obj[I40E_HMC_LAN_FULL];
	full_obj->max_cnt = 0;
	full_obj->cnt = 0;
	full_obj->base = 0;
	full_obj->size = l2fpm_size;

	/* Tx queue context information */
	obj = &hw->hmc.hmc_obj[I40E_HMC_LAN_TX];
	obj->max_cnt = rd32(hw, I40E_GLHMC_LANQMAX);
	obj->cnt = txq_num;
	obj->base = 0;
	obj->size = BIT_ULL(rd32(hw, I40E_GLHMC_LANTXOBJSZ));

	/* validate values requested by driver don't exceed HMC capacity */
	if (txq_num > obj->max_cnt)
		goto err_txq_count;

	/* aggregate values into the full LAN object for later */
	full_obj->max_cnt += obj->max_cnt;
	full_obj->cnt += obj->cnt;

	/* Rx queue context information */
	obj = &hw->hmc.hmc_obj[I40E_HMC_LAN_RX];
	obj->max_cnt = rd32(hw, I40E_GLHMC_LANQMAX);
	obj->cnt = rxq_num;
	obj->base = hw->hmc.hmc_obj[I40E_HMC_LAN_TX].base +
		    (hw->hmc.hmc_obj[I40E_HMC_LAN_TX].cnt *
		     hw->hmc.hmc_obj[I40E_HMC_LAN_TX].size);
	obj->base = ALIGN(obj->base, I40E_HMC_L2OBJ_BASE_ALIGNMENT);
	obj->size = BIT_ULL(rd32(hw, I40E_GLHMC_LANRXOBJSZ));

	/* validate values requested by driver don't exceed HMC capacity */
	if (rxq_num > obj->max_cnt)
		goto err_rxq_count;

	/* aggregate values into the full LAN object for later */
	full_obj->max_cnt += obj->max_cnt;
	full_obj->cnt += obj->cnt;

	return 0;

err_rxq_count:
err_txq_count:
	free(hmc->sd_table.sd_entry);
err_alloc_sd_entry:
	free(hmc->hmc_obj);
err_alloc_hmc_obj:
	return -1;
}

void i40e_hmc_destroy(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_hmc_info *hmc;

	hmc = &data->hmc;

	free(hmc->sd_table.sd_entry);
	free(hmc->hmc_obj);

	return;
}

i40e_status i40e_configure_lan_hmc(struct ufp_hadle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_hmc_info *hmc;
	struct i40e_hmc_obj_info *obj;
	struct i40e_hmc_sd_entry *sd_entry;
	unsigned int sd_allocated = 0;
	int err;

	hmc = &data->hmc;

	for (i = 0; i < hmc->sd_table.sd_cnt; i++, sd_allocated++){
		sd_entry = hmc->sd_table.sd_entry[i];

		sd_entry->u.bp.addr = ufp_i40e_page_alloc(ih);
		if(!sd_entry->u.bp.addr)
			goto err_add_sd_table_entry;

		sd_entry->u.bp.sd_pd_index = i;
		sd_entry->valid = true;

		/* increment the ref count */
		I40E_INC_SD_REFCNT(&hmc_info->sd_table);

		/* Increment backing page reference count */
		I40E_INC_BP_REFCNT(&sd_entry->u.bp);

		i40e_set_pf_sd_entry(hw, sd_entry->u.bp.addr.pa, i,
			I40E_SD_TYPE_DIRECT);
	}

	/* Configure and program the FPM registers so objects can be created */

	/* Tx contexts */
	obj = &hmc->hmc_obj[I40E_HMC_LAN_TX];
	wr32(hw, I40E_GLHMC_LANTXBASE(hmc->fn_id),
	     (u32)((obj->base & I40E_GLHMC_LANTXBASE_FPMLANTXBASE_MASK) / 512));
	wr32(hw, I40E_GLHMC_LANTXCNT(hmc->fn_id), obj->cnt);

	/* Rx contexts */
	obj = &hmc->hmc_obj[I40E_HMC_LAN_RX];
	wr32(hw, I40E_GLHMC_LANRXBASE(hmc->fn_id),
	     (u32)((obj->base & I40E_GLHMC_LANRXBASE_FPMLANRXBASE_MASK) / 512));
	wr32(hw, I40E_GLHMC_LANRXCNT(hmc->fn_id), obj->cnt);

	return 0;

configure_lan_hmc_out:
exit_sd_error:
	for(i = 0; i < sd_allocated; i++){
		sd_entry = &hmc->sd_table.sd_entry[i];

		I40E_DEC_BP_REFCNT(&sd_entry->u.bp);
		if (sd_entry->u.bp.ref_cnt) {
			goto err_not_ready;
		}

		I40E_DEC_SD_REFCNT(&hmc->sd_table);

		/* mark the entry invalid */
		sd_entry->valid = false;

		i40e_clear_pf_sd_entry(hw, i, I40E_SD_TYPE_DIRECT);
		ufp_i40e_page_release(sd_entry->u.bp.addr);
	}
	return -1;
}

i40e_status i40e_shutdown_lan_hmc(struct ufp_hadle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_hmc_info *hmc;
	struct i40e_hmc_obj_info *obj;
	struct i40e_hmc_sd_entry *sd_entry;
	int err;

	hmc = &data->hmc;

	for (i = 0; i < hmc->sd_table.sd_cnt; i++){
		if (!hmc->sd_table.sd_entry[i].valid)
			continue;

		/* get the entry and decrease its ref counter */
		sd_entry = &hmc->sd_table.sd_entry[i];

		I40E_DEC_BP_REFCNT(&sd_entry->u.bp);
		if (sd_entry->u.bp.ref_cnt) {
			goto err_not_ready;
		}

		I40E_DEC_SD_REFCNT(&hmc->sd_table);

		/* mark the entry invalid */
		sd_entry->valid = false;

		i40e_clear_pf_sd_entry(hw, i, I40E_SD_TYPE_DIRECT);
		ufp_i40e_page_release(sd_entry->u.bp.addr);
	}

}

static void i40e_set_pf_sd_entry(struct ufp_handle *ih, unsigned long pa,
	uint32_t sd_index,  enum i40e_sd_entry_type type)
{
	u32 val1, val2, val3;

	val1 = (u32)(upper_32_bits(pa));

	val2 = (u32)(pa)
	val2 |= I40E_HMC_MAX_BP_COUNT
		<< I40E_PFHMC_SDDATALOW_PMSDBPCOUNT_SHIFT;
	val2 |= (((type) == I40E_SD_TYPE_PAGED) ? 0 : 1)
		<< I40E_PFHMC_SDDATALOW_PMSDTYPE_SHIFT;
	val2 |= BIT(I40E_PFHMC_SDDATALOW_PMSDVALID_SHIFT);

	val3 = (sd_index) | BIT_ULL(I40E_PFHMC_SDCMD_PMSDWR_SHIFT);

	wr32((hw), I40E_PFHMC_SDDATAHIGH, val1);
	wr32((hw), I40E_PFHMC_SDDATALOW, val2);
	wr32((hw), I40E_PFHMC_SDCMD, val3);

	return;
}

static void i40e_clear_pf_sd_entry(struct ufp_handle *ih,
	uint32_t sd_index, enum i40e_sd_entry_type type)
{
	u32 val2, val3;

	val2 = I40E_HMC_MAX_BP_COUNT
		<< I40E_PFHMC_SDDATALOW_PMSDBPCOUNT_SHIFT;
	val2 |= (((type) == I40E_SD_TYPE_PAGED) ? 0 : 1)
		<< I40E_PFHMC_SDDATALOW_PMSDTYPE_SHIFT;

	val3 = (sd_index) | BIT_ULL(I40E_PFHMC_SDCMD_PMSDWR_SHIFT); 

	wr32((hw), I40E_PFHMC_SDDATAHIGH, 0);
	wr32((hw), I40E_PFHMC_SDDATALOW, val2);
	wr32((hw), I40E_PFHMC_SDCMD, val3);

	return;
}

void i40e_hmc_write(uint8_t *hmc_bits,
	struct i40e_context_ele *ce_info, uint8_t *host_buf)
{
	uint8_t data[8], mask[8];
	uint8_t *src, *dst;
	uint8_t byte_data, prev_data, byte_mask, prev_mask;
	uint16_t shift_width, width_remain;
	int i;

	/* copy from the next struct field */
	src = host_buf + ce_info->offset;

	for(i = 0; i < ce_info->size_of; i++){
#if __BYTE_ORDER == __LITTLE_ENDIAN
		data[i] = src[i];
#elif __BYTE_ORDER == __BIG_ENDIAN
		data[i] = src[ce_info->size_of - (i + 1)];
#endif

		width_remain = ce_info->width - (i * 8);
		mask[i] = (1 << min(width_remain, (uint16_t)8)) - 1;
	}

	/* prepare the bits and mask */
	shift_width = ce_info->lsb % 8;

	/* get the current bits from the target bit string */
	dst = hmc_bits + (ce_info->lsb / 8);

	prev_data = 0;
	prev_mask = 0;
	for(i = 0; i < ce_info->size_of; i++){
		byte_data = data[i] << shift_width;
		byte_data |= prev_data;

		byte_mask = mask[i] << shift_width;
		byte_mask |= prev_mask;

		dst[i] = byte_data | (~byte_mask & dst[i]);

		prev_data = data[i] >> (8 - shift_width);
		prev_mask = mask[i] >> (8 - shift_width);
	}

	return;
}

void i40e_hmc_read(uint8_t *hmc_bits,
	struct i40e_context_ele *ce_info, uint8_t *host_buf)
{
	uint8_t data[8], mask[8];
	uint8_t *src, *dst;
	uint8_t byte_data, next_data;
	uint16_t shift_width, width_remain;
	int i;

	/* prepare the bits and mask */
	shift_width = ce_info->lsb % 8;

	/* get the current bits from the src bit string */
	src = hmc_bits + (ce_info->lsb / 8);

	for(i = 0; i < ce_info->size_of; i++){
		width_remain = ce_info->width - (i * 8);
		mask[i] = (1 << min(width_remain, (uint16_t)8)) - 1;
	}

	next_data = 0;
	for(i = ce_info->size_of - 1; i >= 0; i--){
		byte_data = src[i] >> shift_width;
		byte_data |= next_data;

		data[i] = byte_data & mask[i];

		next_data = src[i] << (8 - shift_width);
	}

	/* get the address from the struct field */
	dst = host_buf + ce_info->offset;

	for(i = 0; i < ce_info->size_of; i++){
#if __BYTE_ORDER == __LITTLE_ENDIAN
		dst[i] = data[i];
#elif __BYTE_ORDER == __BIG_ENDIAN
		dst[i] = data[ce_info->size_of - (i + 1)];
#endif
	}

	return;
}

