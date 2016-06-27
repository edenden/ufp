#include <linux/types.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/miscdevice.h>
#include <linux/semaphore.h>

#include "ufp_main.h"
#include "ufp_mac.h"

s32 ufp_mac_init_ops(struct ufp_hw *hw)
{
	switch(hw->device_id){
	case IXGBE_DEV_ID_82599_VF:
		hw->mac.type = ixgbe_mac_82599_vf;
		break;
	case IXGBE_DEV_ID_X540_VF:
		hw->mac.type = ixgbe_mac_X540_vf;
		break;
	case IXGBE_DEV_ID_X550_VF:
		hw->mac.type = ixgbe_mac_X550_vf;
		break;
	case IXGBE_DEV_ID_X550EM_X_VF:
		hw->mac.type = ixgbe_mac_X550EM_x_vf;
		break;
	default:
		hw->mac.type = ixgbe_mac_unknown;
		break;
	}

	hw->mac.ops.reset_hw		= ufp_mac_reset;
	hw->mac.ops.stop_adapter	= ufp_mac_stop_adapter;
	hw->mac.ops.negotiate_api	= ufp_mac_negotiate_api_version;
	hw->mac.ops.get_queues		= ufp_mac_get_queues;
	hw->mac.ops.check_link		= ufp_mac_check_mac_link;
	hw->mac.ops.set_rar		= ufp_mac_set_rar;
	hw->mac.ops.set_uc_addr		= ufp_mac_set_uc_addr;
	hw->mac.ops.update_mc_addr_list	= ufp_mac_update_mc_addr_list;
	hw->mac.ops.update_xcast_mode	= ufp_mac_update_xcast_mode;
	hw->mac.ops.set_vfta		= ufp_mac_set_vfta;
	hw->mac.ops.set_rlpml		= ufp_mac_set_rlpml;

	return 0;
}

static void ufp_mac_clr_reg(struct ufp_hw *hw)
{
	int i;
	u32 vfsrrctl;
	u32 ufpca_rxctrl;
	u32 ufpca_txctrl;

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

s32 ufp_mac_reset(struct ufp_hw *hw)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 timeout = IXGBE_VF_INIT_TIMEOUT;
	s32 ret_val = IXGBE_ERR_INVALID_MAC_ADDR;
	u32 msgbuf[IXGBE_VF_PERMADDR_MSG_LEN];
	u8 *addr = (u8 *)(&msgbuf[1]);

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

s32 ufp_mac_stop_adapter(struct ufp_hw *hw)
{
	u32 reg_val;
	u16 i;

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

static s32 ufp_mac_mta_vector(struct ufp_hw *hw, u8 *mc_addr)
{
	u32 vector = 0;

	switch (hw->mac.mc_filter_type) {
	case 0:   /* use bits [47:36] of the address */
		vector = ((mc_addr[4] >> 4) | (((u16)mc_addr[5]) << 4));
		break;
	case 1:   /* use bits [46:35] of the address */
		vector = ((mc_addr[4] >> 3) | (((u16)mc_addr[5]) << 5));
		break;
	case 2:   /* use bits [45:34] of the address */
		vector = ((mc_addr[4] >> 2) | (((u16)mc_addr[5]) << 6));
		break;
	case 3:   /* use bits [43:32] of the address */
		vector = ((mc_addr[4]) | (((u16)mc_addr[5]) << 8));
		break;
	default:  /* Invalid mc_filter_type */
		hw_dbg(hw, "MC filter type param set incorrectly\n");
		break;
	}

	/* vector can only be 12-bits or boundary will be exceeded */
	vector &= 0xFFF;
	return vector;
}

s32 ufp_mac_set_rar(struct ufp_hw *hw, u8 *addr)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 msgbuf[3];
	u8 *msg_addr = (u8 *)(&msgbuf[1]);
	s32 ret_val;

	memset(msgbuf, 0, 12);
	msgbuf[0] = IXGBE_VF_SET_MAC_ADDR;
	memcpy(msg_addr, addr, 6);
	ret_val = mbx->ops.write_posted(hw, msgbuf, 3, 0);

	if (!ret_val)
		ret_val = mbx->ops.read_posted(hw, msgbuf, 3, 0);

	msgbuf[0] &= ~IXGBE_VT_MSGTYPE_CTS;
	return ret_val;
}

s32 ufp_mac_update_mc_addr_list(struct ufp_hw *hw, u8 *mc_addr_list,
	u32 mc_addr_count, ixgbe_mc_addr_itr next)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 msgbuf[IXGBE_VFMAILBOX_SIZE];
	u16 *vector_list = (u16 *)&msgbuf[1];
	u32 vector;
	u32 cnt, i;
	u32 vmdq;

	/* Each entry in the list uses 1 16 bit word.  We have 30
	 * 16 bit words available in our HW msg buffer (minus 1 for the
	 * msg type).  That's 30 hash values if we pack 'em right.  If
	 * there are more than 30 MC addresses to add then punt the
	 * extras for now and then add code to handle more than 30 later.
	 * It would be unusual for a server to request that many multi-cast
	 * addresses except for in large enterprise network environments.
	 */

	hw_dbg(hw, "MC Addr Count = %d\n", mc_addr_count);

	cnt = (mc_addr_count > 30) ? 30 : mc_addr_count;
	msgbuf[0] = IXGBE_VF_SET_MULTICAST;
	msgbuf[0] |= cnt << IXGBE_VT_MSGINFO_SHIFT;

	for (i = 0; i < cnt; i++) {
		vector = ufp_mac_mta_vector(hw, next(hw, &mc_addr_list, &vmdq));
		hw_dbg(hw, "Hash value = 0x%03X\n", vector);
		vector_list[i] = (u16)vector;
	}

	return mbx->ops.write_posted(hw, msgbuf, IXGBE_VFMAILBOX_SIZE, 0);
}

static s32 ufp_mac_update_xcast_mode(struct ufp_hw *hw, int xcast_mode)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 msgbuf[2];
	s32 err;

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

s32 ufp_mac_set_vfta(struct ufp_hw *hw, u32 vlan, u32 vlan_on)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 msgbuf[2];
	s32 ret_val;

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

s32 ufp_mac_set_uc_addr(struct ufp_hw *hw, u32 index, u8 *addr)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 msgbuf[3];
	u8 *msg_addr = (u8 *)(&msgbuf[1]);
	s32 ret_val;

	memset(msgbuf, 0, sizeof(msgbuf));
	/*
	 * If index is one then this is the start of a new list and needs
	 * indication to the PF so it can do it's own list management.
	 * If it is zero then that tells the PF to just clear all of
	 * this VF's macvlans and there is no new list.
	 */
	msgbuf[0] |= index << IXGBE_VT_MSGINFO_SHIFT;
	msgbuf[0] |= IXGBE_VF_SET_MACVLAN;
	if (addr)
		memcpy(msg_addr, addr, 6);
	ret_val = mbx->ops.write_posted(hw, msgbuf, 3, 0);

	if (!ret_val)
		ret_val = mbx->ops.read_posted(hw, msgbuf, 3, 0);

	msgbuf[0] &= ~IXGBE_VT_MSGTYPE_CTS;

	if (!ret_val)
		if (msgbuf[0] == (IXGBE_VF_SET_MACVLAN | IXGBE_VT_MSGTYPE_NACK))
			ret_val = IXGBE_ERR_OUT_OF_MEM;

	return ret_val;
}

s32 ufp_mac_check_mac_link(struct ufp_hw *hw, u32 *speed, u32 *link_up)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	struct ixgbe_mac_info *mac = &hw->mac;
	s32 ret_val = 0;
	u32 links_reg;
	u32 in_msg = 0;

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

void ufp_mac_set_rlpml(struct ufp_hw *hw, u16 max_size)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 msgbuf[2];
	u32 retmsg[IXGBE_VFMAILBOX_SIZE];

	msgbuf[0] = IXGBE_VF_SET_LPE;
	msgbuf[1] = max_size;

        s32 retval = mbx->ops.write_posted(hw, msgbuf, 2, 0);

	if (!retval)
		mbx->ops.read_posted(hw, retmsg, 2, 0);

	return;
}

s32 ufp_mac_negotiate_api_version(struct ufp_hw *hw, enum ufp_mbx_api_rev api)
{
	s32 err;
	u32 msg[3];

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

s32 ufp_mac_get_queues(struct ufp_hw *hw, u32 *num_tcs, u32 *default_tc)
{
	s32 err;
	u32 msg[5];

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
