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

static void i40e_setup_pf_switch_element(struct i40e_pf *pf,
                                struct i40e_aqc_switch_config_element_resp *ele,
                                u16 num_reported)
{
        u16 downlink_seid = le16_to_cpu(ele->downlink_seid);
        u16 uplink_seid = le16_to_cpu(ele->uplink_seid);
        u8 element_type = ele->element_type;
        u16 seid = le16_to_cpu(ele->seid);

        switch (element_type) {
        case I40E_SWITCH_ELEMENT_TYPE_MAC:
                pf->mac_seid = seid;
                break;
        case I40E_SWITCH_ELEMENT_TYPE_VSI:
                if (num_reported != 1)
                        break;
                /* This is immediately after a reset so we can assume this is
                 * the PF's VSI
                 */
                pf->mac_seid = uplink_seid;
                pf->pf_seid = downlink_seid;
                pf->main_vsi_seid = seid;
                if (printconfig)
                        dev_info(&pf->pdev->dev,
                                 "pf_seid=%d main_vsi_seid=%d\n",
                                 pf->pf_seid, pf->main_vsi_seid);
                break;
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
                dev_info(&pf->pdev->dev, "unknown element type=%d seid=%d\n",
                         element_type, seid);
                break;
        }
}

