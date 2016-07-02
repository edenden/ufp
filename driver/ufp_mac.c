#include <linux/types.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/miscdevice.h>
#include <linux/semaphore.h>

#include "ufp_main.h"
#include "ufp_mac.h"

static int32_t ufp_mac_reset(struct ufp_hw *hw);
static int32_t ufp_mac_stop_adapter(struct ufp_hw *hw);
static int32_t ufp_mac_set_rar(struct ufp_hw *hw, uint8_t *addr);
static int32_t ufp_mac_set_vfta(struct ufp_hw *hw, uint32_t vlan, uint32_t vlan_on);
static int32_t ufp_mac_check_mac_link(struct ufp_hw *hw, uint32_t *speed,
	uint32_t *link_up);
static void ufp_mac_set_rlpml(struct ufp_hw *hw, uint16_t max_size);
static int32_t ufp_mac_negotiate_api_version(struct ufp_hw *hw, uint32_t api);
static int32_t ufp_mac_get_queues(struct ufp_hw *hw, uint32_t *num_tcs,
	uint32_t *default_tc);

int ufp_mac_init(struct ufp_hw *hw)
{
	struct ufp_mac_info *mac;

	mac = kzalloc(sizeof(struct ufp_mac_info), GFP_KERNEL);
	if(!mac){
		return -1;
	}

	mac->ops.reset_hw		= ufp_mac_reset;
	mac->ops.stop_adapter		= ufp_mac_stop_adapter;
	mac->ops.negotiate_api		= ufp_mac_negotiate_api_version;
	mac->ops.get_queues		= ufp_mac_get_queues;
	mac->ops.check_link		= ufp_mac_check_mac_link;
	mac->ops.set_rar		= ufp_mac_set_rar;
	mac->ops.update_xcast_mode	= ufp_mac_update_xcast_mode;
	mac->ops.set_vfta		= ufp_mac_set_vfta;
	mac->ops.set_rlpml		= ufp_mac_set_rlpml;

	switch(hw->device_id){
	case IXGBE_DEV_ID_82599_VF:
		mac->type = ixgbe_mac_82599_vf;
		break;
	case IXGBE_DEV_ID_X540_VF:
		mac->type = ixgbe_mac_X540_vf;
		break;
	case IXGBE_DEV_ID_X550_VF:
		mac->type = ixgbe_mac_X550_vf;
		break;
	case IXGBE_DEV_ID_X550EM_X_VF:
		mac->type = ixgbe_mac_X550EM_x_vf;
		break;
	default:
		mac->type = ixgbe_mac_unknown;
		break;
	}

	hw->mac = mac;
	return 0;
}

void ufp_mac_free(struct ufp_hw *hw)
{
	kfree(hw->mac);
	hw->mac = NULL;

	return;
}

static void ufp_mac_clr_reg(struct ufp_hw *hw)
{
	int i;
	uint32_t vfsrrctl;
	uint32_t ufpca_rxctrl;
	uint32_t ufpca_txctrl;

	/* VRSRRCTL default values (BSIZEPACKET = 2048, BSIZEHEADER = 256) */
	vfsrrctl = 0x100 << IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT;
	vfsrrctl |= 0x800 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;

	/* DCA_RXCTRL default value */
	ufpca_rxctrl = IXGBE_DCA_RXCTRL_DESC_RRO_EN |
		       IXGBE_DCA_RXCTRL_DATA_WRO_EN |
		       IXGBE_DCA_RXCTRL_HEAD_WRO_EN;

	/* DCA_TXCTRL default value */
	ufpca_txctrl = IXGBE_DCA_TXCTRL_DESC_RRO_EN |
		       IXGBE_DCA_TXCTRL_DESC_WRO_EN |
		       IXGBE_DCA_TXCTRL_DATA_RRO_EN;

	ufp_write_reg(hw, IXGBE_VFPSRTYPE, 0);

	for (i = 0; i < 7; i++) {
		ufp_write_reg(hw, IXGBE_VFRDH(i), 0);
		ufp_write_reg(hw, IXGBE_VFRDT(i), 0);
		ufp_write_reg(hw, IXGBE_VFRXDCTL(i), 0);
		ufp_write_reg(hw, IXGBE_VFSRRCTL(i), vfsrrctl);
		ufp_write_reg(hw, IXGBE_VFTDH(i), 0);
		ufp_write_reg(hw, IXGBE_VFTDT(i), 0);
		ufp_write_reg(hw, IXGBE_VFTXDCTL(i), 0);
		ufp_write_reg(hw, IXGBE_VFTDWBAH(i), 0);
		ufp_write_reg(hw, IXGBE_VFTDWBAL(i), 0);
		ufp_write_reg(hw, IXGBE_VFDCA_RXCTRL(i), ufpca_rxctrl);
		ufp_write_reg(hw, IXGBE_VFDCA_TXCTRL(i), ufpca_txctrl);
	}

	IXGBE_WRITE_FLUSH(hw);
}

static int32_t ufp_mac_reset(struct ufp_hw *hw)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	uint32_t timeout = IXGBE_VF_INIT_TIMEOUT;
	int32_t ret_val = IXGBE_ERR_INVALID_MAC_ADDR;
	uint32_t msgbuf[IXGBE_VF_PERMADDR_MSG_LEN];
	uint8_t *addr = (uint8_t *)(&msgbuf[1]);

	/* Call adapter stop to disable tx/rx and clear interrupts */
	hw->mac.ops.stop_adapter(hw);

	/* reset the api version */
	hw->api_version = ixgbe_mbox_api_10;

	hw_dbg(hw, "Issuing a function level reset to MAC\n");

	IXGBE_VFWRITE_REG(hw, IXGBE_VFCTRL, IXGBE_CTRL_RST);
	IXGBE_WRITE_FLUSH(hw);

	msleep(50);

	/* we cannot reset while the RSTI / RSTD bits are asserted */
	while (!mbx->ops.check_for_rst(hw, 0) && timeout) {
		timeout--;
		udelay(5);
	}

	if (!timeout)
		return IXGBE_ERR_RESET_FAILED;

	/* Reset VF registers to initial values */
	ufp_mac_clr_reg(hw);

	/* mailbox timeout can now become active */
	mbx->timeout = IXGBE_VF_MBX_INIT_TIMEOUT;

	msgbuf[0] = IXGBE_VF_RESET;
	mbx->ops.write_posted(hw, msgbuf, 1, 0);

	msleep(10);

	/*
	 * set our "perm_addr" based on info provided by PF
	 * also set up the mc_filter_type which is piggy backed
	 * on the mac address in word 3
	 */
	ret_val = mbx->ops.read_posted(hw, msgbuf,
			IXGBE_VF_PERMADDR_MSG_LEN, 0);
	if (ret_val)
		return ret_val;

	if (msgbuf[0] != (IXGBE_VF_RESET | IXGBE_VT_MSGTYPE_ACK) &&
	    msgbuf[0] != (IXGBE_VF_RESET | IXGBE_VT_MSGTYPE_NACK))
		return IXGBE_ERR_INVALID_MAC_ADDR;

	if (msgbuf[0] == (IXGBE_VF_RESET | IXGBE_VT_MSGTYPE_ACK))
		memcpy(hw->mac.perm_addr, addr, IXGBE_ETH_LENGTH_OF_ADDRESS);

	hw->mac.mc_filter_type = msgbuf[IXGBE_VF_MC_TYPE_WORD];

	return ret_val;
}

static int32_t ufp_mac_stop_adapter(struct ufp_hw *hw)
{
	uint32_t reg_val;
	uint16_t i;

	/* Clear interrupt mask to stop from interrupts being generated */
	IXGBE_VFWRITE_REG(hw, IXGBE_VTEIMC, IXGBE_VF_IRQ_CLEAR_MASK);

	/* Clear any pending interrupts, flush previous writes */
	IXGBE_VFREAD_REG(hw, IXGBE_VTEICR);

	/* Disable the transmit unit.  Each queue must be disabled. */
	for (i = 0; i < hw->mac.max_tx_queues; i++)
		IXGBE_VFWRITE_REG(hw, IXGBE_VFTXDCTL(i), IXGBE_TXDCTL_SWFLSH);

	/* Disable the receive unit by stopping each queue */
	for (i = 0; i < hw->mac.max_rx_queues; i++) {
		reg_val = IXGBE_VFREAD_REG(hw, IXGBE_VFRXDCTL(i));
		reg_val &= ~IXGBE_RXDCTL_ENABLE;
		IXGBE_VFWRITE_REG(hw, IXGBE_VFRXDCTL(i), reg_val);
	}
	/* Clear packet split and pool config */
	ufp_write_reg(hw, IXGBE_VFPSRTYPE, 0);

	/* flush all queues disables */
	IXGBE_WRITE_FLUSH(hw);
	msleep(2);

	return 0;
}

static int32_t ufp_mac_set_rar(struct ufp_hw *hw, uint8_t *addr)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	uint32_t msgbuf[3];
	uint8_t *msg_addr = (uint8_t *)(&msgbuf[1]);
	int32_t ret_val;

	memset(msgbuf, 0, 12);
	msgbuf[0] = IXGBE_VF_SET_MAC_ADDR;
	memcpy(msg_addr, addr, 6);
	ret_val = mbx->ops.write_posted(hw, msgbuf, 3, 0);

	if (!ret_val)
		ret_val = mbx->ops.read_posted(hw, msgbuf, 3, 0);

	msgbuf[0] &= ~IXGBE_VT_MSGTYPE_CTS;
	return ret_val;
}

static int32_t ufp_mac_update_xcast_mode(struct ufp_hw *hw, int xcast_mode)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	uint32_t msgbuf[2];
	int32_t err;

	switch (hw->api_version) {
	case ixgbe_mbox_api_12:
		break;
	default:
		return -EOPNOTSUPP;
	}

	msgbuf[0] = IXGBE_VF_UPDATE_XCAST_MODE;
	msgbuf[1] = xcast_mode;

	err = mbx->ops.write_posted(hw, msgbuf, 2, 0);
	if (err)
		return err;

	err = mbx->ops.read_posted(hw, msgbuf, 2, 0);
	if (err)
		return err;

	msgbuf[0] &= ~IXGBE_VT_MSGTYPE_CTS;
	if (msgbuf[0] == (IXGBE_VF_UPDATE_XCAST_MODE | IXGBE_VT_MSGTYPE_NACK))
		return -EPERM;

	return 0;
}

static int32_t ufp_mac_set_vfta(struct ufp_hw *hw, uint32_t vlan, uint32_t vlan_on)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	uint32_t msgbuf[2];
	int32_t ret_val;

	msgbuf[0] = IXGBE_VF_SET_VLAN;
	msgbuf[1] = vlan;
	/* Setting the 8 bit field MSG INFO to TRUE indicates "add" */
	msgbuf[0] |= vlan_on << IXGBE_VT_MSGINFO_SHIFT;

	ret_val = mbx->ops.write_posted(hw, msgbuf, 2, 0);
	if (!ret_val)
		ret_val = mbx->ops.read_posted(hw, msgbuf, 1, 0);

	if (!ret_val && (msgbuf[0] & IXGBE_VT_MSGTYPE_ACK))
		return 0;

	return ret_val | (msgbuf[0] & IXGBE_VT_MSGTYPE_NACK);
}

static int32_t ufp_mac_check_mac_link(struct ufp_hw *hw, uint32_t *speed,
	uint32_t *link_up)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	struct ixgbe_mac_info *mac = &hw->mac;
	int32_t ret_val = 0;
	uint32_t links_reg;
	uint32_t in_msg = 0;

	/* If we were hit with a reset drop the link */
	if (!mbx->ops.check_for_rst(hw, 0) || !mbx->timeout)
		mac->get_link_status = true;

	if (!mac->get_link_status)
		goto out;

	/* if link status is down no point in checking to see if pf is up */
	links_reg = ufp_read_reg(hw, IXGBE_VFLINKS);
	if (!(links_reg & IXGBE_LINKS_UP))
		goto out;

	/* for SFP+ modules and DA cables on 82599 it can take up to 500usecs
	 * before the link status is correct
	 */
	if (mac->type == ixgbe_mac_82599_vf) {
		int i;

		for (i = 0; i < 5; i++) {
			udelay(100);
			links_reg = ufp_read_reg(hw, IXGBE_VFLINKS);

			if (!(links_reg & IXGBE_LINKS_UP))
				goto out;
		}
	}

	switch (links_reg & IXGBE_LINKS_SPEED_82599) {
	case IXGBE_LINKS_SPEED_10G_82599:
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
		break;
	case IXGBE_LINKS_SPEED_1G_82599:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		break;
	case IXGBE_LINKS_SPEED_100_82599:
		*speed = IXGBE_LINK_SPEED_100_FULL;
		break;
	}

	/* if the read failed it could just be a mailbox collision, best wait
	 * until we are called again and don't report an error
	 */
	if (mbx->ops.read(hw, &in_msg, 1, 0))
		goto out;

	if (!(in_msg & IXGBE_VT_MSGTYPE_CTS)) {
		/* msg is not CTS and is NACK we must have lost CTS status */
		if (in_msg & IXGBE_VT_MSGTYPE_NACK)
			ret_val = -1;
		goto out;
	}

	/* the pf is talking, if we timed out in the past we reinit */
	if (!mbx->timeout) {
		ret_val = -1;
		goto out;
	}

	/* if we passed all the tests above then the link is up and we no
	 * longer need to check for link
	 */
	mac->get_link_status = false;

out:
	*link_up = !mac->get_link_status;
	return ret_val;
}

static void ufp_mac_set_rlpml(struct ufp_hw *hw, uint16_t max_size)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	uint32_t msgbuf[2];
	uint32_t retmsg[IXGBE_VFMAILBOX_SIZE];

	msgbuf[0] = IXGBE_VF_SET_LPE;
	msgbuf[1] = max_size;

        int32_t retval = mbx->ops.write_posted(hw, msgbuf, 2, 0);

	if (!retval)
		mbx->ops.read_posted(hw, retmsg, 2, 0);

	return;
}

static int32_t ufp_mac_negotiate_api_version(struct ufp_hw *hw, uint32_t api)
{
	int32_t err;
	uint32_t msg[3];

	/* Negotiate the mailbox API version */
	msg[0] = IXGBE_VF_API_NEGOTIATE;
	msg[1] = api;
	msg[2] = 0;
	err = hw->mbx.ops.write_posted(hw, msg, 3, 0);

	if (!err)
		err = hw->mbx.ops.read_posted(hw, msg, 3, 0);

	if (!err) {
		msg[0] &= ~IXGBE_VT_MSGTYPE_CTS;

		/* Store value and return 0 on success */
		if (msg[0] == (IXGBE_VF_API_NEGOTIATE | IXGBE_VT_MSGTYPE_ACK)) {
			hw->api_version = api;
			return 0;
		}

		err = IXGBE_ERR_INVALID_ARGUMENT;
	}

	return err;
}

static int32_t ufp_mac_get_queues(struct ufp_hw *hw, uint32_t *num_tcs,
	uint32_t *default_tc)
{
	int32_t err;
	uint32_t msg[5];

	/* do nothing if API doesn't support ixgbevf_get_queues */
	switch (hw->api_version) {
	case ixgbe_mbox_api_11:
		break;
	default:
		return 0;
	}

	/* Fetch queue configuration from the PF */
	msg[0] = IXGBE_VF_GET_QUEUES;
	msg[1] = msg[2] = msg[3] = msg[4] = 0;
	err = hw->mbx.ops.write_posted(hw, msg, 5, 0);

	if (!err)
		err = hw->mbx.ops.read_posted(hw, msg, 5, 0);

	if (!err) {
		msg[0] &= ~IXGBE_VT_MSGTYPE_CTS;

		/*
		 * if we we didn't get an ACK there must have been
		 * some sort of mailbox error so we should treat it
		 * as such
		 */
		if (msg[0] != (IXGBE_VF_GET_QUEUES | IXGBE_VT_MSGTYPE_ACK))
			return IXGBE_ERR_MBX;

		/* record and validate values from message */
		hw->mac.max_tx_queues = msg[IXGBE_VF_TX_QUEUES];
		if (hw->mac.max_tx_queues == 0 ||
		    hw->mac.max_tx_queues > IXGBE_VF_MAX_TX_QUEUES)
			hw->mac.max_tx_queues = IXGBE_VF_MAX_TX_QUEUES;

		hw->mac.max_rx_queues = msg[IXGBE_VF_RX_QUEUES];
		if (hw->mac.max_rx_queues == 0 ||
		    hw->mac.max_rx_queues > IXGBE_VF_MAX_RX_QUEUES)
			hw->mac.max_rx_queues = IXGBE_VF_MAX_RX_QUEUES;

		*num_tcs = msg[IXGBE_VF_TRANS_VLAN];
		/* in case of unknown state assume we cannot tag frames */
		if (*num_tcs > hw->mac.max_rx_queues)
			*num_tcs = 1;

		*default_tc = msg[IXGBE_VF_DEF_QUEUE];
		/* default to queue 0 on out-of-bounds queue number */
		if (*default_tc >= hw->mac.max_tx_queues)
			*default_tc = 0;
	}

	return err;
}
