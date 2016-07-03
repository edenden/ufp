#include <linux/types.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "ufp_main.h"
#include "ufp_mbx.h"

static int32_t ufp_mbx_poll_for_msg(struct ufp_hw *hw);
static int32_t ufp_mbx_poll_for_ack(struct ufp_hw *hw);
static int32_t ufp_mbx_read_posted(struct ufp_hw *hw, uint32_t *msg,
	uint16_t size);
static int32_t ufp_mbx_write_posted(struct ufp_hw *hw, uint32_t *msg,
	uint16_t size);
static uint32_t ufp_mbx_read_v2p_mailbox(struct ufp_hw *hw);
static int32_t ufp_mbx_check_for_bit(struct ufp_hw *hw, uint32_t mask);
static int32_t ufp_mbx_check_for_msg(struct ufp_hw *hw);
static int32_t ufp_mbx_check_for_ack(struct ufp_hw *hw);
static int32_t ufp_mbx_check_for_rst(struct ufp_hw *hw);
static int32_t ufp_mbx_obtain_lock(struct ufp_hw *hw);
static int32_t ufp_mbx_write(struct ufp_hw *hw, uint32_t *msg,
	uint16_t size);
static int32_t ufp_mbx_read(struct ufp_hw *hw, uint32_t *msg,
	uint16_t size);

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

	hw->mbx = mbx;
	return 0;
}

void ufp_mbx_free(struct ufp_hw *hw)
{
	kfree(hw->mbx);
	hw->mbx = NULL;

	return;
}

static int32_t ufp_mbx_poll_for_msg(struct ufp_hw *hw)
{
	struct ufp_mbx_info *mbx = hw->mbx;
	int countdown = mbx->timeout;

	if (!countdown || !mbx->ops.check_for_msg)
		goto out;

	while (countdown && mbx->ops.check_for_msg(hw)) {
		countdown--;
		if (!countdown)
			break;
		udelay(mbx->udelay);
	}

	if (countdown == 0)
		pr_err("Polling for VF mailbox ack timedout");

out:
	return countdown ? 0 : IXGBE_ERR_MBX;
}

static int32_t ufp_mbx_poll_for_ack(struct ufp_hw *hw)
{
	struct ufp_mbx_info *mbx = hw->mbx;
	int countdown = mbx->timeout;

	if (!countdown || !mbx->ops.check_for_ack)
		goto out;

	while (countdown && mbx->ops.check_for_ack(hw)) {
		countdown--;
		if (!countdown)
			break;
		udelay(mbx->udelay);
	}

	if (countdown == 0)
		pr_err("Polling for VF mailbox ack timedout");

out:
	return countdown ? 0 : IXGBE_ERR_MBX;
}

static int32_t ufp_mbx_read_posted(struct ufp_hw *hw, uint32_t *msg,
	uint16_t size)
{
	struct ufp_mbx_info *mbx = hw->mbx;
	int32_t ret_val = IXGBE_ERR_MBX;

	if (!mbx->ops.read)
		goto out;

	ret_val = ufp_mbx_poll_for_msg(hw);

	/* if ack received read message, otherwise we timed out */
	if (!ret_val)
		ret_val = mbx->ops.read(hw, msg, size);
out:
	return ret_val;
}

static int32_t ufp_mbx_write_posted(struct ufp_hw *hw, uint32_t *msg,
	uint16_t size)
{
	struct ufp_mbx_info *mbx = hw->mbx;
	int32_t ret_val = IXGBE_ERR_MBX;

	/* exit if either we can't write or there isn't a defined timeout */
	if (!mbx->ops.write || !mbx->timeout)
		goto out;

	/* send msg */
	ret_val = mbx->ops.write(hw, msg, size);

	/* if msg sent wait until we receive an ack */
	if (!ret_val)
		ret_val = ufp_mbx_poll_for_ack(hw);
out:
	return ret_val;
}

static uint32_t ufp_mbx_read_v2p_mailbox(struct ufp_hw *hw)
{
	struct ufp_mbx_info *mbx = hw->mbx;
	uint32_t v2p_mailbox;

	v2p_mailbox = ufp_read_reg(hw, IXGBE_VFMAILBOX);

	v2p_mailbox |= mbx->v2p_mailbox;
	mbx->v2p_mailbox |= v2p_mailbox & IXGBE_VFMAILBOX_R2C_BITS;

	return v2p_mailbox;
}

static int32_t ufp_mbx_check_for_bit(struct ufp_hw *hw, uint32_t mask)
{
	struct ufp_mbx_info *mbx = hw->mbx;
	uint32_t v2p_mailbox;

	v2p_mailbox = ufp_mbx_read_v2p_mailbox(hw);
	mbx->v2p_mailbox &= ~mask;

	return !(v2p_mailbox & mask);
}

static int32_t ufp_mbx_check_for_msg(struct ufp_hw *hw)
{
	if (!ufp_mbx_check_for_bit(hw, IXGBE_VFMAILBOX_PFSTS))
		return 0;

	return -1;
}

static int32_t ufp_mbx_check_for_ack(struct ufp_hw *hw)
{
	if (!ufp_mbx_check_for_bit(hw, IXGBE_VFMAILBOX_PFACK))
		return 0;

	return -1;
}

static int32_t ufp_mbx_check_for_rst(struct ufp_hw *hw)
{
	if (!ufp_mbx_check_for_bit(hw,
		(IXGBE_VFMAILBOX_RSTD | IXGBE_VFMAILBOX_RSTI)))
		return 0;

	return -1;
}

static int32_t ufp_mbx_obtain_lock(struct ufp_hw *hw)
{
	/* Take ownership of the buffer */
	ufp_write_reg(hw, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_VFU);

	/* reserve mailbox for vf use */
	if (ufp_mbx_read_v2p_mailbox(hw) & IXGBE_VFMAILBOX_VFU)
		return 0;

	return -1;
}

static int32_t ufp_mbx_write(struct ufp_hw *hw, uint32_t *msg,
	uint16_t size)
{
	int32_t ret_val;
	uint16_t i;

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ufp_mbx_obtain_lock(hw);
	if (ret_val)
		goto out_no_write;

	/* flush msg and acks as we are overwriting the message buffer */
	ufp_mbx_check_for_msg(hw);
	ufp_mbx_check_for_ack(hw);

	/* copy the caller specified message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		ufp_write_reg(hw, IXGBE_VFMBMEM + (i << 2), msg[i]);

	/* Drop VFU and interrupt the PF to tell it a message has been sent */
	ufp_write_reg(hw, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_REQ);

out_no_write:
	return ret_val;
}

static int32_t ufp_mbx_read(struct ufp_hw *hw, uint32_t *msg,
	uint16_t size)
{
	int32_t ret_val = 0;
	uint16_t i;

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ufp_mbx_obtain_lock(hw);
	if (ret_val)
		goto out_no_read;

	/* copy the message from the mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = ufp_read_reg(hw, IXGBE_VFMBMEM + (i << 2));

	/* Acknowledge receipt and release mailbox, then we're done */
	ufp_write_reg(hw, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_ACK);

out_no_read:
	return ret_val;
}

