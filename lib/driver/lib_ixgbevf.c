#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

#include "ufp_ixgbevf.h"
#include "ufp_ixgbevf_mbx.h"

static int ufp_ixgbevf_negotiate_api(struct ufp_handle *ih);
static int ufp_ixgbevf_stop_adapter(struct ufp_handle *ih);
static int ufp_ixgbevf_reset(struct ufp_handle *ih);
static int ufp_ixgbevf_get_queues(struct ufp_handle *ih);
static int ufp_ixgbevf_get_bufsize(struct ufp_handle *ih);
static int ufp_ixgbevf_configure_irq(struct ufp_handle *ih);
static int ufp_ixgbevf_configure_tx(struct ufp_handle *ih);
static int ufp_ixgbevf_configure_rx(struct ufp_handle *ih);

int ufp_ixgbevf_init(struct ufp_handle *ih, struct ufp_ops *ops)
{
	struct ufp_ixgbevf_data *data;

	data = malloc(sizeof(struct ufp_ixgbevf_data));
	if(!data)
		goto err_alloc_data;

	ops->data		= data;

	mbx_timeout		= IXGBE_VF_INIT_TIMEOUT;
	mbx_udelay		= IXGBE_VF_MBX_INIT_DELAY;
	mbx_size		= IXGBE_VFMAILBOX_SIZE;

	ops->reset_hw		= ufp_ixgbevf_reset;
	ops->get_queues		= ufp_ixgbevf_get_queues;
	ops->get_bufsize	= ufp_ixgbevf_get_bufsize;
	ops->configure_irq	= ufp_ixgbevf_configure_irq;
	ops->configure_tx	= ufp_ixgbevf_configure_tx;
	ops->configure_rx	= ufp_ixgbevf_configure_rx;
	ops->stop_adapter	= ufp_ixgbevf_stop_adapter;

	err = ufp_ixgbevf_negotiate_api(ih);
	if(err < 0)
		goto err_nego_api;

	return 0;

err_nego_api:
	free(ops->data);
err_alloc_data:
	return -1;
}

void ufp_ixgbevf_destroy(struct ufp_ops *ops)
{
	free(ops->data);
	return;
}

static int ufp_ixgbevf_negotiate_api(struct ufp_handle *ih)
{
	int32_t err, i = 0;
	uint32_t msg[3];
	struct ufp_ixgbevf_data *data;

	data = ih->ops.data;

	enum ufp_mbx_api_rev api[] = {
		ixgbe_mbox_api_12,
		ixgbe_mbox_api_11,
		ixgbe_mbox_api_10,
		ixgbe_mbox_api_unknown
	};

	while (api[i] != ixgbe_mbox_api_unknown) {

		/* Negotiate the mailbox API version */
		msg[0] = IXGBE_VF_API_NEGOTIATE;
		msg[1] = api[i];
		msg[2] = 0;

		err = mbx->ops.write_posted(hw, msg, 3);
		if(err) 
			goto err_write;

		err = mbx->ops.read_posted(hw, msg, 3);
		if(err) 
			goto err_read;

		msg[0] &= ~IXGBE_VT_MSGTYPE_CTS;

		if (msg[0] == (IXGBE_VF_API_NEGOTIATE | IXGBE_VT_MSGTYPE_ACK))
			break;

		i++;
	}

	data->api_version = api[i];
	return 0;

err_read:
err_write:
	return -1;
}

static int ufp_ixgbevf_stop_adapter(struct ufp_handle *ih)
{
	uint32_t reg_val;
	uint16_t i;
	struct timespec ts;

	/* Clear interrupt mask to stop from interrupts being generated */
	ufp_write_reg(ih, IXGBE_VTEIMC, IXGBE_VF_IRQ_CLEAR_MASK);

	/* Clear any pending interrupts, flush previous writes */
	ufp_read_reg(ih, IXGBE_VTEICR);

	/* Disable the transmit unit.  Each queue must be disabled. */
	for (i = 0; i < IXGBE_VF_MAX_TX_QUEUES; i++)
		ufp_write_reg(ih, IXGBE_VFTXDCTL(i), IXGBE_TXDCTL_SWFLSH);

	/* Disable the receive unit by stopping each queue */
	for (i = 0; i < IXGBE_VF_MAX_RX_QUEUES; i++) {
		reg_val = ufp_read_reg(ih, IXGBE_VFRXDCTL(i));
		reg_val &= ~IXGBE_RXDCTL_ENABLE;
		ufp_write_reg(ih, IXGBE_VFRXDCTL(i), reg_val);
	}
	/* Clear packet split and pool config */
	ufp_write_reg(ih, IXGBE_VFPSRTYPE, 0);

	/* flush all queues disables */
	ufp_write_flush(ih);
	msleep(&ts, 2);

	return 0;
}

static int ufp_ixgbevf_reset(struct ufp_handle *ih)
{
	uint32_t msgbuf[IXGBE_VF_PERMADDR_MSG_LEN];
	uint32_t timeout;
	int err;
	struct timespec ts;
	int32_t mc_filter_type;

	/* Call adapter stop to disable tx/rx and clear interrupts */
	err = stop_adapter(ih);
	if(err < 0)
		goto err_stop_adapter;

	ufp_write_reg(hw, IXGBE_VFCTRL, IXGBE_CTRL_RST);
	ufp_write_flush(hw);

	msleep(&ts, 50);

	/* we cannot reset while the RSTI / RSTD bits are asserted */
	timeout = data->mbx_timeout;
	while (!mbx->ops.check_for_rst(hw) && timeout) {
		timeout--;
		usleep(&ts, 5);
	}

	if (!timeout)
		goto err_reset_failed;

	/* Reset VF registers to initial values */
	ufp_mac_clr_reg(hw);

	msgbuf[0] = IXGBE_VF_RESET;
	err = mbx->ops.write_posted(hw, msgbuf, 1);
	if(err)
		goto err_write;

	msleep(&ts, 10);

	/*
	 * set our "perm_addr" based on info provided by PF
	 * also set up the mc_filter_type which is piggy backed
	 * on the mac address in word 3
	 */
	err = mbx->ops.read_posted(hw, msgbuf, IXGBE_VF_PERMADDR_MSG_LEN);
	if (err)
		goto err_read;

	if (msgbuf[0] != (IXGBE_VF_RESET | IXGBE_VT_MSGTYPE_ACK))
		goto err_read_mac_addr;

	memcpy(ih->mac_addr, &msgbuf[1], 6);

	/* Currently mc filter is not used */
	mc_filter_type = msgbuf[IXGBE_VF_MC_TYPE_WORD];

	return 0;

err_read_mac_addr:
err_read:
err_write:
err_reset_failed:
err_stop_adapter:
	return -1;
}

static int ufp_ixgbevf_get_queues(struct ufp_handle *ih)
{
	int32_t err;
	uint32_t msg[5];
	uint32_t num_tcs, default_tc;
	uint32_t max_tx_queues, max_rx_queues;

	/* do nothing if API doesn't support ixgbevf_get_queues */
	switch (hw->api_version) {
	case ixgbe_mbox_api_11:
	case ixgbe_mbox_api_12:
		break;
	default:
		/* assume legacy case in which
		 * PF would only give VF 2 queue pairs
		 */
		ih->num_queues = 2;
		return 0;
	}

	/* Fetch queue configuration from the PF */
	msg[0] = IXGBE_VF_GET_QUEUES;
	msg[1] = msg[2] = msg[3] = msg[4] = 0;
	err = mbx->ops.write_posted(hw, msg, 5);
	if(err)
		goto err_write;

	err = mbx->ops.read_posted(hw, msg, 5);
	if(err)
		goto err_read;

	msg[0] &= ~IXGBE_VT_MSGTYPE_CTS;

	/*
	 * if we we didn't get an ACK there must have been
	 * some sort of mailbox error so we should treat it
	 * as such
	 */
	if (msg[0] != (IXGBE_VF_GET_QUEUES | IXGBE_VT_MSGTYPE_ACK))
		goto err_get_queues;

	/* record and validate values from message */
	max_tx_queues = msg[IXGBE_VF_TX_QUEUES];
	if(max_tx_queues == 0 ||
	max_tx_queues > IXGBE_VF_MAX_TX_QUEUES)
		max_tx_queues = IXGBE_VF_MAX_TX_QUEUES;

	max_rx_queues = msg[IXGBE_VF_RX_QUEUES];
	if(max_rx_queues == 0 ||
	max_rx_queues > IXGBE_VF_MAX_RX_QUEUES)
		max_rx_queues = IXGBE_VF_MAX_RX_QUEUES;

	/* Currently tc related parameters are not used */
	*num_tcs = msg[IXGBE_VF_TRANS_VLAN];
	/* in case of unknown state assume we cannot tag frames */
	if (*num_tcs > mac->max_rx_queues)
		*num_tcs = 1;

	*default_tc = msg[IXGBE_VF_DEF_QUEUE];
	/* default to queue 0 on out-of-bounds queue number */
	if (*default_tc >= mac->max_tx_queues)
		*default_tc = 0;

	ih->num_queues = min(max_tx_queues, max_rx_queues);
	return 0;

err_get_queues:
err_read:
err_write:
	return -1;
}

static int ufp_ixgbevf_get_bufsize(struct ufp_handle *ih)
{
	ih->buf_size = IXGBEVF_RX_BUFSZ;
	return 0;
}

static int ufp_ixgbevf_configure_irq(struct ufp_handle *ih)
{
	unsigned int qmask = 0;

	for(queue_idx = 0, vector = 0; queue_idx < ih->num_queues;
	queue_idx++, vector++){
		/* set RX queue interrupt */
		ufp_set_ivar(ih, 0, queue_idx, vector);
		ufp_write_eitr(ih, vector);
		qmask |= 1 << vector;
	}

	for(queue_idx = 0; queue_idx < ih->num_queues;
	queue_idx++, vector++){
		/* set TX queue interrupt */
		ufp_set_ivar(ih, 1, queue_idx, vector);
		ufp_write_eitr(ih, vector);
		qmask |= 1 << vector;
	}

	/* clear any pending interrupts, may auto mask */
	ufp_read_reg(ih, IXGBE_VTEICR);

	ufp_write_reg(ih, IXGBE_VTEIAM, qmask);
	ufp_write_reg(ih, IXGBE_VTEIAC, qmask);
	ufp_write_reg(ih, IXGBE_VTEIMS, qmask);
}

static int ufp_ixgbevf_configure_tx(struct ufp_handle *ih)
{
	int i;

	/* Setup the HW Tx Head and Tail descriptor pointers */
	for (i = 0; i < ih->num_queues; i++)
		ufp_ixgbevf_confiugre_tx_ring(ih, i, &ih->rx_ring[i]);
}

static int ufp_ixgbevf_configure_rx(struct ufp_handle *ih)
{
	int i, err;

	/* VF can't support promiscuous mode */
	if(ih->promisc)
		goto err_promisc;

	err = ufp_ixgbevf_update_xcast_mode(ih, IXGBEVF_XCAST_MODE_ALLMULTI);
	if(err < 0)
		goto err_xcast_mode;

	ufp_ixgbevf_set_psrtype(ih);

	switch(ih->ops->device_id){
	case IXGBE_DEV_ID_X550_VF:
	case IXGBE_DEV_ID_X550EM_X_VF:
		ufp_ixgbevf_set_vfmrqc(ih);
		break;
	default:
		break;
	}

	err = ufp_ixgbevf_set_rlpml(ih);
	if(err < 0)
		goto err_set_rlpml;

	/* Setup the HW Rx Head and Tail Descriptor Pointers and
	 * the Base and Length of the Rx Descriptor Ring
	 */
	for (i = 0; i < ih->num_queues; i++)
		ufp_ixgbevf_configure_rx_ring(ih, i, &ih->rx_ring[i]);

	return 0;

err_set_rlpml:
err_xcast_mode:
err_promisc:
	return -1;
}

