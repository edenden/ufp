#include <linux/types.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "lib_ixgbevf.h"
#include "lib_ixgbevf_mbx.h"

static int ufp_ixgbevf_mbx_poll_for_msg(struct ufp_handle *ih);
static int ufp_ixgbevf_mbx_poll_for_ack(struct ufp_handle *ih);
static uint32_t ufp_ixgbevf_mbx_read_v2p_mailbox(struct ufp_handle *ih);
static int ufp_ixgbevf_mbx_check_for_bit(struct ufp_handle *ih, uint32_t mask);
static int ufp_ixgbevf_mbx_check_for_msg(struct ufp_handle *ih);
static int ufp_ixgbevf_mbx_check_for_ack(struct ufp_handle *ih);
static int ufp_ixgbevf_mbx_check_for_rst(struct ufp_handle *ih);
static int ufp_ixgbevf_mbx_obtain_lock(struct ufp_handle *ih);

static int ufp_ixgbevf_mbx_poll_for_msg(struct ufp_handle *ih)
{
	struct ufp_ixgbevf_data *data;
	struct timespec ts;
	int countdown;

	data = ih->ops->data;
	countdown = data->mbx_timeout;

	while(countdown && ufp_ixgbevf_mbx_check_for_msg(ih)) {
		usleep(&ts, data->mbx_udelay);
		countdown--;
	}

	if(!countdown)
		goto err_timeout;

	return 0;

err_timeout:
	return -1;
}

static int ufp_ixgbevf_mbx_poll_for_ack(struct ufp_handle *ih)
{
	struct ufp_ixgbevf_data *data;
	struct timespec ts;
	int countdown; 

	data = ih->ops->data;
	countdown = data->mbx_timeout;

	while(countdown && ufp_ixgbevf_mbx_check_for_ack(ih)) {
		usleep(&ts, data->mbx_udelay);
		countdown--;
	}

	if(!countdown)
		goto err_timeout;

	return 0;

err_timeout:
	return -1;
}

int ufp_ixgbevf_mbx_poll_for_rst(struct ufp_handle *ih)
{
	struct ufp_ixgbevf_data *data;
	struct timespec ts;
	int countdown;

	data = ih->ops->data;
	countdown = data->mbx_timeout;

	while(countdown && !ufp_ixgbevf_mbx_check_for_rst(hw)){
		usleep(&ts, data->mbx_udelay);
		countdown--;
	}

	if(!countdown)
		goto err_timeout;

	return 0;

err_timeout:
	return -1;
}

int ufp_ixgbevf_mbx_read_posted(struct ufp_handle *ih,
	uint32_t *msg, uint16_t size)
{
	int err;

	err = ufp_ixgbevf_mbx_poll_for_msg(ih);
	if(err < 0)
		goto err_poll_for_msg;

	/* if ack received read message, otherwise we timed out */
	err = ufp_ixgbevf_mbx_read(ih, msg, size);
	if(err < 0)
		goto err_read;

	return 0;

err_read:
err_poll_for_msg:
	return -1;
}

int ufp_ixgbevf_mbx_write_posted(struct ufp_handle *ih,
	uint32_t *msg, uint16_t size)
{
	int err;

	/* send msg */
	err = ufp_ixgbevf_mbx_write(ih, msg, size);
	if(err < 0)
		goto err_write;

	/* if msg sent wait until we receive an ack */
	err = ufp_ixgbevf_mbx_poll_for_ack(ih);
	if(err < 0)
		goto err_poll_for_ack;

	return 0;

err_poll_for_ack:
err_write:
	return -1;
}

static uint32_t ufp_ixgbevf_mbx_read_v2p_mailbox(struct ufp_handle *ih)
{
	struct ufp_ixgbevf_data *data;
	uint32_t v2p_mailbox;

	data = ih->ops->data;

	v2p_mailbox = ufp_read_reg(ih, IXGBE_VFMAILBOX);

	v2p_mailbox |= data->mbx_v2p_mailbox;
	data->mbx_v2p_mailbox |= v2p_mailbox & IXGBE_VFMAILBOX_R2C_BITS;

	return v2p_mailbox;
}

static int ufp_ixgbevf_mbx_check_for_bit(struct ufp_handle *ih, uint32_t mask)
{
	struct ufp_ixgbevf_data *data;
	uint32_t v2p_mailbox;

	data = ih->ops->data;

	v2p_mailbox = ufp_ixgbevf_mbx_read_v2p_mailbox(ih);
	data->mbx_v2p_mailbox &= ~mask;

	return !(v2p_mailbox & mask);
}

static int ufp_ixgbevf_mbx_check_for_msg(struct ufp_handle *ih)
{
	if (!ufp_ixgbevf_mbx_check_for_bit(ih, IXGBE_VFMAILBOX_PFSTS))
		return 0;

	return -1;
}

static int ufp_ixgbevf_mbx_check_for_ack(struct ufp_handle *ih)
{
	if (!ufp_ixgbevf_mbx_check_for_bit(ih, IXGBE_VFMAILBOX_PFACK))
		return 0;

	return -1;
}

static int ufp_ixgbevf_mbx_check_for_rst(struct ufp_handle *ih)
{
	if (!ufp_ixgbevf_mbx_check_for_bit(ih,
		(IXGBE_VFMAILBOX_RSTD | IXGBE_VFMAILBOX_RSTI)))
		return 0;

	return -1;
}

static int ufp_ixgbevf_mbx_obtain_lock(struct ufp_handle *ih)
{
	/* Take ownership of the buffer */
	ufp_write_reg(ih, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_VFU);

	/* reserve mailbox for vf use */
	if (ufp_ixgbevf_mbx_read_v2p_mailbox(ih) & IXGBE_VFMAILBOX_VFU)
		return 0;

	return -1;
}

int ufp_ixgbevf_mbx_write(struct ufp_handle *ih,
	uint32_t *msg, uint16_t size)
{
	int i, err;

	/* lock the mailbox to prevent pf/vf race condition */
	err = ufp_ixgbevf_mbx_obtain_lock(ih);
	if(err < 0)
		goto out_no_write;

	/* flush msg and acks as we are overwriting the message buffer */
	ufp_ixgbevf_mbx_check_for_msg(ih);
	ufp_ixgbevf_mbx_check_for_ack(ih);

	/* copy the caller specified message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		ufp_write_reg(ih, IXGBE_VFMBMEM + (i << 2), msg[i]);

	/* Drop VFU and interrupt the PF to tell it a message has been sent */
	ufp_write_reg(ih, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_REQ);

	return 0;

out_no_write:
	return -1;
}

int ufp_ixgbevf_mbx_read(struct ufp_handle *ih,
	uint32_t *msg, uint16_t size)
{
	int i, err;

	/* lock the mailbox to prevent pf/vf race condition */
	err = ufp_ixgbevf_mbx_obtain_lock(ih);
	if(err < 0)
		goto out_no_read;

	/* copy the message from the mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = ufp_read_reg(ih, IXGBE_VFMBMEM + (i << 2));

	/* Acknowledge receipt and release mailbox, then we're done */
	ufp_write_reg(ih, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_ACK);

	return 0;

out_no_read:
	return -1;
}

