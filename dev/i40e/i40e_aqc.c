#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <lib_main.h>

#include "i40e_aq.h"
#include "i40e_main.h"
#include "i40e_regs.h"
#include "i40e_aqc.h"

int i40e_aqc_wait_cmd(struct ufp_dev *dev,
	struct i40e_aq_session *session)
{
	unsigned int timeout;

	timeout = I40E_ASQ_CMD_TIMEOUT;
	do{
		usleep(1000);
		i40e_aq_asq_clean(dev);
		if(session->retval != 0xffff)
			break;
	}while(--timeout);
	if(!timeout)
		goto err_timeout;

	return 0;

err_timeout:
	return -1;
}

void i40e_aqc_req_firmware_version(struct ufp_dev *dev,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_firmware_version cmd = {0};

	i40e_aq_asq_assign(dev, i40e_aq_opc_firmware_version, 0,
		&cmd, sizeof(struct i40e_aq_cmd_firmware_version),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_driver_version(struct ufp_dev *dev,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_driver_version cmd = {0};
	char driver_string[32];
	uint16_t flags;

	cmd.driver_major_ver = I40E_DRV_VERSION_MAJOR;
	cmd.driver_minor_ver = I40E_DRV_VERSION_MINOR;
	cmd.driver_build_ver = I40E_DRV_VERSION_BUILD;
	cmd.driver_subbuild_ver = 0;

	snprintf(driver_string, sizeof(driver_string), "%d.%d.%d.%d",
		cmd.driver_major_ver, cmd.driver_minor_ver,
		cmd.driver_build_ver, cmd.driver_subbuild_ver);

	flags = I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD;

	i40e_aq_asq_assign(dev, i40e_aq_opc_driver_version, flags,
		&cmd, sizeof(struct i40e_aq_cmd_driver_version),
		driver_string, strlen(driver_string),
		(uint64_t)session);
	return;
}

void i40e_aqc_req_queue_shutdown(struct ufp_dev *dev,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_queue_shutdown cmd = {0};

	cmd.driver_unloading = htole32(I40E_AQ_DRIVER_UNLOADING);

	i40e_aq_asq_assign(dev, i40e_aq_opc_queue_shutdown, 0,
		&cmd, sizeof(struct i40e_aq_cmd_queue_shutdown),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_macaddr_read(struct ufp_dev *dev,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_macaddr_read cmd = {0};
	uint16_t flags;

	flags = I40E_AQ_FLAG_BUF;

	i40e_aq_asq_assign(dev, i40e_aq_opc_macaddr_read, flags,
		&cmd, sizeof(struct i40e_aq_cmd_macaddr_read),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_clear_pxemode(struct ufp_dev *dev,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_clear_pxemode cmd = {0};

	cmd.rx_cnt = 0x2;

	i40e_aq_asq_assign(dev, i40e_aq_opc_clear_pxemode, 0,
		&cmd, sizeof(struct i40e_aq_cmd_clear_pxemode),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_get_swconf(struct ufp_dev *dev,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_get_swconf cmd = {0};
	uint16_t flags;

	if(session)
		cmd.seid_offset = htole16(session->data.seid_offset);
	else
		cmd.seid_offset = 0;

	flags = I40E_AQ_FLAG_BUF;

	i40e_aq_asq_assign(dev, i40e_aq_opc_get_swconf, flags,
		&cmd, sizeof(struct i40e_aq_cmd_get_swconf),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_set_swconf(struct ufp_dev *dev,
	uint16_t flags, uint16_t valid_flags,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_set_swconf cmd = {0};

	cmd.flags = htole16(flags);
	cmd.valid_flags = htole16(valid_flags);

	i40e_aq_asq_assign(dev, i40e_aq_opc_set_swconf, 0,
		&cmd, sizeof(struct i40e_aq_cmd_set_swconf),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_rxctl_write(struct ufp_dev *dev,
	uint32_t reg_addr, uint32_t reg_val,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_rxctl_write cmd = {0};

	cmd.address = htole32(reg_addr);
	cmd.value = htole32(reg_val);

	i40e_aq_asq_assign(dev, i40e_aq_opc_rxctl_write, 0,
		&cmd, sizeof(struct i40e_aq_cmd_rxctl_write),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_rxctl_read(struct ufp_dev *dev,
	uint32_t reg_addr,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_rxctl_read cmd = {0};

	cmd.address = htole32(reg_addr);

	i40e_aq_asq_assign(dev, i40e_aq_opc_rxctl_read, 0,
		&cmd, sizeof(struct i40e_aq_cmd_rxctl_read),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_update_vsi(struct ufp_dev *dev,
	struct ufp_iface *iface, struct i40e_aq_buf_vsi_data *buf,
	struct i40e_aq_session *session)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aq_cmd_update_vsi cmd = {0};
	uint16_t flags;

	cmd.uplink_seid = htole16(i40e_iface->seid);
	flags = I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD;

	i40e_aq_asq_assign(dev, i40e_aq_opc_update_vsi, flags,
		&cmd, sizeof(struct i40e_aq_cmd_update_vsi),
		buf, sizeof(struct i40e_aq_buf_vsi_data),
		(uint64_t)session);
	return;
}

void i40e_aqc_req_get_vsi(struct ufp_dev *dev,
	struct ufp_iface *iface,
	struct i40e_aq_session *session)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aq_cmd_get_vsi cmd = {0};
	uint16_t flags;

	cmd.uplink_seid = htole16(i40e_iface->seid);
	flags = I40E_AQ_FLAG_BUF;

	i40e_aq_asq_assign(dev, i40e_aq_opc_get_vsi, flags,
		&cmd, sizeof(struct i40e_aq_cmd_get_vsi),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_promisc_mode(struct ufp_dev *dev,
	struct ufp_iface *iface, uint16_t promisc_flags,
	struct i40e_aq_session *session)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aq_cmd_promisc_mode cmd = {0};

	cmd.promiscuous_flags = htole16(promisc_flags);
	cmd.valid_flags = htole16(
		I40E_AQC_SET_VSI_PROMISC_UNICAST |
		I40E_AQC_SET_VSI_PROMISC_MULTICAST |
		I40E_AQC_SET_VSI_PROMISC_BROADCAST);
	cmd.seid = htole16(i40e_iface->seid);

	i40e_aq_asq_assign(dev, i40e_aq_opc_promisc_mode, 0,
		&cmd, sizeof(struct i40e_aq_cmd_promisc_mode),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_set_phyintmask(struct ufp_dev *dev,
	uint16_t mask,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_set_phyintmask cmd = {0};

	cmd.event_mask = htole16(mask);

	i40e_aq_asq_assign(dev, i40e_aq_opc_set_phyintmask, 0,
		&cmd, sizeof(struct i40e_aq_cmd_set_phyintmask),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_stop_lldp(struct ufp_dev *dev,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_stop_lldp cmd = {0};

	cmd.command = I40E_AQ_LLDP_AGENT_SHUTDOWN;

	i40e_aq_asq_assign(dev, i40e_aq_opc_stop_lldp, 0,
		&cmd, sizeof(struct i40e_aq_cmd_stop_lldp),
		NULL, 0, (uint64_t)session);
	return;
}

void i40e_aqc_req_set_rsskey(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *key, uint16_t key_size,
	struct i40e_aq_session *session)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aq_cmd_set_rsskey cmd = {0};
	uint16_t flags;

	cmd.vsi_id = htole16((uint16_t)
		((i40e_iface->id << I40E_AQC_SET_RSS_KEY_VSI_ID_SHIFT) &
		I40E_AQC_SET_RSS_KEY_VSI_ID_MASK));
	cmd.vsi_id |= htole16((uint16_t)I40E_AQC_SET_RSS_KEY_VSI_VALID);
	flags = I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD;

	i40e_aq_asq_assign(dev, i40e_aq_opc_set_rsskey, flags,
		&cmd, sizeof(struct i40e_aq_cmd_set_rsskey),
		key, key_size, (uint64_t)session);
	return;
}

void i40e_aqc_req_set_rsslut(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *lut, uint16_t lut_size,
	struct i40e_aq_session *session)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct i40e_aq_cmd_set_rsslut cmd = {0};
	uint16_t flags;

	cmd.vsi_id = htole16((uint16_t)
		((i40e_iface->id << I40E_AQC_SET_RSS_LUT_VSI_ID_SHIFT) &
		I40E_AQC_SET_RSS_LUT_VSI_ID_MASK));
	cmd.vsi_id |= htole16((uint16_t)I40E_AQC_SET_RSS_LUT_VSI_VALID);

	if(i40e_iface->type == I40E_VSI_MAIN){
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
		&cmd, sizeof(struct i40e_aq_cmd_set_rsslut),
		lut, lut_size, (uint64_t)session);
	return;
}

void i40e_aqc_resp_firmware_version(struct ufp_dev *dev,
	void *cmd_ptr)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_firmware_version *cmd = cmd_ptr;

	i40e_dev->version.fw_major = le16toh(cmd->fw_major);
	i40e_dev->version.fw_minor = le16toh(cmd->fw_minor);
	i40e_dev->version.fw_build = le32toh(cmd->fw_build);
	i40e_dev->version.api_major = le16toh(cmd->api_major);
	i40e_dev->version.api_minor = le16toh(cmd->api_minor);
	return;
}

void i40e_aqc_resp_macaddr_read(struct ufp_dev *dev,
	void *cmd_ptr, void *buf_ptr,
	struct i40e_aq_session *session)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_macaddr_read *cmd = cmd_ptr;
	struct i40e_aq_buf_macaddr_read *buf = buf_ptr;

	if(!(cmd->command_flags & I40E_AQC_PORT_ADDR_VALID))
		goto err_invalid;

	memcpy(i40e_dev->pf_lan_mac, buf->pf_lan_mac, ETH_ALEN);
	return;

err_invalid:
	if(session)
		session->retval = -1;
	return;
}

void i40e_aqc_resp_clear_pxemode(struct ufp_dev *dev)
{
	UFP_WRITE32(dev, I40E_GLLAN_RCTL_0, 0x1);
	return;
}

void i40e_aqc_resp_get_swconf(struct ufp_dev *dev,
	void *cmd_ptr, void *buf_ptr,
	struct i40e_aq_session *session)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_cmd_get_swconf *cmd = cmd_ptr;
	struct i40e_aq_buf_get_swconf_header *buf = buf_ptr;
	struct i40e_aq_buf_get_swconf_elem *buf_elem;
	struct i40e_elem *elem;
	uint16_t num_reported;
	int i;

	num_reported = le16toh(buf->num_reported);
	buf_elem = (struct i40e_aq_buf_get_swconf_elem *)
		((char *)buf_ptr
		+ sizeof(struct i40e_aq_buf_get_swconf_header));

	for (i = 0; i < num_reported; i++) {
		elem = malloc(sizeof(struct i40e_elem));
		if(!elem)
			goto err_alloc_elem;

		elem->type = (&buf_elem[i])->element_type;
		elem->seid = le16toh((&buf_elem[i])->seid);
		elem->seid_uplink = le16toh((&buf_elem[i])->uplink_seid);
		elem->seid_downlink = le16toh((&buf_elem[i])->downlink_seid);
		elem->element_info = le16toh((&buf_elem[i])->element_info);

		list_add_last(&i40e_dev->elem, &elem->list);
	}

	if(session)
		session->data.seid_offset = cmd->seid_offset;
	return;

err_alloc_elem:
	if(session)
		session->retval = -1;
	return;
}

void i40e_aqc_resp_rxctl_read(struct ufp_dev *dev,
	void *cmd_ptr,
	struct i40e_aq_session *session)
{
	struct i40e_aq_cmd_rxctl_read *cmd = cmd_ptr;

	if(session)
		session->data.read_val = le32toh(cmd->value);
	return;
}

void i40e_aqc_resp_get_vsi(struct ufp_dev *dev,
	void *cmd_ptr, void *buf_ptr,
	struct i40e_aq_session *session)
{
	struct ufp_iface *iface;
	struct i40e_iface *i40e_iface, *_i40e_iface;
	struct i40e_aq_cmd_get_vsi_resp *cmd = cmd_ptr;
	struct i40e_aq_buf_vsi_data *buf = buf_ptr;
	int i;

	i40e_iface = NULL;
	list_for_each(&dev->iface, iface, list){
		_i40e_iface = iface->drv_data;
		if(_i40e_iface->seid == cmd->seid){
			i40e_iface = _i40e_iface;
			break;
		}
	}
	if(!i40e_iface)
		goto err_notfound;

	for(i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		i40e_iface->qs_handles[i] = buf->qs_handle[i];
	return;

err_notfound:
	if(session)
		session->retval = -1;
	return;
}

