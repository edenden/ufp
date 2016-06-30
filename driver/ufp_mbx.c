#include <linux/types.h>

#include "ufp_main.h"
#include "ufp_mbx.h"

int ufp_mbx_init(struct ufp_hw *hw)
{
	struct ufp_mbx_info *mbx;

	mbx = kzalloc(sizeof(struct ufp_mbx_info), GFP_KERNEL);
	if(!mbx){
		return -1;
	}

	/* start mailbox as timed out and let the reset_hw call set the timeout
	 * value to begin communications */
	mbx->timeout = 0;
	mbx->udelay = IXGBE_VF_MBX_INIT_DELAY;

	mbx->size = IXGBE_VFMAILBOX_SIZE;

	mbx->ops.read		= ufp_mbx_read;
	mbx->ops.write		= ufp_mbx_write;
	mbx->ops.read_posted	= ufp_mbx_read_posted;
	mbx->ops.write_posted	= ufp_mbx_write_posted;
	mbx->ops.check_for_msg	= ufp_mbx_check_for_msg;
	mbx->ops.check_for_ack	= ufp_mbx_check_for_ack;
	mbx->ops.check_for_rst	= ufp_mbx_check_for_rst;

	mbx->stats.msgs_tx	= 0;
	mbx->stats.msgs_rx	= 0;
	mbx->stats.reqs		= 0;
	mbx->stats.acks		= 0;
	mbx->stats.rsts		= 0;

	hw->mbx = mbx;
	return 0;
}

void ufp_mbx_free(struct ufp_hw *hw)
{
	kfree(hw->mbx);
	hw->mbx = NULL;

	return;
}

static s32 ufp_mbx_poll_for_msg(struct ufp_hw *hw, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	if (!countdown || !mbx->ops.check_for_msg)
		goto out;

	while (countdown && mbx->ops.check_for_msg(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		udelay(mbx->udelay);
	}

	if (countdown == 0)
		ERROR_REPORT2(IXGBE_ERROR_POLLING,
			   "Polling for VF%d mailbox message timedout", mbx_id);

out:
	return countdown ? 0 : IXGBE_ERR_MBX;
}

static s32 ufp_mbx_poll_for_ack(struct ufp_hw *hw, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	if (!countdown || !mbx->ops.check_for_ack)
		goto out;

	while (countdown && mbx->ops.check_for_ack(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		udelay(mbx->udelay);
	}

	if (countdown == 0)
		ERROR_REPORT2(IXGBE_ERROR_POLLING,
			     "Polling for VF%d mailbox ack timedout", mbx_id);

out:
	return countdown ? 0 : IXGBE_ERR_MBX;
}

static s32 ufp_mbx_read_posted(struct ufp_hw *hw, u32 *msg,
	u16 size, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = IXGBE_ERR_MBX;

	if (!mbx->ops.read)
		goto out;

	ret_val = ufp_mbx_poll_for_msg(hw, mbx_id);

	/* if ack received read message, otherwise we timed out */
	if (!ret_val)
		ret_val = mbx->ops.read(hw, msg, size, mbx_id);
out:
	return ret_val;
}

static s32 ufp_mbx_write_posted(struct ufp_hw *hw, u32 *msg,
	u16 size, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = IXGBE_ERR_MBX;

	/* exit if either we can't write or there isn't a defined timeout */
	if (!mbx->ops.write || !mbx->timeout)
		goto out;

	/* send msg */
	ret_val = mbx->ops.write(hw, msg, size, mbx_id);

	/* if msg sent wait until we receive an ack */
	if (!ret_val)
		ret_val = ufp_mbx_poll_for_ack(hw, mbx_id);
out:
	return ret_val;
}

static u32 ufp_mbx_read_v2p_mailbox(struct ufp_hw *hw)
{
	u32 v2p_mailbox = IXGBE_READ_REG(hw, IXGBE_VFMAILBOX);

	v2p_mailbox |= hw->mbx.v2p_mailbox;
	hw->mbx.v2p_mailbox |= v2p_mailbox & IXGBE_VFMAILBOX_R2C_BITS;

	return v2p_mailbox;
}

static s32 ufp_mbx_check_for_bit(struct ufp_hw *hw, u32 mask)
{
	u32 v2p_mailbox = ufp_mbx_read_v2p_mailbox(hw);
	s32 ret_val = IXGBE_ERR_MBX;

	if (v2p_mailbox & mask)
		ret_val = 0;

	hw->mbx.v2p_mailbox &= ~mask;

	return ret_val;
}

static s32 ufp_mbx_check_for_msg(struct ufp_hw *hw, u16 mbx_id)
{
	s32 ret_val = IXGBE_ERR_MBX;

	UNREFERENCED_1PARAMETER(mbx_id);
	if (!ufp_mbx_check_for_bit(hw, IXGBE_VFMAILBOX_PFSTS)) {
		ret_val = 0;
		hw->mbx.stats.reqs++;
	}

	return ret_val;
}

static s32 ufp_mbx_check_for_ack(struct ufp_hw *hw, u16 mbx_id)
{
	s32 ret_val = IXGBE_ERR_MBX;

	UNREFERENCED_1PARAMETER(mbx_id);
	if (!ufp_mbx_check_for_bit(hw, IXGBE_VFMAILBOX_PFACK)) {
		ret_val = 0;
		hw->mbx.stats.acks++;
	}

	return ret_val;
}

static s32 ufp_mbx_check_for_rst(struct ufp_hw *hw, u16 mbx_id)
{
	s32 ret_val = IXGBE_ERR_MBX;

	UNREFERENCED_1PARAMETER(mbx_id);
	if (!ufp_mbx_check_for_bit(hw, (IXGBE_VFMAILBOX_RSTD |
	    IXGBE_VFMAILBOX_RSTI))) {
		ret_val = 0;
		hw->mbx.stats.rsts++;
	}

	return ret_val;
}

static s32 ufp_mbx_obtain_lock(struct ufp_hw *hw)
{
	s32 ret_val = IXGBE_ERR_MBX;

	/* Take ownership of the buffer */
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_VFU);

	/* reserve mailbox for vf use */
	if (ufp_mbx_read_v2p_mailbox(hw) & IXGBE_VFMAILBOX_VFU)
		ret_val = 0;

	return ret_val;
}

static s32 ufp_mbx_write(struct ufp_hw *hw, u32 *msg,
	u16 size, u16 mbx_id)
{
	s32 ret_val;
	u16 i;

	UNREFERENCED_1PARAMETER(mbx_id);

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ufp_mbx_obtain_lock(hw);
	if (ret_val)
		goto out_no_write;

	/* flush msg and acks as we are overwriting the message buffer */
	ixgbe_check_for_msg_vf(hw, 0);
	ixgbe_check_for_ack_vf(hw, 0);

	/* copy the caller specified message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		IXGBE_WRITE_REG_ARRAY(hw, IXGBE_VFMBMEM, i, msg[i]);

	/* update stats */
	hw->mbx.stats.msgs_tx++;

	/* Drop VFU and interrupt the PF to tell it a message has been sent */
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_REQ);

out_no_write:
	return ret_val;
}

static s32 ufp_mbx_read(struct ufp_hw *hw, u32 *msg,
	u16 size, u16 mbx_id)
{
	s32 ret_val = 0;
	u16 i;

	UNREFERENCED_1PARAMETER(mbx_id);
	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ufp_mbx_obtain_lock(hw);
	if (ret_val)
		goto out_no_read;

	/* copy the message from the mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = IXGBE_READ_REG_ARRAY(hw, IXGBE_VFMBMEM, i);

	/* Acknowledge receipt and release mailbox, then we're done */
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_ACK);

	/* update stats */
	hw->mbx.stats.msgs_rx++;

out_no_read:
	return ret_val;
}

