/**
 * i40e_align_l2obj_base - aligns base object pointer to 512 bytes
 * @offset: base address offset needing alignment
 *
 * Aligns the layer 2 function private memory so it's 512-byte aligned.
 **/
static u64 i40e_align_l2obj_base(u64 offset)
{
	u64 aligned_offset = offset;

	if ((offset % I40E_HMC_L2OBJ_BASE_ALIGNMENT) > 0)
		aligned_offset += (I40E_HMC_L2OBJ_BASE_ALIGNMENT -
				   (offset % I40E_HMC_L2OBJ_BASE_ALIGNMENT));

	return aligned_offset;
}

u64 i40e_calculate_l2fpm_size(u32 txq_num, u32 rxq_num)
{
	u64 fpm_size = 0;

	fpm_size = txq_num * I40E_HMC_OBJ_SIZE_TXQ;
	fpm_size = i40e_align_l2obj_base(fpm_size);

	fpm_size += (rxq_num * I40E_HMC_OBJ_SIZE_RXQ);
	fpm_size = i40e_align_l2obj_base(fpm_size);

	return fpm_size;
}

/**
 * i40e_init_lan_hmc - initialize i40e_hmc_info struct
 * @hw: pointer to the HW structure
 * @txq_num: number of Tx queues needing backing context
 * @rxq_num: number of Rx queues needing backing context
 * @fcoe_cntx_num: amount of FCoE statefull contexts needing backing context
 * @fcoe_filt_num: number of FCoE filters needing backing context
 *
 * This function will be called once per physical function initialization.
 * It will fill out the i40e_hmc_obj_info structure for LAN objects based on
 * the driver's provided input, as well as information from the HMC itself
 * loaded from NVRAM.
 *
 * Assumptions:
 *   - HMC Resource Profile has been selected before calling this function.
 **/

int ufp_i40e_init_lan_hmc(struct i40e_hw *hw, u32 txq_num, u32 rxq_num)
{
	struct i40e_hmc_obj_info *obj, *full_obj;
	i40e_status ret_code = I40E_SUCCESS;
	u64 l2fpm_size;
	u32 size_exp;

	hw->hmc.signature = I40E_HMC_INFO_SIGNATURE;
	hw->hmc.hmc_fn_id = hw->pf_id;

	/* allocate memory for hmc_obj */
	ret_code = i40e_allocate_virt_mem(hw, &hw->hmc.hmc_obj_virt_mem,
			sizeof(struct i40e_hmc_obj_info) * I40E_HMC_LAN_MAX);
	if (ret_code)
		goto init_lan_hmc_out;
	hw->hmc.hmc_obj = (struct i40e_hmc_obj_info *)
			  hw->hmc.hmc_obj_virt_mem.va;

	/* The full object will be used to create the LAN HMC SD */
	full_obj = &hw->hmc.hmc_obj[I40E_HMC_LAN_FULL];
	full_obj->max_cnt = 0;
	full_obj->cnt = 0;
	full_obj->base = 0;
	full_obj->size = 0;

	/* Tx queue context information */
	obj = &hw->hmc.hmc_obj[I40E_HMC_LAN_TX];
	obj->max_cnt = rd32(hw, I40E_GLHMC_LANQMAX);
	obj->cnt = txq_num;
	obj->base = 0;
	size_exp = rd32(hw, I40E_GLHMC_LANTXOBJSZ);
	obj->size = BIT_ULL(size_exp);

	/* validate values requested by driver don't exceed HMC capacity */
	if (txq_num > obj->max_cnt) {
		ret_code = I40E_ERR_INVALID_HMC_OBJ_COUNT;
		hw_dbg(hw, "i40e_init_lan_hmc: Tx context: asks for 0x%x but max allowed is 0x%x, returns error %d\n",
			  txq_num, obj->max_cnt, ret_code);
		goto init_lan_hmc_out;
	}

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
	obj->base = i40e_align_l2obj_base(obj->base);
	size_exp = rd32(hw, I40E_GLHMC_LANRXOBJSZ);
	obj->size = BIT_ULL(size_exp);

	/* validate values requested by driver don't exceed HMC capacity */
	if (rxq_num > obj->max_cnt) {
		ret_code = I40E_ERR_INVALID_HMC_OBJ_COUNT;
		hw_dbg(hw, "i40e_init_lan_hmc: Rx context: asks for 0x%x but max allowed is 0x%x, returns error %d\n",
			  rxq_num, obj->max_cnt, ret_code);
		goto init_lan_hmc_out;
	}

	/* aggregate values into the full LAN object for later */
	full_obj->max_cnt += obj->max_cnt;
	full_obj->cnt += obj->cnt;

	hw->hmc.first_sd_index = 0;
	hw->hmc.sd_table.ref_cnt = 0;
	l2fpm_size = i40e_calculate_l2fpm_size(txq_num, rxq_num);
	if (NULL == hw->hmc.sd_table.sd_entry) {
		hw->hmc.sd_table.sd_cnt = (u32)
				   (l2fpm_size + I40E_HMC_DIRECT_BP_SIZE - 1) /
				   I40E_HMC_DIRECT_BP_SIZE;

		/* allocate the sd_entry members in the sd_table */
		ret_code = i40e_allocate_virt_mem(hw, &hw->hmc.sd_table.addr,
					  (sizeof(struct i40e_hmc_sd_entry) *
					  hw->hmc.sd_table.sd_cnt));
		if (ret_code)
			goto init_lan_hmc_out;
		hw->hmc.sd_table.sd_entry =
			(struct i40e_hmc_sd_entry *)hw->hmc.sd_table.addr.va;
	}
	/* store in the LAN full object for later */
	full_obj->size = l2fpm_size;

init_lan_hmc_out:
	return ret_code;
}

/**
 * i40e_create_lan_hmc_object - allocate backing store for hmc objects
 * @hw: pointer to the HW structure
 * @info: pointer to i40e_hmc_create_obj_info struct
 *
 * This will allocate memory for PDs and backing pages and populate
 * the sd and pd entries.
 **/
i40e_status i40e_create_lan_hmc_object(struct i40e_hw *hw,
				struct i40e_hmc_lan_create_obj_info *info)
{
	i40e_status ret_code = I40E_SUCCESS;
	struct i40e_hmc_sd_entry *sd_entry;
	u32 pd_idx1 = 0, pd_lmt1 = 0;
	u32 pd_idx = 0, pd_lmt = 0;
	bool pd_error = false;
	u32 sd_idx, sd_lmt;
	u64 sd_size;
	u32 i, j;

	if (NULL == info) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_create_lan_hmc_object: bad info ptr\n");
		goto exit;
	}
	if (NULL == info->hmc_info) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_create_lan_hmc_object: bad hmc_info ptr\n");
		goto exit;
	}
	if (I40E_HMC_INFO_SIGNATURE != info->hmc_info->signature) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_create_lan_hmc_object: bad signature\n");
		goto exit;
	}

	if (info->start_idx >= info->hmc_info->hmc_obj[info->rsrc_type].cnt) {
		ret_code = I40E_ERR_INVALID_HMC_OBJ_INDEX;
		hw_dbg(hw, "i40e_create_lan_hmc_object: returns error %d\n",
			  ret_code);
		goto exit;
	}
	if ((info->start_idx + info->count) >
	    info->hmc_info->hmc_obj[info->rsrc_type].cnt) {
		ret_code = I40E_ERR_INVALID_HMC_OBJ_COUNT;
		hw_dbg(hw, "i40e_create_lan_hmc_object: returns error %d\n",
			  ret_code);
		goto exit;
	}

	/* find sd index and limit */
	I40E_FIND_SD_INDEX_LIMIT(info->hmc_info, info->rsrc_type,
				 info->start_idx, info->count,
				 &sd_idx, &sd_lmt);
	if (sd_idx >= info->hmc_info->sd_table.sd_cnt ||
	    sd_lmt > info->hmc_info->sd_table.sd_cnt) {
			ret_code = I40E_ERR_INVALID_SD_INDEX;
			goto exit;
	}


	/* find pd index */
	I40E_FIND_PD_INDEX_LIMIT(info->hmc_info, info->rsrc_type,
				 info->start_idx, info->count, &pd_idx,
				 &pd_lmt);

	/* This is to cover for cases where you may not want to have an SD with
	 * the full 2M memory but something smaller. By not filling out any
	 * size, the function will default the SD size to be 2M.
	 */
	if (info->direct_mode_sz == 0)
		sd_size = I40E_HMC_DIRECT_BP_SIZE;
	else
		sd_size = info->direct_mode_sz;

	/* check if all the sds are valid. If not, allocate a page and
	 * initialize it.
	 */
	for (j = sd_idx; j < sd_lmt; j++) {
		/* update the sd table entry */
		ret_code = i40e_add_sd_table_entry(hw, info->hmc_info, j,
						   info->entry_type,
						   sd_size);
		if (I40E_SUCCESS != ret_code)
			goto exit_sd_error;

		sd_entry = &info->hmc_info->sd_table.sd_entry[j];
		if (!sd_entry->valid) {
			sd_entry->valid = true;
			switch (sd_entry->entry_type) {
			case I40E_SD_TYPE_DIRECT:
				I40E_SET_PF_SD_ENTRY(hw, sd_entry->u.bp.addr.pa,
						     j, sd_entry->entry_type);
				break;
			default:
				ret_code = I40E_ERR_INVALID_SD_TYPE;
				goto exit;
			}
		}
	}
	goto exit;

exit_sd_error:
	/* cleanup for sd entries from j to sd_idx */
	while (j && (j > sd_idx)) {
		sd_entry = &info->hmc_info->sd_table.sd_entry[j - 1];
		switch (sd_entry->entry_type) {
		case I40E_SD_TYPE_DIRECT:
			i40e_remove_sd_bp(hw, info->hmc_info, (j - 1));
			break;
		default:
			ret_code = I40E_ERR_INVALID_SD_TYPE;
			break;
		}
		j--;
	}
exit:
	return ret_code;
}

/**
 * i40e_configure_lan_hmc - prepare the HMC backing store
 * @hw: pointer to the hw structure
 * @model: the model for the layout of the SD/PD tables
 *
 * - This function will be called once per physical function initialization.
 * - This function will be called after i40e_init_lan_hmc() and before
 *   any LAN/FCoE HMC objects can be created.
 **/
i40e_status i40e_configure_lan_hmc(struct i40e_hw *hw,
					     enum i40e_hmc_model model)
{
	struct i40e_hmc_lan_create_obj_info info;
	u8 hmc_fn_id = hw->hmc.hmc_fn_id;
	struct i40e_hmc_obj_info *obj;
	i40e_status ret_code = I40E_SUCCESS;

	/* Initialize part of the create object info struct */
	info.hmc_info = &hw->hmc;
	info.rsrc_type = I40E_HMC_LAN_FULL;
	info.start_idx = 0;
	info.direct_mode_sz = hw->hmc.hmc_obj[I40E_HMC_LAN_FULL].size;

	/* Build the SD entry for the LAN objects */
	switch (model) {
	case I40E_HMC_MODEL_DIRECT_PREFERRED:
	case I40E_HMC_MODEL_DIRECT_ONLY:
		info.entry_type = I40E_SD_TYPE_DIRECT;
		/* Make one big object, a single SD */
		info.count = 1;
		ret_code = i40e_create_lan_hmc_object(hw, &info);
		if (ret_code != I40E_SUCCESS)
			goto configure_lan_hmc_out;
		/* else clause falls through the break */
		break;
	default:
		/* unsupported type */
		ret_code = I40E_ERR_INVALID_SD_TYPE;
		hw_dbg(hw, "i40e_configure_lan_hmc: Unknown SD type: %d\n",
			  ret_code);
		goto configure_lan_hmc_out;
	}

	/* Configure and program the FPM registers so objects can be created */

	/* Tx contexts */
	obj = &hw->hmc.hmc_obj[I40E_HMC_LAN_TX];
	wr32(hw, I40E_GLHMC_LANTXBASE(hmc_fn_id),
	     (u32)((obj->base & I40E_GLHMC_LANTXBASE_FPMLANTXBASE_MASK) / 512));
	wr32(hw, I40E_GLHMC_LANTXCNT(hmc_fn_id), obj->cnt);

	/* Rx contexts */
	obj = &hw->hmc.hmc_obj[I40E_HMC_LAN_RX];
	wr32(hw, I40E_GLHMC_LANRXBASE(hmc_fn_id),
	     (u32)((obj->base & I40E_GLHMC_LANRXBASE_FPMLANRXBASE_MASK) / 512));
	wr32(hw, I40E_GLHMC_LANRXCNT(hmc_fn_id), obj->cnt);

configure_lan_hmc_out:
	return ret_code;
}

/**
 * i40e_delete_hmc_object - remove hmc objects
 * @hw: pointer to the HW structure
 * @info: pointer to i40e_hmc_delete_obj_info struct
 *
 * This will de-populate the SDs and PDs.  It frees
 * the memory for PDS and backing storage.  After this function is returned,
 * caller should deallocate memory allocated previously for
 * book-keeping information about PDs and backing storage.
 **/
i40e_status i40e_delete_lan_hmc_object(struct i40e_hw *hw,
				struct i40e_hmc_lan_delete_obj_info *info)
{
	i40e_status ret_code = I40E_SUCCESS;
	struct i40e_hmc_pd_table *pd_table;
	u32 pd_idx, pd_lmt, rel_pd_idx;
	u32 sd_idx, sd_lmt;
	u32 i, j;

	if (NULL == info) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_delete_hmc_object: bad info ptr\n");
		goto exit;
	}
	if (NULL == info->hmc_info) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_delete_hmc_object: bad info->hmc_info ptr\n");
		goto exit;
	}
	if (I40E_HMC_INFO_SIGNATURE != info->hmc_info->signature) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_delete_hmc_object: bad hmc_info->signature\n");
		goto exit;
	}

	if (NULL == info->hmc_info->sd_table.sd_entry) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_delete_hmc_object: bad sd_entry\n");
		goto exit;
	}

	if (NULL == info->hmc_info->hmc_obj) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_delete_hmc_object: bad hmc_info->hmc_obj\n");
		goto exit;
	}
	if (info->start_idx >= info->hmc_info->hmc_obj[info->rsrc_type].cnt) {
		ret_code = I40E_ERR_INVALID_HMC_OBJ_INDEX;
		hw_dbg(hw, "i40e_delete_hmc_object: returns error %d\n",
			  ret_code);
		goto exit;
	}

	if ((info->start_idx + info->count) >
	    info->hmc_info->hmc_obj[info->rsrc_type].cnt) {
		ret_code = I40E_ERR_INVALID_HMC_OBJ_COUNT;
		hw_dbg(hw, "i40e_delete_hmc_object: returns error %d\n",
			  ret_code);
		goto exit;
	}

	/* find sd index and limit */
	I40E_FIND_SD_INDEX_LIMIT(info->hmc_info, info->rsrc_type,
				 info->start_idx, info->count,
				 &sd_idx, &sd_lmt);
	if (sd_idx >= info->hmc_info->sd_table.sd_cnt ||
	    sd_lmt > info->hmc_info->sd_table.sd_cnt) {
		ret_code = I40E_ERR_INVALID_SD_INDEX;
		goto exit;
	}

	for (i = sd_idx; i < sd_lmt; i++) {
		if (!info->hmc_info->sd_table.sd_entry[i].valid)
			continue;
		switch (info->hmc_info->sd_table.sd_entry[i].entry_type) {
		case I40E_SD_TYPE_DIRECT:
			ret_code = i40e_remove_sd_bp(hw, info->hmc_info, i);
			if (I40E_SUCCESS != ret_code)
				goto exit;
			break;
		default:
			break;
		}
	}
exit:
	return ret_code;
}

/**
 * i40e_shutdown_lan_hmc - Remove HMC backing store, free allocated memory
 * @hw: pointer to the hw structure
 *
 * This must be called by drivers as they are shutting down and being
 * removed from the OS.
 **/
i40e_status i40e_shutdown_lan_hmc(struct i40e_hw *hw)
{
	struct i40e_hmc_lan_delete_obj_info info;
	i40e_status ret_code;

	info.hmc_info = &hw->hmc;
	info.rsrc_type = I40E_HMC_LAN_FULL;
	info.start_idx = 0;
	info.count = 1;

	/* delete the object */
	ret_code = i40e_delete_lan_hmc_object(hw, &info);

	/* free the SD table entry for LAN */
	i40e_free_virt_mem(hw, &hw->hmc.sd_table.addr);
	hw->hmc.sd_table.sd_cnt = 0;
	hw->hmc.sd_table.sd_entry = NULL;

	/* free memory used for hmc_obj */
	i40e_free_virt_mem(hw, &hw->hmc.hmc_obj_virt_mem);
	hw->hmc.hmc_obj = NULL;

	return ret_code;
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

/**
 * i40e_hmc_get_object_va - retrieves an object's virtual address
 * @hw: pointer to the hw structure
 * @object_base: pointer to u64 to get the va
 * @rsrc_type: the hmc resource type
 * @obj_idx: hmc object index
 *
 * This function retrieves the object's virtual address from the object
 * base pointer.  This function is used for LAN Queue contexts.
 **/
static
i40e_status i40e_hmc_get_object_va(struct i40e_hw *hw,
					u8 **object_base,
					enum i40e_hmc_lan_rsrc_type rsrc_type,
					u32 obj_idx)
{
	u32 obj_offset_in_sd, obj_offset_in_pd;
	struct i40e_hmc_info     *hmc_info = &hw->hmc;
	struct i40e_hmc_sd_entry *sd_entry;
	struct i40e_hmc_pd_entry *pd_entry;
	u32 pd_idx, pd_lmt, rel_pd_idx;
	i40e_status ret_code = I40E_SUCCESS;
	u64 obj_offset_in_fpm;
	u32 sd_idx, sd_lmt;

	if (NULL == hmc_info) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_hmc_get_object_va: bad hmc_info ptr\n");
		goto exit;
	}
	if (NULL == hmc_info->hmc_obj) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_hmc_get_object_va: bad hmc_info->hmc_obj ptr\n");
		goto exit;
	}
	if (NULL == object_base) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_hmc_get_object_va: bad object_base ptr\n");
		goto exit;
	}
	if (I40E_HMC_INFO_SIGNATURE != hmc_info->signature) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_hmc_get_object_va: bad hmc_info->signature\n");
		goto exit;
	}
	if (obj_idx >= hmc_info->hmc_obj[rsrc_type].cnt) {
		hw_dbg(hw, "i40e_hmc_get_object_va: returns error %d\n",
			  ret_code);
		ret_code = I40E_ERR_INVALID_HMC_OBJ_INDEX;
		goto exit;
	}
	/* find sd index and limit */
	I40E_FIND_SD_INDEX_LIMIT(hmc_info, rsrc_type, obj_idx, 1,
				 &sd_idx, &sd_lmt);

	sd_entry = &hmc_info->sd_table.sd_entry[sd_idx];
	obj_offset_in_fpm = hmc_info->hmc_obj[rsrc_type].base +
			    hmc_info->hmc_obj[rsrc_type].size * obj_idx;
	obj_offset_in_sd = (u32)(obj_offset_in_fpm %
				 I40E_HMC_DIRECT_BP_SIZE);
	*object_base = (u8 *)sd_entry->u.bp.addr.va + obj_offset_in_sd;
exit:
	return ret_code;
}

/**
 * i40e_add_sd_table_entry - Adds a segment descriptor to the table
 * @hw: pointer to our hw struct
 * @hmc_info: pointer to the HMC configuration information struct
 * @sd_index: segment descriptor index to manipulate
 * @type: what type of segment descriptor we're manipulating
 * @direct_mode_sz: size to alloc in direct mode
 **/
i40e_status i40e_add_sd_table_entry(struct i40e_hw *hw,
					      struct i40e_hmc_info *hmc_info,
					      u32 sd_index,
					      enum i40e_sd_entry_type type,
					      u64 direct_mode_sz)
{
	i40e_status ret_code = I40E_SUCCESS;
	struct i40e_hmc_sd_entry *sd_entry;
	enum   i40e_memory_type mem_type;
	bool dma_mem_alloc_done = false;
	struct i40e_dma_mem mem;
	u64 alloc_len;

	if (NULL == hmc_info->sd_table.sd_entry) {
		ret_code = I40E_ERR_BAD_PTR;
		hw_dbg(hw, "i40e_add_sd_table_entry: bad sd_entry\n");
		goto exit;
	}

	if (sd_index >= hmc_info->sd_table.sd_cnt) {
		ret_code = I40E_ERR_INVALID_SD_INDEX;
		hw_dbg(hw, "i40e_add_sd_table_entry: bad sd_index\n");
		goto exit;
	}

	sd_entry = &hmc_info->sd_table.sd_entry[sd_index];
	if (!sd_entry->valid) {
		if (I40E_SD_TYPE_PAGED == type) {
			mem_type = i40e_mem_pd;
			alloc_len = I40E_HMC_PAGED_BP_SIZE;
		} else {
			mem_type = i40e_mem_bp_jumbo;
			alloc_len = direct_mode_sz;
		}

		/* allocate a 4K pd page or 2M backing page */
		ret_code = i40e_allocate_dma_mem(hw, &mem, mem_type, alloc_len,
						 I40E_HMC_PD_BP_BUF_ALIGNMENT);
		if (ret_code)
			goto exit;
		dma_mem_alloc_done = true;
		if (I40E_SD_TYPE_PAGED == type) {
			ret_code = i40e_allocate_virt_mem(hw,
					&sd_entry->u.pd_table.pd_entry_virt_mem,
					sizeof(struct i40e_hmc_pd_entry) * 512);
			if (ret_code)
				goto exit;
			sd_entry->u.pd_table.pd_entry =
				(struct i40e_hmc_pd_entry *)
				sd_entry->u.pd_table.pd_entry_virt_mem.va;
			i40e_memcpy(&sd_entry->u.pd_table.pd_page_addr,
				    &mem, sizeof(struct i40e_dma_mem),
				    I40E_NONDMA_TO_NONDMA);
		} else {
			i40e_memcpy(&sd_entry->u.bp.addr,
				    &mem, sizeof(struct i40e_dma_mem),
				    I40E_NONDMA_TO_NONDMA);
			sd_entry->u.bp.sd_pd_index = sd_index;
		}
		/* initialize the sd entry */
		hmc_info->sd_table.sd_entry[sd_index].entry_type = type;

		/* increment the ref count */
		I40E_INC_SD_REFCNT(&hmc_info->sd_table);
	}
	/* Increment backing page reference count */
	if (I40E_SD_TYPE_DIRECT == sd_entry->entry_type)
		I40E_INC_BP_REFCNT(&sd_entry->u.bp);
exit:
	if (I40E_SUCCESS != ret_code)
		if (dma_mem_alloc_done)
			i40e_free_dma_mem(hw, &mem);

	return ret_code;
}

/**
 * i40e_remove_sd_bp_new - Removes a backing page from a segment descriptor
 * @hw: pointer to our hw struct
 * @hmc_info: pointer to the HMC configuration information structure
 * @idx: the page index
 * @is_pf: used to distinguish between VF and PF
 **/
i40e_status i40e_remove_sd_bp(struct i40e_hw *hw,
	struct i40e_hmc_info *hmc_info, u32 idx)
{
	struct i40e_hmc_sd_entry *sd_entry;

	/* get the entry and decrease its ref counter */
	sd_entry = &hmc_info->sd_table.sd_entry[idx];
	I40E_DEC_BP_REFCNT(&sd_entry->u.bp);
	if (sd_entry->u.bp.ref_cnt) {
		goto err_not_ready;
	}
	I40E_DEC_SD_REFCNT(&hmc_info->sd_table);

	/* mark the entry invalid */
	sd_entry->valid = false;

	I40E_CLEAR_PF_SD_ENTRY(hw, idx, I40E_SD_TYPE_DIRECT);
	return i40e_free_dma_mem(hw, &(sd_entry->u.bp.addr));

err_not_ready:
	return I40E_ERR_NOT_READY;
}
