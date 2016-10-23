int i40e_aq_cmd_xmit_macaddr(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_aq_cmd_macaddr cmd;
	struct ufp_i40e_page *buf;
	uint16_t flags;
	int err;

	buf = ufp_i40e_page_alloc(ih);
	if(!buf)
		goto err_page_alloc;

	flags = I40E_AQ_FLAG_BUF;

	err = i40e_aq_asq_xmit(ih, i40e_aqc_opc_mac_address_read, flags
		&cmd, sizeof(struct i40e_aq_cmd_macaddr),
		buf, sizeof(struct i40e_aq_buf_macaddr));
	if(err < 0)
		goto err_xmit;

	data->aq_flag &= ~AQ_MAC_ADDR;
	return 0;

err_xmit:
	ufp_i40e_page_release(buf);
err_page_alloc:
	return -1;
}

int i40e_aq_cmd_clean_macaddr(struct ufp_handle *ih,
	struct i40e_aq_cmd_macaddr *cmd, struct i40e_aq_buf_macaddr *buf)
{
	struct ufp_i40e_data *data = ih->ops->data;

	if(data->aq_flag & AQ_MAC_ADDR)
		goto err_already;

	if(!(cmd->command_flags & I40E_AQC_PORT_ADDR_VALID))
		goto err_invalid;

	memcpy(ih->mac_addr, result->port_mac, ETH_ALEN);
	data->aq_flag |= AQ_MAC_ADDR;

	return 0;

err_invalid:
err_already:
	return -1;
}

int i40e_aq_cmd_xmit_pxeclear(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_aq_cmd_clear_pxe cmd;
	int err;

	cmd->rx_cnt = 0x2;

	err = i40e_aq_asq_xmit(ih, i40e_aqc_opc_clear_pxe_mode, 0, 
		&cmd, sizeof(struct i40e_aq_cmd_clear_pxe),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	data->aq_flag &= ~AQ_CLEAR_PXE;
	return 0;

err_xmit:
	return -1;
}

int i40e_aq_cmd_clean_pxeclear(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;

	if(data->aq_flag & AQ_CLEAR_PXE)
		goto err_already;

	wr32(hw, I40E_GLLAN_RCTL_0, 0x1);
	data->aq_flag |= AQ_CLEAR_PXE;

	return 0;

err_already:
	return -1;
}

int i40e_aq_cmd_xmit_getconf(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_aq_cmd_getconf cmd;
	struct ufp_i40e_page *buf;
	uint16_t flags;
	int err;

	buf = ufp_i40e_page_alloc(ih);
	if(!buf)
		goto err_page_alloc;

	cmd.seid_offset = CPU_TO_LE16(data->aq_seid_offset);
	flags = I40E_AQ_FLAG_BUF;

	err = i40e_aq_asq_xmit(ih, i40e_aqc_opc_get_switch_config, flags,
		&cmd, sizeof(struct i40e_aq_cmd_getconf),
		buf, I40E_AQ_LARGE_BUF);
	if(err < 0)
		goto err_xmit;

	data->aq_flag &= ~AQ_GET_CONF;
	return 0;

err_xmit:
	ufp_i40e_page_release(buf);
err_page_alloc:
	return -1;
}

int i40e_aq_cmd_clean_getconf(struct ufp_handle *ih,
	struct i40e_aq_cmd_getconf *cmd, struct i40e_aqc_get_switch_config_resp *buf)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_aqc_switch_config_element_resp *elem_resp;

	if(data->aq_flag & AQ_GET_CONF)
		goto err_already;

	num_reported = le16_to_cpu(buf->header.num_reported);
	for (i = 0; i < num_reported; i++) {
		struct switch_elem *elem;

		elem_resp = buf->element[i];

		elem->type = elem_resp->element_type;
		elem->seid = le16_to_cpu(elem_resp->seid);
		elem->seid_uplink = le16_to_cpu(elem_resp->uplink_seid);
		elem->seid_downlink = le16_to_cpu(elem_resp->downlink_seid);

		add_conf_to_list(data->switch_elem, elem);
	}

	data->aq_flag |= AQ_GET_CONF;
	data->aq_seid_offset = cmd->seid_offset;

	return 0;

err_already:
	return -1;
}

int i40e_aq_cmd_xmit_setconf(struct ufp_handle *ih,
	uint16_t flags, uint16_t valid_flags)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_aqc_set_switch_config cmd;
	int err;

	cmd.flags = CPU_TO_LE16(flags);
	cmd.valid_flags = CPU_TO_LE16(valid_flags);

	err = i40e_aq_asq_xmit(ih, i40e_aqc_opc_set_switch_config, 0,
		&cmd, sizeof(struct i40e_aqc_set_switch_config),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	data->aq_flag &= ~AQ_SET_CONF;
	return 0;

err_xmit:
	return -1;
}

int i40e_aq_cmd_clean_setconf(struct ufp_handle *ih,
	struct i40e_aq_cmd_clear_pxe *cmd)
{
	struct ufp_i40e_data *data = ih->ops->data;

	if(data->aq_flag & AQ_SET_CONF)
		goto err_already;

	data->aq_flag |= AQ_SET_CONF;

	return 0;

err_already:
	return -1;
}

int i40e_aq_cmd_xmit_setrsskey(struct ufp_handle *ih,
	struct ufp_i40e_vsi *vsi, uint8_t *key, uint16_t key_size)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_aqc_get_set_rss_key cmd;
	struct ufp_i40e_page *buf;
	uint16_t flags;
	int err;

	buf = ufp_i40e_page_alloc(ih);
	if(!buf)
		goto err_page_alloc;

	cmd.vsi_id = CPU_TO_LE16((u16)
		((vsi->id << I40E_AQC_SET_RSS_KEY_VSI_ID_SHIFT) &
		I40E_AQC_SET_RSS_KEY_VSI_ID_MASK));
	cmd.vsi_id |= CPU_TO_LE16((u16)I40E_AQC_SET_RSS_KEY_VSI_VALID);
	flags = I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD;

	memcpy(buf->addr_virt, key, key_size);

	err = i40e_aq_asq_xmit(ih, i40e_aqc_opc_set_rss_key, flags,
		&cmd, sizeof(struct i40e_aqc_get_set_rss_key),
		buf, key_size);
	if(err < 0)
		goto err_xmit;

	data->aq_flag &= ~AQ_SET_RSSKEY;
	return 0;

err_xmit:
	ufp_i40e_page_release(buf);
err_page_alloc:
	return -1;
}

int i40e_aq_cmd_clean_setrsskey(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;

	if(data->aq_flag & AQ_SET_RSSKEY)
		goto err_already;

	data->aq_flag |= AQ_SET_RSSKEY;
	return 0;

err_already:
	return -1;
}

int i40e_aq_cmd_xmit_setrsslut(struct ufp_handle *ih,
	struct ufp_i40e_vsi *vsi, uint8_t *lut, uint16_t lut_size)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_aqc_get_set_rss_lut cmd;
	struct ufp_i40e_page *buf;
	uint16_t flags;
	int err;

	buf = ufp_i40e_page_alloc(ih);
	if(!buf)
		goto err_page_alloc;

	cmd.vsi_id = CPU_TO_LE16((u16)
		((vsi->id << I40E_AQC_SET_RSS_LUT_VSI_ID_SHIFT) &
		I40E_AQC_SET_RSS_LUT_VSI_ID_MASK));
	cmd.vsi_id |= CPU_TO_LE16((u16)I40E_AQC_SET_RSS_LUT_VSI_VALID);

	if(vsi->type == VSI_TYPE_MAIN){
		cmd.flags |= CPU_TO_LE16((u16)
			((I40E_AQC_SET_RSS_LUT_TABLE_TYPE_PF <<
			I40E_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT) &
			I40E_AQC_SET_RSS_LUT_TABLE_TYPE_MASK));
	}else{
		cmd.flags |= CPU_TO_LE16((u16)
			((I40E_AQC_SET_RSS_LUT_TABLE_TYPE_VSI <<
			I40E_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT) &
			I40E_AQC_SET_RSS_LUT_TABLE_TYPE_MASK));
	}

	flags = I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD;

	memcpy(buf->addr_virt, lut, lut_size);

	err = i40e_aq_asq_xmit(ih, i40e_aqc_opc_set_rss_lut, flags,
		&cmd, sizeof(struct i40e_aqc_get_set_rss_lut),
		buf, lut_size);
	if(err < 0)
		goto err_xmit;

	data->aq_flag &= ~AQ_SET_RSSLUT;
	return 0;

err_xmit:
	ufp_i40e_page_release(buf);
err_page_alloc:
	return -1;
}

int i40e_aq_cmd_clean_setrsslut(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;

	if(data->aq_flag & AQ_SET_RSSLUT)
		goto err_already;

	data->aq_flag |= AQ_SET_RSSLUT;
	return 0;

err_already:
	return -1;
}

