int i40e_aq_cmd_xmit_macaddr(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct ufp_i40e_page *buf;
	int err;

	buf = ufp_i40e_page_alloc(ih);
	if(!buf)
		goto err_page_alloc;

	err = i40e_aq_asq_xmit(ih, i40e_aqc_opc_mac_address_read,
		buf, sizeof(struct i40e_aq_cmd_macaddr));
	if(err < 0)
		goto err_xmit;

	data->aq_flag &= ~AQ_MAC_ADDR;
	return 0;

err_xmit_fail:
err_read:
err_xmit:
	ufp_i40e_page_release(buf);
err_page_alloc:
	return -1;
}

int i40e_aq_cmd_read_macaddr(struct ufp_handle *ih,
	struct i40e_aq_cmd_macaddr *result)
{
	struct ufp_i40e_data *data = ih->ops->data;

	if(data->aq_flag & AQ_MAC_ADDR)
		goto err_already;

	memcpy(ih->mac_addr, result->port_mac, ETH_ALEN);
	data->aq_flag |= AQ_MAC_ADDR;

	return 0;

err_already:
	return -1;
}
