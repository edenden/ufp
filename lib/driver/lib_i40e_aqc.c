int i40e_aqc_req_macaddr(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_macaddr cmd;
	uint16_t flags;
	int err;

	flags = I40E_AQ_FLAG_BUF;

	err = i40e_aq_asq_assign(dev, i40e_aqc_opc_mac_address_read, flags
		&cmd, sizeof(struct i40e_aq_cmd_macaddr),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_MAC_ADDR;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_macaddr(struct ufp_dev *dev,
	struct i40e_aq_cmd_macaddr *cmd, struct i40e_aq_buf_macaddr *buf)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;

	if(!(cmd->command_flags & I40E_AQC_PORT_ADDR_VALID))
		goto err_invalid;

	memcpy(dev->mac_addr, result->port_mac, ETH_ALEN);
	i40e_dev->aq.flag &= ~AQ_MAC_ADDR;

	return 0;

err_invalid:
	return -1;
}

int i40e_aqc_req_pxeclear(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_clear_pxe cmd;
	int err;

	cmd->rx_cnt = 0x2;

	err = i40e_aq_asq_assign(dev, i40e_aqc_opc_clear_pxe_mode, 0, 
		&cmd, sizeof(struct i40e_aq_cmd_clear_pxe),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_CLEAR_PXE;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_pxeclear(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;

	wr32(hw, I40E_GLLAN_RCTL_0, 0x1);
	i40e_dev->aq.flag &= ~AQ_CLEAR_PXE;

	return 0;
}

int i40e_aqc_req_getconf(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_getconf cmd;
	uint16_t flags;
	int err;

	cmd.seid_offset = CPU_TO_LE16(i40e_dev->aq_seid_offset);
	flags = I40E_AQ_FLAG_BUF;

	err = i40e_aq_asq_assign(dev, i40e_aqc_opc_get_switch_config, flags,
		&cmd, sizeof(struct i40e_aq_cmd_getconf),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_GET_CONF;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_getconf(struct ufp_dev *dev,
	struct i40e_aq_cmd_getconf *cmd, struct i40e_aqc_get_switch_config_resp *buf)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aqc_switch_config_element_resp *elem_resp;

	num_reported = le16_to_cpu(buf->header.num_reported);
	for (i = 0; i < num_reported; i++) {
		struct switch_elem *elem;

		elem_resp = buf->element[i];

		elem->type = elem_resp->element_type;
		elem->seid = le16_to_cpu(elem_resp->seid);
		elem->seid_uplink = le16_to_cpu(elem_resp->uplink_seid);
		elem->seid_downlink = le16_to_cpu(elem_resp->downlink_seid);

		add_conf_to_list(i40e_dev->switch_elem, elem);
	}

	i40e_dev->aq.flag &= ~AQ_GET_CONF;
	i40e_dev->aq_seid_offset = cmd->seid_offset;

	return 0;
}

int i40e_aqc_req_setconf(struct ufp_dev *dev,
	uint16_t flags, uint16_t valid_flags)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aqc_set_switch_config cmd;
	int err;

	cmd.flags = CPU_TO_LE16(flags);
	cmd.valid_flags = CPU_TO_LE16(valid_flags);

	err = i40e_aq_asq_assign(dev, i40e_aqc_opc_set_switch_config, 0,
		&cmd, sizeof(struct i40e_aqc_set_switch_config),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_SET_CONF;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_setconf(struct ufp_dev *dev,
	struct i40e_aq_cmd_clear_pxe *cmd)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_SET_CONF;
	return 0;
}

int i40e_aqc_req_setrsskey(struct ufp_dev *dev,
	struct ufp_i40e_vsi *vsi, uint8_t *key, uint16_t key_size)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aqc_get_set_rss_key cmd;
	uint16_t flags;
	int err;

	cmd.vsi_id = CPU_TO_LE16((u16)
		((vsi->id << I40E_AQC_SET_RSS_KEY_VSI_ID_SHIFT) &
		I40E_AQC_SET_RSS_KEY_VSI_ID_MASK));
	cmd.vsi_id |= CPU_TO_LE16((u16)I40E_AQC_SET_RSS_KEY_VSI_VALID);
	flags = I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD;

	err = i40e_aq_asq_assign(dev, i40e_aqc_opc_set_rss_key, flags,
		&cmd, sizeof(struct i40e_aqc_get_set_rss_key),
		key, key_size);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_SET_RSSKEY;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_setrsskey(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_SET_RSSKEY;
	return 0;
}

int i40e_aqc_req_setrsslut(struct ufp_dev *dev,
	struct ufp_i40e_vsi *vsi, uint8_t *lut, uint16_t lut_size)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aqc_get_set_rss_lut cmd;
	uint16_t flags;
	int err;

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

	err = i40e_aq_asq_assign(dev, i40e_aqc_opc_set_rss_lut, flags,
		&cmd, sizeof(struct i40e_aqc_get_set_rss_lut),
		lut, lut_size);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_SET_RSSLUT;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_setrsslut(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_SET_RSSLUT;
	return 0;
}

