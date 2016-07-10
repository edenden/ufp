#include <linux/types.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "ufp_ixgbevf.h"
#include "ufp_ixgbevf_mbx.h"

static int ufp_ixgbevf_get_queues(struct ufp_handle *ih);
static int ufp_ixgbevf_reset(struct ufp_handle *ih);
static int ufp_ixgbevf_irq_configure(struct ufp_handle *ih);

static int32_t ufp_mac_update_xcast_mode(struct ufp_hw *hw, int xcast_mode);
static int32_t ufp_mac_set_rlpml(struct ufp_hw *hw, uint16_t max_size);

int ufp_ixgbevf_init(struct ufp_ops *ops)
{
	struct ufp_ixgbevf_data *data;

	data = malloc(sizeof(struct ufp_ixgbevf_data));
	if(!data)
		goto err_alloc_data;

	ops->data		= data;

	ops->reset_hw		= ufp_ixgbevf_reset;
	ops->get_queues		= ufp_ixgbevf_get_queues;
	ops->get_bufsize	= ufp_ixgbevf_get_bufsize;
	ops->irq_configure	= ufp_ixgbevf_irq_configure;
	ops->tx_configure	= ufp_ixgbevf_tx_configure;
	ops->rx_configure	= ufp_ixgbevf_rx_configure;

	return 0;

err_alloc_data:
	return -1;
}

void ufp_ixgbevf_destroy(struct ufp_ops *ops)
{
	free(ops->data);
	return;
}

static int ufp_ixgbevf_reset(struct ufp_handle *ih)
{
        uint32_t timeout = IXGBE_VF_INIT_TIMEOUT;
        uint32_t msgbuf[IXGBE_VF_PERMADDR_MSG_LEN];
        int err;
        int32_t mc_filter_type;

        /* Call adapter stop to disable tx/rx and clear interrupts */
        mac->ops.stop_adapter(hw);

        pr_info("Issuing a function level reset to MAC\n");

        ufp_write_reg(hw, IXGBE_VFCTRL, IXGBE_CTRL_RST);
        ufp_write_flush(hw);

        msleep(50);

        /* we cannot reset while the RSTI / RSTD bits are asserted */
        while (!mbx->ops.check_for_rst(hw) && timeout) {
                timeout--;
                udelay(5);
        }

        if (!timeout)
                goto err_reset_failed;

        /* Reset VF registers to initial values */
        ufp_mac_clr_reg(hw);

        /* mailbox timeout can now become active */
        mbx->timeout = IXGBE_VF_MBX_INIT_TIMEOUT;

        msgbuf[0] = IXGBE_VF_RESET;
        mbx->ops.write_posted(hw, msgbuf, 1);

        msleep(10);

        /*
         * set our "perm_addr" based on info provided by PF
         * also set up the mc_filter_type which is piggy backed
         * on the mac address in word 3
         */
        err = mbx->ops.read_posted(hw, msgbuf, IXGBE_VF_PERMADDR_MSG_LEN);
        if (err)
                goto err_read_mac_addr;

        if (msgbuf[0] != (IXGBE_VF_RESET | IXGBE_VT_MSGTYPE_ACK))
                goto err_read_mac_addr;

        memcpy(ih->mac_addr, &msgbuf[1], 6);

        /* Currently mc filter is not used */
        mc_filter_type = msgbuf[IXGBE_VF_MC_TYPE_WORD];

        return 0;

err_read_mac_addr:
err_reset_failed:
        return -1;
}

static int ufp_ixgbevf_get_queues(struct ufp_handle *ih)
{
	int32_t err;
	uint32_t msg[5];
	uint32_t num_tcs, default_tc;
	uint32_t max_tx_queues, max_rx_queues;

	err = ufp_ixgbevf_negotiate_api(ih);
	if(err < 0)
		goto err_nego_api;

	/* do nothing if API doesn't support ixgbevf_get_queues */
	switch (hw->api_version) {
	case ixgbe_mbox_api_11:
	case ixgbe_mbox_api_12:
		break;
	default:
		ih->num_queues = 1;
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
err_nego_api:
        return -1;
}

static int ufp_ixgbevf_get_bufsize(struct ufp_handle *ih)
{
	ih->buf_size = IXGBEVF_RX_BUFSZ;
	return 0;
}

static int ufp_ixgbevf_irq_configure(struct ufp_handle *ih)
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

static int ufp_ixgbevf_tx_configure(struct ufp_handle *ih)
{
	int i;

	/* Setup the HW Tx Head and Tail descriptor pointers */
	for (i = 0; i < ih->num_queues; i++)
		ufp_ixgbevf_tx_configure_ring(ih, i, &ih->rx_ring[i]);
}

static int ufp_ixgbevf_rx_configure(struct ufp_handle *ih)
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
		ufp_ixgbevf_rx_configure_ring(ih, i, &ih->rx_ring[i]);

	return 0;

err_set_rlpml:
err_xcast_mode:
err_promisc:
	return -1;
}

