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

	/* Configuration related functions*/
	ops->reset_hw		= ufp_ixgbevf_reset;
	ops->set_device_params	= ufp_ixgbevf_set_device_params;
	ops->configure_irq	= ufp_ixgbevf_configure_irq;
	ops->configure_tx	= ufp_ixgbevf_configure_tx;
	ops->configure_rx	= ufp_ixgbevf_configure_rx;
	ops->stop_adapter	= ufp_ixgbevf_stop_adapter;

	/* RxTx related functions */
	ops->unmask_queues	= ufp_ixgbevf_unmask_queues;
	ops->set_rx_desc	= ufp_ixgbevf_rx_desc_set;
	ops->check_rx_desc	= ufp_ixgbevf_rx_desc_check;
	ops->get_rx_desc	= ufp_ixgbevf_rx_desc_get;
	ops->set_tx_desc	= ufp_ixgbevf_tx_desc_set;
	ops->check_tx_desc	= ufp_ixgbevf_tx_desc_check;

	return 0;

err_alloc_data:
	return -1;
}

void ufp_ixgbevf_destroy(struct ufp_ops *ops)
{
	free(ops->data);
	return;
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

	ufp_write_reg(ih, IXGBE_VFCTRL, IXGBE_CTRL_RST);
	ufp_write_flush(ih);

	msleep(&ts, 50);

	/* we cannot reset while the RSTI / RSTD bits are asserted */
	timeout = data->mbx_timeout;
	while (!ufp_ixgbevf_mbx_check_for_rst(hw) && timeout) {
		timeout--;
		usleep(&ts, 5);
	}

	if (!timeout)
		goto err_check_for_rst;

	/* Reset VF registers to initial values */
	ufp_ixgbevf_clr_reg(ih);

	msgbuf[0] = IXGBE_VF_RESET;
	err = ufp_ixgbevf_mbx_write_posted(ih, msgbuf, 1);
	if(err < 0)
		goto err_write;

	msleep(&ts, 10);

	/*
	 * set our "perm_addr" based on info provided by PF
	 * also set up the mc_filter_type which is piggy backed
	 * on the mac address in word 3
	 */
	err = ufp_ixgbevf_mbx_read_posted(ih, msgbuf, IXGBE_VF_PERMADDR_MSG_LEN);
	if (err < 0)
		goto err_read;

	if (msgbuf[0] != (IXGBE_VF_RESET | IXGBE_VT_MSGTYPE_ACK))
		goto err_reset_failed;

	memcpy(ih->mac_addr, &msgbuf[1], 6);

	/* Currently mc filter is not used */
	mc_filter_type = msgbuf[IXGBE_VF_MC_TYPE_WORD];

	err = ufp_ixgbevf_negotiate_api(ih);
	if(err < 0)
		goto err_nego_api;

	return 0;

err_nego_api:
err_reset_failed:
err_read:
err_write:
err_check_for_rst:
err_stop_adapter:
	return -1;
}

static int ufp_ixgbevf_set_device_params(struct ufp_handle *ih,
	uint32_t num_queues_req, uint32_t num_rx_desc, uint32_t num_tx_desc)
{
	int err;

	err = ufp_ixgbevf_get_queues(ih, num_queues_req);
	if(err < 0)
		goto err_get_queues;

	if(num_rx_desc > IXGBE_MAX_RXD)
		ih->num_rx_desc = IXGBE_MAX_RXD;
	else if(num_rx_desc < IXGBE_MIN_RXD)
		ih->num_rx_desc = IXGBE_MIN_RXD;
	else
		ih->num_rx_desc = num_rx_desc;

	if(num_tx_desc > IXGBE_MAX_TXD)
		ih->num_tx_desc = IXGBE_MAX_TXD;
	else if(num_tx_desc < IXGBE_MAX_TXD)
                ih->num_tx_desc = IXGBE_MIN_TXD;
	else
		ih->num_tx_desc = num_tx_desc;

	ih->size_rx_desc = sizeof(union ixgbe_adv_rx_desc) * ih->num_rx_desc;
	ih->size_tx_desc = sizeof(union ixgbe_adv_tx_desc) * ih->num_tx_desc;

	ih->buf_size = IXGBEVF_RX_BUFSZ;

	return 0;

err_get_queues:
	return -1;
}

static int ufp_ixgbevf_configure_irq(struct ufp_handle *ih, uint32_t rate)
{
	unsigned int qmask = 0;

	for(queue_idx = 0, vector = 0; queue_idx < ih->num_queues;
	queue_idx++, vector++){
		/* set RX queue interrupt */
		ufp_set_ivar(ih, 0, queue_idx, vector);
		ufp_write_eitr(ih, vector, rate);
		qmask |= 1 << vector;
	}

	for(queue_idx = 0; queue_idx < ih->num_queues;
	queue_idx++, vector++){
		/* set TX queue interrupt */
		ufp_set_ivar(ih, 1, queue_idx, vector);
		ufp_write_eitr(ih, vector, rate);
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

static int ufp_ixgbevf_configure_rx(struct ufp_handle *ih,
	uint32_t mtu_frame, uint32_t promisc)
{
	int i, err;

	/* VF can't support promiscuous mode */
	if(promisc)
		goto err_promisc;

	ih->promisc = promisc;

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

	err = ufp_ixgbevf_set_rlpml(ih, mtu_frame);
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

static void ufp_ixgbevf_rx_desc_set(struct ufp_ring *rx_ring, uint16_t index,
	uint64_t addr_dma)
{
	union ufp_ixgbevf_rx_desc *rx_desc;

	rx_desc = IXGBE_RX_DESC(rx_ring, index);

	rx_desc->read.pkt_addr = htole64(addr_dma);
	rx_desc->read.hdr_addr = 0;

	return;
}

static void ufp_ixgbevf_tx_desc_set(struct ufp_ring *tx_ring, uint16_t index,
	uint64_t addr_dma, struct ufp_packet *packet)
{
	union ufp_ixgbevf_tx_desc *tx_desc;
	uint32_t cmd_type;
	uint32_t olinfo_status;

	/* XXX: Each buffer size must be less than IXGBE_MAX_DATA_PER_TXD */

	/* set type for advanced descriptor with frame checksum insertion */
	tx_desc = IXGBE_TX_DESC(tx_ring, index);

	cmd_type =	IXGBE_ADVTXD_DTYP_DATA |
			IXGBE_ADVTXD_DCMD_DEXT |
			IXGBE_ADVTXD_DCMD_IFCS;

	if(unlikely(packet->flag & UFP_PACKET_NOTEOP)){
		cmd_type |= packet->slot_size;
		olinfo_status = 0;
	}else{
		cmd_type |= packet->slot_size | IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS;
		olinfo_status =	(packet->slot_size << IXGBE_ADVTXD_PAYLEN_SHIFT) |
				IXGBE_ADVTXD_CC;
	}

	tx_desc->read.buffer_addr = htole64(addr_dma);
	tx_desc->read.cmd_type_len = htole32(cmd_type);
	tx_desc->read.olinfo_status = htole32(olinfo_status);

	return;
}

static inline uint32_t ufp_test_staterr(union ufp_adv_rx_desc *rx_desc,
        const uint32_t stat_err_bits)
{
	return rx_desc->wb.upper.status_error & htole32(stat_err_bits);
}

static int ufp_ixgbevf_rx_desc_check(struct ufp_ring *rx_ring, uint16_t index)
{
	union ufp_ixgbevf_rx_desc *rx_desc;

	rx_desc = IXGBE_RX_DESC(rx_ring, index);

	if (!ufp_test_staterr(rx_desc, IXGBE_RXD_STAT_DD)){
		goto not_received;
	}

	return 0;

not_received:
	return -1;
}

static void ufp_ixgbevf_rx_desc_get(struct ufp_ring *rx_ring, uint16_t index,
	struct ufp_packet *packet)
{
	union ufp_ixgbevf_rx_desc *rx_desc;

	rx_desc = IXGBE_RX_DESC(rx_ring, index);

	packet->flag = 0;

	if(unlikely(!ufp_test_staterr(rx_desc, IXGBE_RXD_STAT_EOP)))
		packet->flag |= UFP_PAKCET_NOTEOP;

	if(unlikely(ufp_test_staterr(rx_desc,
		IXGBE_RXDADV_ERR_FRAME_ERR_MASK)))
		packet->flag |= UFP_PAKCET_ERROR;

	packet->slot_size = le16toh(rx_desc->wb.upper.length);

	return;
}

static int ufp_ixgbevf_tx_desc_check(struct ufp_ring *tx_ring, uint16_t index)
{
	union ufp_ixgbevf_tx_desc *tx_desc;

	tx_desc = IXGBE_TX_DESC(tx_ring, index);

	if (!(tx_desc->wb.status & htole32(IXGBE_TXD_STAT_DD)))
		goto not_sent;

	return 0;

not_sent:
	return -1;
}

static void ufp_ixgbevf_unmask_queues(void *bar, uint64_t qmask)
{
	uint32_t qmask_low;

	qmask_low = (qmask & 0xFFFFFFFF);
	if(qmask_low)
		ufp_writel(qmask, bar + IXGBE_VTEIMS);

        return;
}
