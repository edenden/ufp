#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "lib_main.h"
#include "i40e_main.h"
#include "i40e_aq.h"
#include "i40e_aqc.h"

int i40e_aqc_req_macaddr_read(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_macaddr cmd;
	uint16_t flags;
	int err;

	flags = I40E_AQ_FLAG_BUF;

	err = i40e_aq_asq_assign(dev, i40e_aq_opc_macaddr_read, flags
		&cmd, sizeof(struct i40e_aq_cmd_macaddr),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_MAC_ADDR;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_macaddr_read(struct ufp_dev *dev,
	struct i40e_aq_cmd_macaddr *cmd, struct i40e_aq_buf_macaddr *buf)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	if(!(cmd->command_flags & I40E_AQC_PORT_ADDR_VALID))
		goto err_invalid;

	memcpy(i40e_dev->pf_lan_mac, result->pf_lan_mac, ETH_ALEN);
	i40e_dev->aq.flag &= ~AQ_MAC_ADDR;
	return 0;

err_invalid:
	return -1;
}

int i40e_aqc_req_clear_pxemode(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_clear_pxe cmd;
	int err;

	cmd.rx_cnt = 0x2;

	err = i40e_aq_asq_assign(dev, i40e_aq_opc_clear_pxemode, 0, 
		&cmd, sizeof(struct i40e_aq_cmd_clear_pxe),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_CLEAR_PXE;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_clear_pxemode(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	UFP_WRITE32(dev, I40E_GLLAN_RCTL_0, 0x1);
	i40e_dev->aq.flag &= ~AQ_CLEAR_PXE;

	return 0;
}

int i40e_aqc_req_stop_lldp(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aqc_lldp_stop cmd;
	int err;

	cmd.command |= I40E_AQ_LLDP_AGENT_SHUTDOWN;

	err = i40e_aq_asq_assign(dev, i40e_aq_opc_stop_lldp, 0,
		&cmd, sizeof(struct i40e_aqc_lldp_stop),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_STOP_LLDP;

err_xmit:
	return -1;
}

int i40e_aqc_resp_stop_lldp(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_STOP_LLDP;
	return 0;
}

int i40e_aqc_req_get_swconf(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_getconf cmd;
	uint16_t flags;
	int err;

	cmd.seid_offset = htole16(i40e_dev->aq_seid_offset);
	flags = I40E_AQ_FLAG_BUF;

	err = i40e_aq_asq_assign(dev, i40e_aq_opc_get_swconf, flags,
		&cmd, sizeof(struct i40e_aq_cmd_getconf),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_GET_CONF;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_get_swconf(struct ufp_dev *dev,
	struct i40e_aq_cmd_getconf *cmd, struct i40e_aqc_get_switch_config_resp *buf)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aqc_switch_config_element_resp *elem_resp;
	struct i40e_elem *elem, *elem_new;

	num_reported = le16_to_cpu(buf->header.num_reported);

	elem = i40e_dev->elem;
	while(elem->next){
		elem = elem->next;
	}

	for (i = 0; i < num_reported; i++) {
		elem_new = malloc(sizeof(struct i40e_elem));
		if(!elem_new)
			goto err_alloc_elem_new;

		elem_resp = buf->element[i];

		elem_new->type = elem_resp->element_type;
		elem_new->seid = le16_to_cpu(elem_resp->seid);
		elem_new->seid_uplink = le16_to_cpu(elem_resp->uplink_seid);
		elem_new->seid_downlink = le16_to_cpu(elem_resp->downlink_seid);
		elem_new->element_info = le16_to_cpu(elem_resp->element_info);

		elem_new->next = NULL;
		elem->next = elem_new; 
		elem = elem_new;
	}

	i40e_dev->aq.flag &= ~AQ_GET_CONF;
	i40e_dev->aq_seid_offset = cmd->seid_offset;

	return 0;
}

int i40e_aqc_req_set_swconf(struct ufp_dev *dev,
	uint16_t flags, uint16_t valid_flags)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aqc_set_switch_config cmd;
	int err;

	cmd.flags = htole16(flags);
	cmd.valid_flags = htole16(valid_flags);

	err = i40e_aq_asq_assign(dev, i40e_aq_opc_set_swconf, 0,
		&cmd, sizeof(struct i40e_aqc_set_switch_config),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_SET_CONF;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_set_swconf(struct ufp_dev *dev,
	struct i40e_aq_cmd_clear_pxe *cmd)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_SET_CONF;
	return 0;
}

int i40e_aqc_req_update_vsi(struct ufp_dev *dev, struct ufp_iface *iface,
	struct i40e_aqc_vsi_properties_data *data)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aqc_add_get_update_vsi cmd;
	uint16_t flags;
	int err;

	cmd.uplink_seid = htole16(i40e_iface->seid);
	flags = I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD;

	err = i40e_aq_asq_assign(dev, i40e_aq_opc_update_vsi, flags,
		&cmd, sizeof(struct i40e_aqc_add_get_update_vsi),
		data, sizeof(struct i40e_aqc_vsi_properties_data));
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_UPDATE_VSI;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_update_vsi(struct ufp_dev *dev,
	struct i40e_aqc_add_get_update_vsi_completion *cmd)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_iface *iface = dev->iface;
	struct i40e_iface *i40e_iface;

	while(iface){
		i40e_iface = iface->drv_data;
		if(i40e_iface->seid == cmd->seid)
			break;
		iface = iface->next;
	}
	if(!iface)
		goto err_notfound;

	for(i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		i40e_iface->qs_handles[i] = cmd->qs_handle[i];

	i40e_dev->aq.flag &= ~AQ_UPDATE_VSI;
	return 0;

err_notfound:
	return -1;
}

int i40e_aqc_req_set_rsskey(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *key, uint16_t key_size)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aqc_get_set_rss_key cmd;
	uint16_t flags;
	int err;

	cmd.vsi_id = htole16((uint16_t)
		((i40e_iface->id << I40E_AQC_SET_RSS_KEY_VSI_ID_SHIFT) &
		I40E_AQC_SET_RSS_KEY_VSI_ID_MASK));
	cmd.vsi_id |= htole16((uint16_t)I40E_AQC_SET_RSS_KEY_VSI_VALID);
	flags = I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD;

	err = i40e_aq_asq_assign(dev, i40e_aq_opc_set_rsskey, flags,
		&cmd, sizeof(struct i40e_aqc_get_set_rss_key),
		key, key_size);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_SET_RSSKEY;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_set_rsskey(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_SET_RSSKEY;
	return 0;
}

int i40e_aqc_req_set_rsslut(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *lut, uint16_t lut_size)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aqc_get_set_rss_lut cmd;
	uint16_t flags;
	int err;

	cmd.vsi_id = htole16((uint16_t)
		((i40e_iface->id << I40E_AQC_SET_RSS_LUT_VSI_ID_SHIFT) &
		I40E_AQC_SET_RSS_LUT_VSI_ID_MASK));
	cmd.vsi_id |= htole16((uint16_t)I40E_AQC_SET_RSS_LUT_VSI_VALID);

	if(vsi->type == VSI_TYPE_MAIN){
		cmd.flags |= htole16((uint16_t)
			((I40E_AQC_SET_RSS_LUT_TABLE_TYPE_PF <<
			I40E_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT) &
			I40E_AQC_SET_RSS_LUT_TABLE_TYPE_MASK));
	}else{
		cmd.flags |= htole16((uint16_t)
			((I40E_AQC_SET_RSS_LUT_TABLE_TYPE_VSI <<
			I40E_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT) &
			I40E_AQC_SET_RSS_LUT_TABLE_TYPE_MASK));
	}

	flags = I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD;

	err = i40e_aq_asq_assign(dev, i40e_aq_opc_set_rsslut, flags,
		&cmd, sizeof(struct i40e_aqc_get_set_rss_lut),
		lut, lut_size);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_SET_RSSLUT;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_set_rsslut(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_SET_RSSLUT;
	return 0;
}

int i40e_aqc_req_set_phyintmask(struct ufp_dev *dev, uint16_t mask)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aqc_set_phy_int_mask cmd;
	int err;

	cmd.event_mask = htole16(mask);

	err = i40e_aq_asq_assign(dev, i40e_aq_opc_set_phyintmask, 0,
		&cmd, sizeof(struct i40e_aqc_set_phy_int_mask),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_SET_PHYINTMASK;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_set_phyintmask(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_SET_PHYINTMASK;
	return 0;
}

int i40e_aqc_req_promisc_mode(struct ufp_dev *dev, struct ufp_iface *iface,
	uint16_t promisc_flags)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aqc_set_vsi_promiscuous_modes cmd;
	int err;

	cmd.promiscuous_flags = htole16(promisc_flags);
	cmd.valid_flags = htole16(
		I40E_AQC_SET_VSI_PROMISC_UNICAST |
		I40E_AQC_SET_VSI_PROMISC_MULTICAST |
		I40E_AQC_SET_VSI_PROMISC_BROADCAST);
	cmd.seid = htole16(i40e_iface->seid);

	err = i40e_aq_asq_assign(dev, i40e_aq_opc_promisc_mode, 0,
		&cmd, sizeof(struct i40e_aqc_set_vsi_promiscuous_modes),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_PROMISC_MODE;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_promisc_mode(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_PROMISC_MODE;
	return 0;
}

int i40e_aqc_req_rxctl_write(struct ufp_dev *dev,
	uint32_t reg_addr, uint32_t reg_val)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aqc_rx_ctl_reg_read_write cmd;
	int err;

	cmd.address = htole32(reg_addr);
	cmd.value = htole32(reg_val);

	err = i40e_aq_asq_assign(dev, i40e_aq_opc_rxctl_write, 0,
		&cmd, sizeof(struct i40e_aqc_rx_ctl_reg_read_write),
		NULL, 0);
	if(err < 0)
		goto err_xmit;

	i40e_dev->aq.flag |= AQ_RXCTL_WRITE;
	return 0;

err_xmit:
	return -1;
}

int i40e_aqc_resp_rxctl_write(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_RXCTL_WRITE;
	return 0;
}
