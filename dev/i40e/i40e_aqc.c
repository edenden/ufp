#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <lib_main.h>

#include "i40e_main.h"
#include "i40e_aq.h"
#include "i40e_aqc.h"

void i40e_aqc_req_queue_shutdown(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_queue_shutdown cmd;

	cmd.driver_unloading = htole32(I40E_AQ_DRIVER_UNLOADING);

	i40e_aq_asq_assign(dev, i40e_aq_opc_queue_shutdown, 0,
		&cmd, sizeof(struct i40e_aq_cmd_queue_shutdown),
		NULL, 0);

	i40e_dev->aq.flag |= AQ_QUEUE_SHUTDOWN;
	return;
}

int i40e_aqc_resp_queue_shutdown(struct ufp_dev *dev,
	struct i40e_aq_cmd_queue_shutdown *cmd)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_QUEUE_SHUTDOWN;
	return 0;
}

void i40e_aqc_req_macaddr_read(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_macaddr_read cmd;
	uint16_t flags;

	flags = I40E_AQ_FLAG_BUF;

	i40e_aq_asq_assign(dev, i40e_aq_opc_macaddr_read, flags
		&cmd, sizeof(struct i40e_aq_cmd_macaddr),
		NULL, 0);

	i40e_dev->aq.flag |= AQ_MAC_ADDR;
	return;
}

int i40e_aqc_resp_macaddr_read(struct ufp_dev *dev,
	struct i40e_aq_cmd_macaddr_read *cmd, void *buf)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_buf_macaddr *resp_macaddr;

	resp_macaddr = buf;

	if(!(cmd->command_flags & I40E_AQC_PORT_ADDR_VALID))
		goto err_invalid;

	memcpy(i40e_dev->pf_lan_mac, resp_macaddr->pf_lan_mac, ETH_ALEN);
	i40e_dev->aq.flag &= ~AQ_MAC_ADDR;
	return 0;

err_invalid:
	return -1;
}

void i40e_aqc_req_clear_pxemode(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_clear_pxemode cmd;

	cmd.rx_cnt = 0x2;

	i40e_aq_asq_assign(dev, i40e_aq_opc_clear_pxemode, 0,
		&cmd, sizeof(struct i40e_aq_cmd_clear_pxemode),
		NULL, 0);

	i40e_dev->aq.flag |= AQ_CLEAR_PXE;
	return;
}

int i40e_aqc_resp_clear_pxemode(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	UFP_WRITE32(dev, I40E_GLLAN_RCTL_0, 0x1);
	i40e_dev->aq.flag &= ~AQ_CLEAR_PXE;

	return 0;
}

void i40e_aqc_req_get_swconf(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_get_swconf cmd;
	uint16_t flags;

	cmd.seid_offset = htole16(i40e_dev->aq.seid_offset);
	flags = I40E_AQ_FLAG_BUF;

	i40e_aq_asq_assign(dev, i40e_aq_opc_get_swconf, flags,
		&cmd, sizeof(struct i40e_aq_cmd_getconf),
		NULL, 0);

	i40e_dev->aq.flag |= AQ_GET_CONF;
	return;
}

int i40e_aqc_resp_get_swconf(struct ufp_dev *dev,
	struct i40e_aq_cmd_get_swconf *cmd, void *buf)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_buf_get_swconf_elem *resp_elem;
	struct i40e_elem *elem;

	num_reported = le16_to_cpu(buf->header.num_reported);

	resp_elem = buf + sizeof(struct i40e_aq_buf_get_swconf_header);

	for (i = 0; i < num_reported; i++) {
		elem = malloc(sizeof(struct i40e_elem));
		if(!elem)
			goto err_alloc_elem;

		elem->type = (&resp_elem[i])->element_type;
		elem->seid = le16_to_cpu((&resp_elem[i])->seid);
		elem->seid_uplink = le16_to_cpu((&resp_elem[i])->uplink_seid);
		elem->seid_downlink = le16_to_cpu((&resp_elem[i])->downlink_seid);
		elem->element_info = le16_to_cpu((&resp_elem[i])->element_info);

		list_add_last(&i40e_dev->elem, &elem->list);
	}

	i40e_dev->aq.flag &= ~AQ_GET_CONF;
	i40e_dev->aq.seid_offset = cmd->seid_offset;

	return 0;
}

void i40e_aqc_req_set_swconf(struct ufp_dev *dev,
	uint16_t flags, uint16_t valid_flags)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_set_swconf cmd;

	cmd.flags = htole16(flags);
	cmd.valid_flags = htole16(valid_flags);

	i40e_aq_asq_assign(dev, i40e_aq_opc_set_swconf, 0,
		&cmd, sizeof(struct i40e_aqc_set_switch_config),
		NULL, 0);

	i40e_dev->aq.flag |= AQ_SET_CONF;
	return;
}

int i40e_aqc_resp_set_swconf(struct ufp_dev *dev,
	struct i40e_aq_cmd_set_swconf *cmd)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_SET_CONF;
	return 0;
}

void i40e_aqc_req_rxctl_write(struct ufp_dev *dev,
	uint32_t reg_addr, uint32_t reg_val)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_rxctl_write cmd;

	cmd.address = htole32(reg_addr);
	cmd.value = htole32(reg_val);

	i40e_aq_asq_assign(dev, i40e_aq_opc_rxctl_write, 0,
		&cmd, sizeof(struct i40e_aq_cmd_rxctl_write),
		NULL, 0);

	i40e_dev->aq.flag |= AQ_RXCTL_WRITE;
	return;
}

int i40e_aqc_resp_rxctl_write(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_RXCTL_WRITE;
	return 0;
}

void i40e_aqc_req_rxctl_read(struct ufp_dev *dev,
	uint32_t reg_addr)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_rxctl_read cmd;

	cmd.address = htole32(reg_addr);

	i40e_aq_asq_assign(dev, i40e_aq_opc_rxctl_read, 0,
		&cmd, sizeof(struct i40e_aq_cmd_rxctl_read),
		NULL, 0);

	i40e_dev->aq.flag |= AQ_RXCTL_READ;
	return;
}

int i40e_aqc_resp_rxctl_read(struct ufp_dev *dev,
	struct i40e_aq_cmd_rxctl_read *cmd)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_RXCTL_READ;
	i40e_dev->aq.read_val = le32toh(cmd->value);
	return 0;
}

void i40e_aqc_req_update_vsi(struct ufp_dev *dev, struct ufp_iface *iface,
	struct i40e_aq_buf_vsi_data *data)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aq_cmd_update_vsi cmd;
	uint16_t flags;

	cmd.uplink_seid = htole16(i40e_iface->seid);
	flags = I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD;

	i40e_aq_asq_assign(dev, i40e_aq_opc_update_vsi, flags,
		&cmd, sizeof(struct i40e_aq_cmd_update_vsi),
		data, sizeof(struct i40e_aq_buf_vsi_data));

	i40e_dev->aq.flag |= AQ_UPDATE_VSI;
	return;
}

int i40e_aqc_resp_update_vsi(struct ufp_dev *dev,
	struct i40e_aq_cmd_update_vsi_resp *cmd)
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

void i40e_aqc_req_promisc_mode(struct ufp_dev *dev, struct ufp_iface *iface,
	uint16_t promisc_flags)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aq_cmd_promisc_mode cmd;

	cmd.promiscuous_flags = htole16(promisc_flags);
	cmd.valid_flags = htole16(
		I40E_AQC_SET_VSI_PROMISC_UNICAST |
		I40E_AQC_SET_VSI_PROMISC_MULTICAST |
		I40E_AQC_SET_VSI_PROMISC_BROADCAST);
	cmd.seid = htole16(i40e_iface->seid);

	i40e_aq_asq_assign(dev, i40e_aq_opc_promisc_mode, 0,
		&cmd, sizeof(struct i40e_aqc_set_vsi_promiscuous_modes),
		NULL, 0);

	i40e_dev->aq.flag |= AQ_PROMISC_MODE;
	return;
}

int i40e_aqc_resp_promisc_mode(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_PROMISC_MODE;
	return 0;
}

void i40e_aqc_req_set_phyintmask(struct ufp_dev *dev, uint16_t mask)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_set_phyintmask cmd;

	cmd.event_mask = htole16(mask);

	i40e_aq_asq_assign(dev, i40e_aq_opc_set_phyintmask, 0,
		&cmd, sizeof(struct i40e_aqc_set_phy_int_mask),
		NULL, 0);

	i40e_dev->aq.flag |= AQ_SET_PHYINTMASK;
	return;
}

int i40e_aqc_resp_set_phyintmask(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_SET_PHYINTMASK;
	return 0;
}

void i40e_aqc_req_stop_lldp(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_stop_lldp cmd;
	int err;

	cmd.command |= I40E_AQ_LLDP_AGENT_SHUTDOWN;

	i40e_aq_asq_assign(dev, i40e_aq_opc_stop_lldp, 0,
		&cmd, sizeof(struct i40e_aq_cmd_stop_lldp),
		NULL, 0);

	i40e_dev->aq.flag |= AQ_STOP_LLDP;
	return;
}

int i40e_aqc_resp_stop_lldp(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_STOP_LLDP;
	return 0;
}

void i40e_aqc_req_set_rsskey(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *key, uint16_t key_size)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aq_cmd_set_rsskey cmd;
	uint16_t flags;

	cmd.vsi_id = htole16((uint16_t)
		((i40e_iface->id << I40E_AQC_SET_RSS_KEY_VSI_ID_SHIFT) &
		I40E_AQC_SET_RSS_KEY_VSI_ID_MASK));
	cmd.vsi_id |= htole16((uint16_t)I40E_AQC_SET_RSS_KEY_VSI_VALID);
	flags = I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD;

	i40e_aq_asq_assign(dev, i40e_aq_opc_set_rsskey, flags,
		&cmd, sizeof(struct i40e_aqc_get_set_rss_key),
		key, key_size);

	i40e_dev->aq.flag |= AQ_SET_RSSKEY;
	return;
}

int i40e_aqc_resp_set_rsskey(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_SET_RSSKEY;
	return 0;
}

void i40e_aqc_req_set_rsslut(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *lut, uint16_t lut_size)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aq_cmd_set_rsslut cmd;
	uint16_t flags;

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

	i40e_aq_asq_assign(dev, i40e_aq_opc_set_rsslut, flags,
		&cmd, sizeof(struct i40e_aqc_get_set_rss_lut),
		lut, lut_size);

	i40e_dev->aq.flag |= AQ_SET_RSSLUT;
	return;
}

int i40e_aqc_resp_set_rsslut(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_dev->aq.flag &= ~AQ_SET_RSSLUT;
	return 0;
}
