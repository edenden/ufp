#include <linux/types.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "ufp_ixgbevf.h"
#include "ufp_ixgbevf_mbx.h"

static int ufp_ixgbevf_negotiate_api(struct ufp_handle *ih);
static int ufp_ixgbevf_get_queues(struct ufp_handle *ih);

static int ufp_ixgbevf_get_intr_rate(struct ufp_handle *ih);

static void ufp_ixgbevf_stop_adapter(struct ufp_handle *ih);
static void ufp_ixgbevf_clr_reg(struct ufp_handle *ih)
static int ufp_ixgbevf_reset(struct ufp_handle *ih);

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
	ops->get_intr_rate	= ufp_ixgbevf_get_intr_rate;

	return 0;

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

static int ufp_ixgbevf_get_intr_rate(struct ufp_handle *ih)
{
	ih->num_interrupt_rate = IXGBE_MAX_EITR;
	return 0;
}

static void ufp_mac_stop_adapter(struct ufp_handle *ih)
{
        uint32_t reg_val;
        uint16_t i;

	/* Clear interrupt mask to stop from interrupts being generated */
	ufp_write_reg(ih, IXGBE_VTEIMC, IXGBE_VF_IRQ_CLEAR_MASK);

	/* Clear any pending interrupts, flush previous writes */
	ufp_read_reg(ih, IXGBE_VTEICR);

	/* Disable the transmit unit.  Each queue must be disabled. */
	for (i = 0; i < ih->num_queues; i++)
		ufp_write_reg(ih, IXGBE_VFTXDCTL(i), IXGBE_TXDCTL_SWFLSH);

	/* Disable the receive unit by stopping each queue */
	for (i = 0; i < ih->num_queues; i++) {
		reg_val = ufp_read_reg(ih, IXGBE_VFRXDCTL(i));
		reg_val &= ~IXGBE_RXDCTL_ENABLE;
		ufp_write_reg(ih, IXGBE_VFRXDCTL(i), reg_val);
	}
	/* Clear packet split and pool config */
	ufp_write_reg(ih, IXGBE_VFPSRTYPE, 0);

	/* flush all queues disables */
	ufp_write_flush(ih);
	msleep(2);

	return 0;
}

static void ufp_ixgbevf_clr_reg(struct ufp_handle *ih)
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

	ufp_write_reg(ih, IXGBE_VFPSRTYPE, 0);

	for (i = 0; i < 7; i++) {
		ufp_write_reg(ih, IXGBE_VFRDH(i), 0);
		ufp_write_reg(ih, IXGBE_VFRDT(i), 0);
		ufp_write_reg(ih, IXGBE_VFRXDCTL(i), 0);
		ufp_write_reg(ih, IXGBE_VFSRRCTL(i), vfsrrctl);
		ufp_write_reg(ih, IXGBE_VFTDH(i), 0);
		ufp_write_reg(ih, IXGBE_VFTDT(i), 0);
		ufp_write_reg(ih, IXGBE_VFTXDCTL(i), 0);
		ufp_write_reg(ih, IXGBE_VFTDWBAH(i), 0);
		ufp_write_reg(ih, IXGBE_VFTDWBAL(i), 0);
		ufp_write_reg(ih, IXGBE_VFDCA_RXCTRL(i), ufpca_rxctrl);
		ufp_write_reg(ih, IXGBE_VFDCA_TXCTRL(i), ufpca_txctrl);
	}

	ufp_write_flush(ih);
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

static void ufp_write_eitr(struct ufp_port *port, int vector)
{
	struct ufp_hw *hw = port->hw;
	uint32_t itr_reg = port->num_interrupt_rate & IXGBE_MAX_EITR;

	/*
	 * set the WDIS bit to not clear the timer bits and cause an
	 * immediate assertion of the interrupt
	 */
	itr_reg |= IXGBE_EITR_CNT_WDIS;
	ufp_write_reg(hw, IXGBE_VTEITR(vector), itr_reg);
}

static void ufp_set_ivar(struct ufp_port *port,
        int8_t direction, uint8_t queue, uint8_t msix_vector)
{
	uint32_t ivar, index;
	struct ufp_hw *hw = port->hw;

	/* tx or rx causes */
	msix_vector |= IXGBE_IVAR_ALLOC_VAL;
	index = ((16 * (queue & 1)) + (8 * direction));
	ivar = ufp_read_reg(hw, IXGBE_VTIVAR(queue >> 1));
	ivar &= ~(0xFF << index);
	ivar |= (msix_vector << index);
	ufp_write_reg(hw, IXGBE_VTIVAR(queue >> 1), ivar);
}

static int ufp_ixgbevf_intr_configure(struct ufp_handle *ih)
{
	unsigned int qmask = 0;

	for(queue_idx = 0, vector = 0; queue_idx < port->num_rx_queues;
	queue_idx++, vector++, num_rx_requested++){
		/* set RX queue interrupt */
		ufp_set_ivar(port, 0, queue_idx, vector);
		ufp_write_eitr(port, vector);
		qmask |= 1 << vector;
	}

	for(queue_idx = 0; queue_idx < port->num_tx_queues;
	queue_idx++, vector++, num_tx_requested++){
		/* set TX queue interrupt */
		ufp_set_ivar(port, 1, queue_idx, vector);
		ufp_write_eitr(port, vector);
		qmask |= 1 << vector;
	}

	/* clear any pending interrupts, may auto mask */
	IXGBE_READ_REG(hw, IXGBE_VTEICR);

	ufp_write_reg(hw, IXGBE_VTEIAM, qmask);
	ufp_write_reg(hw, IXGBE_VTEIAC, qmask);
	ufp_write_reg(hw, IXGBE_VTEIMS, qmask);
}

static int32_t ufp_mac_update_xcast_mode(struct ufp_hw *hw, int xcast_mode)
{
	struct ufp_mbx_info *mbx = hw->mbx;
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

	err = mbx->ops.write_posted(hw, msgbuf, 2);
	if (err)
		return err;

	err = mbx->ops.read_posted(hw, msgbuf, 2);
	if (err)
		return err;

	msgbuf[0] &= ~IXGBE_VT_MSGTYPE_CTS;
	if (msgbuf[0] == (IXGBE_VF_UPDATE_XCAST_MODE | IXGBE_VT_MSGTYPE_NACK))
		return -EPERM;

	return 0;
}

static int32_t ufp_mac_set_rlpml(struct ufp_hw *hw, uint16_t max_size)
{
	struct ufp_mbx_info *mbx = hw->mbx;
	uint32_t msgbuf[2];
	uint32_t retmsg[IXGBE_VFMAILBOX_SIZE];
	int32_t err;

	msgbuf[0] = IXGBE_VF_SET_LPE;
	msgbuf[1] = max_size;

        err = mbx->ops.write_posted(hw, msgbuf, 2);
	if(err)
		goto err_write;

	err = mbx->ops.read_posted(hw, retmsg, 2);
	if(err)
		goto err_read;

	return 0;

err_read:
err_write:
	return -1;
}

