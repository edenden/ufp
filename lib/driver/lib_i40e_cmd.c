int i40e_aq_cmd_xmit_macaddr(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_aq_cmd_macaddr cmd;
	struct ufp_i40e_page *buf;
	int err;

	buf = ufp_i40e_page_alloc(ih);
	if(!buf)
		goto err_page_alloc;

	err = i40e_aq_asq_xmit(ih, i40e_aqc_opc_mac_address_read,
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
	struct ufp_i40e_page *buf;
	int err;

	buf = ufp_i40e_page_alloc(ih);
	if(!buf)
		goto err_page_alloc;

	cmd->rx_cnt = 0x2;

	err = i40e_aq_asq_xmit(ih, i40e_aqc_opc_clear_pxe_mode,
		&cmd, sizeof(struct i40e_aq_cmd_clear_pxe),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	data->aq_flag &= ~AQ_CLEAR_PXE;
	return 0;

err_xmit:
	ufp_i40e_page_release(buf);
err_page_alloc:
	return -1;
}

int i40e_aq_cmd_clean_pxeclear(struct ufp_handle *ih,
	struct i40e_aq_cmd_clear_pxe *cmd)
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

int i40e_aq_cmd_xmit_getconf(struct ufp_handle *ih, uint16_t seid_offset)
{
        struct ufp_i40e_data *data = ih->ops->data;
	struct i40e_aq_cmd_getconf cmd;
        struct ufp_i40e_page *buf;
        int err;

        buf = ufp_i40e_page_alloc(ih);
        if(!buf)
                goto err_page_alloc;

	cmd.seid_offset = CPU_TO_LE16(seid_offset);

        err = i40e_aq_asq_xmit(ih, i40e_aqc_opc_get_switch_config, 
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

        if(data->aq_flag & AQ_GET_CONF)
                goto err_already;

	num_reported = le16_to_cpu(buf->header.num_reported);
	for (i = 0; i < num_reported; i++) {
		struct i40e_aqc_switch_config_element_resp *ele =
			&sw_config->element[i];
		i40e_setup_pf_switch_element(pf, ele, num_reported, printconfig);
	}

	data->aq_flag |= AQ_GET_CONF;
	data->aq_getconf_offset = cmd->seid_offset;

        return 0;

err_already:
        return -1;
}

int i40e_fetch_switch_configuration(struct i40e_pf *pf, bool printconfig)
{
        struct i40e_aqc_get_switch_config_resp *sw_config;
        u16 next_seid = 0;
        int ret = 0;
        u8 *aq_buf;
        int i;

        aq_buf = kzalloc(I40E_AQ_LARGE_BUF, GFP_KERNEL);
        if (!aq_buf)
                return -ENOMEM;

        sw_config = (struct i40e_aqc_get_switch_config_resp *)aq_buf;
        do {
                u16 num_reported, num_total;

                ret = i40e_aq_get_switch_config(&pf->hw, sw_config,
                                                I40E_AQ_LARGE_BUF,
                                                &next_seid, NULL);
                if (ret) {
                        dev_info(&pf->pdev->dev,
                                 "get switch config failed err %s aq_err %s\n",
                                 i40e_stat_str(&pf->hw, ret),
                                 i40e_aq_str(&pf->hw,
                                              pf->hw.aq.asq_last_status));
                        kfree(aq_buf);
                        return -ENOENT;
                }

                num_reported = le16_to_cpu(sw_config->header.num_reported);
                num_total = le16_to_cpu(sw_config->header.num_total);

                if (printconfig)
                        dev_info(&pf->pdev->dev,
                                 "header: %d reported %d total\n",
                                 num_reported, num_total);

                for (i = 0; i < num_reported; i++) {
                        struct i40e_aqc_switch_config_element_resp *ele =
                                &sw_config->element[i];

                        i40e_setup_pf_switch_element(pf, ele, num_reported,
                                                     printconfig);
                }
        } while (next_seid != 0);

        kfree(aq_buf);
        return ret;
}

static void i40e_setup_pf_switch_element(struct i40e_pf *pf,
	struct i40e_aqc_switch_config_element_resp *ele)
{
        u16 downlink_seid = le16_to_cpu(ele->downlink_seid);
        u16 uplink_seid = le16_to_cpu(ele->uplink_seid);
        u8 element_type = ele->element_type;
        u16 seid = le16_to_cpu(ele->seid);

        switch (element_type) {
        case I40E_SWITCH_ELEMENT_TYPE_VSI:
		/* By default, only 1 MAIN VSI exsits in HW switch */
		struct vsi *vsi;
		vsi = add_vsi_to_list();

		vsi->seid = seid;
		vsi->uplink_seid = uplink_seid;
		vsi->downlink_seid = downlink_seid;
		break;
        case I40E_SWITCH_ELEMENT_TYPE_MAC:
        case I40E_SWITCH_ELEMENT_TYPE_VEB:
        case I40E_SWITCH_ELEMENT_TYPE_PF:
        case I40E_SWITCH_ELEMENT_TYPE_VF:
        case I40E_SWITCH_ELEMENT_TYPE_EMP:
        case I40E_SWITCH_ELEMENT_TYPE_BMC:
        case I40E_SWITCH_ELEMENT_TYPE_PE:
        case I40E_SWITCH_ELEMENT_TYPE_PA:
                /* ignore these for now */
		break;
        default:
                break;
        }
}

