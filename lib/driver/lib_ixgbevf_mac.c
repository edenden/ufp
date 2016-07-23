#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

#include "lib_ixgbevf.h"
#include "lib_ixgbevf_mac.h"

static void ufp_ixgbevf_set_eitr(struct ufp_handle *ih, int vector, uint32_t rate);
static void ufp_ixgbevf_set_ivar(struct ufp_handle *ih,
	int8_t direction, uint8_t queue, uint8_t msix_vector);
static void ufp_ixgbevf_disable_rx_queue(struct ufp_handle *ih, uint8_t reg_idx);
static void ufp_ixgbevf_configure_srrctl(struct ufp_handle *ih, uint8_t reg_idx);
static void ufp_ixgbevf_rx_desc_queue_enable(struct ufp_handle *ih,
	uint8_t reg_idx);

void ufp_ixgbevf_clr_reg(struct ufp_handle *ih)
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

	/* XXX: Why '7' iterations are hard-coded? */
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

int ufp_ixgbevf_negotiate_api(struct ufp_handle *ih)
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

int ufp_ixgbevf_get_queues(struct ufp_handle *ih, uint32_t num_queues_req)
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
		ih->num_queues = min(num_queues_req, 2);
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
	ih->num_queues = min(num_queues_req, ih->num_queues);

	return 0;

err_get_queues:
err_read:
err_write:
	return -1;
}

static void ufp_ixgbevf_set_eitr(struct ufp_handle *ih, int vector, uint32_t rate)
{
	uint32_t itr_reg;

	itr_reg = rate & IXGBE_MAX_EITR;
	ih->irq_rate = itr_reg;

	/*
	 * set the WDIS bit to not clear the timer bits and cause an
	 * immediate assertion of the interrupt
	 */
	itr_reg |= IXGBE_EITR_CNT_WDIS;
	ufp_write_reg(ih, IXGBE_VTEITR(vector), itr_reg);

	return;
}

static void ufp_ixgbevf_set_ivar(struct ufp_handle *ih,
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
	ufp_write_reg(ih, IXGBE_VTIVAR(queue >> 1), ivar);
}

int ufp_ixgbevf_update_xcast_mode(struct ufp_handle *ih, int xcast_mode)
{
	struct ufp_ixgbevf_data *data;
	uint32_t msgbuf[2];
	int err;

	data = ih->ops.data;

	switch(data->api_version) {
	case ixgbe_mbox_api_12:
		break;
	default:
		goto err_not_supported;
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
	if (msgbuf[0] != (IXGBE_VF_UPDATE_XCAST_MODE | IXGBE_VT_MSGTYPE_ACK))
		goto err_ack;

	return 0;

err_ack:
err_not_supported:
	return -1;
}

void ufp_ixgbevf_set_psrtype(struct ufp_handle *ih)
{
	uint32_t psrtype;

	/* PSRTYPE must be initialized in 82599 */
	psrtype = IXGBE_PSRTYPE_TCPHDR | IXGBE_PSRTYPE_UDPHDR |
		IXGBE_PSRTYPE_IPV4HDR | IXGBE_PSRTYPE_IPV6HDR |
		IXGBE_PSRTYPE_L2HDR;

	if(ih->num_queues > 1)
		psrtype |= 1 << 29;

	ufp_write_reg(ih, IXGBE_VFPSRTYPE, psrtype);
}

void ufp_ixgbevf_set_vfmrqc(struct ufp_handle *ih)
{
	uint32_t vfmrqc = 0, vfreta = 0;
	uint16_t rss_i = ih->num_queues;
	int i, j;

	/* Fill out hash function seeds */
	for(i = 0; i < IXGBEVF_VFRSSRK_REGS; i++)
		ufp_write_reg(ih, IXGBE_VFRSSRK(i), rand());

	for (i = 0, j = 0; i < IXGBEVF_X550_VFRETA_SIZE; i++, j++) {
		if (j == rss_i)
			j = 0;

		vfreta |= j << (i & 0x3) * 8;
		if ((i & 3) == 3) {
			ufp_write_reg(ih, IXGBE_VFRETA(i >> 2), vfreta);
			vfreta = 0;
		}
	}

	/* Perform hash on these packet types */
	vfmrqc |= IXGBE_MRQC_RSS_FIELD_IPV4 |
		IXGBE_MRQC_RSS_FIELD_IPV4_TCP |
		IXGBE_MRQC_RSS_FIELD_IPV6 |
		IXGBE_MRQC_RSS_FIELD_IPV6_TCP;

	vfmrqc |= IXGBE_MRQC_RSSEN;

	ufp_write_reg(ih, IXGBE_VFMRQC, vfmrqc);
}

int ufp_ixgbevf_set_rlpml(struct ufp_handle *ih, uint32_t mtu_frame)
{
	uint32_t msgbuf[2];
	uint32_t retmsg[IXGBE_VFMAILBOX_SIZE];
	int err;

	/* adjust max frame to be at least the size of a standard frame */
	if(mtu_frame < (VLAN_ETH_FRAME_LEN + ETH_FCS_LEN))
		mtu_frame = (VLAN_ETH_FRAME_LEN + ETH_FCS_LEN);

	if(mtu_frame > IXGBE_MAX_JUMBO_FRAME_SIZE)
		mtu_frame = IXGBE_MAX_JUMBO_FRAME_SIZE;

	msgbuf[0] = IXGBE_VF_SET_LPE;
	msgbuf[1] = mtu_frame;

	err = mbx->ops.write_posted(hw, msgbuf, 2);
	if(err)
		goto err_write;

	err = mbx->ops.read_posted(hw, retmsg, 2);
	if(err)
		goto err_read;

	msgbuf[0] &= ~IXGBE_VT_MSGTYPE_CTS;
	if (msgbuf[0] != (IXGBE_VF_SET_LPE | IXGBE_VT_MSGTYPE_ACK))
		goto err_ack;

	ih->mtu_frame = mtu_frame;
	return 0;

err_ack:
err_read:
err_write:
	return -1;
}

void ixgbevf_configure_tx_ring(struct ufp_handle *ih,
	uint8_t reg_idx, struct ufp_ring *ring)
{
	int wait_loop = 10;
	uint32_t txdctl = IXGBE_TXDCTL_ENABLE;
	uint64_t addr_dma;
	struct timespec ts;

	addr_dma = (uint64_t)ring->addr_dma;

	/* disable queue to avoid issues while updating state */
	ufp_write_reg(ih, IXGBE_VFTXDCTL(reg_idx), IXGBE_TXDCTL_SWFLSH);
	ufp_write_flush(ih);

	ufp_write_reg(ih, IXGBE_VFTDBAL(reg_idx), addr_dma & DMA_BIT_MASK(32));
	ufp_write_reg(ih, IXGBE_VFTDBAH(reg_idx), addr_dma >> 32);
	ufp_write_reg(ih, IXGBE_VFTDLEN(reg_idx),
		ih->num_tx_desc * sizeof(union ixmap_adv_tx_desc));

	/* disable head writeback */
	ufp_write_reg(ih, IXGBE_VFTDWBAH(reg_idx), 0);
	ufp_write_reg(ih, IXGBE_VFTDWBAL(reg_idx), 0);


	/* enable relaxed ordering */
	ufp_write_reg(ih, IXGBE_VFDCA_TXCTRL(reg_idx),
		(IXGBE_DCA_TXCTRL_DESC_RRO_EN | IXGBE_DCA_TXCTRL_DATA_RRO_EN));

	/* reset head and tail pointers */
	ufp_write_reg(ih, IXGBE_VFTDH(reg_idx), 0);
	ufp_write_reg(ih, IXGBE_VFTDT(reg_idx), 0);

	ring->tail = ih->bar + IXGBE_TDT(reg_idx);

	/* set WTHRESH to encourage burst writeback, it should not be set
	 * higher than 1 when ITR is 0 as it could cause false TX hangs
	 *
	 * In order to avoid issues WTHRESH + PTHRESH should always be equal
	 * to or less than the number of on chip descriptors, which is
	 * currently 40.
	 */
	if(ih->irq_rate < 8)
		txdctl |= (1 << 16);	/* WTHRESH = 1 */
	else
		txdctl |= (8 << 16);	/* WTHRESH = 8 */

	/* Setting PTHRESH to 32 both improves performance */
	txdctl |= (1 << 8) |		/* HTHRESH = 1 */
		32;			/* PTHRESH = 32 */

	/* enable queue */
	ufp_write_reg(ih, IXGBE_VFTXDCTL(reg_idx), txdctl);

	/* poll to verify queue is enabled */
	do{
		msleep(&ts, 1);
		txdctl = ufp_read_reg(ih, IXGBE_VFTXDCTL(reg_idx));
	}while(--wait_loop && !(txdctl & IXGBE_TXDCTL_ENABLE));

	if (!wait_loop)
		printf("Could not enable Tx Queue %d\n", reg_idx);
	return;
}

static void ufp_ixgbevf_disable_rx_queue(struct ufp_handle *ih, uint8_t reg_idx)
{
	int wait_loop = IXGBEVF_MAX_RX_DESC_POLL;
	uint32_t rxdctl;
	struct timespec ts;

	rxdctl = ufp_read_reg(ih, IXGBE_VFRXDCTL(reg_idx));
	rxdctl &= ~IXGBE_RXDCTL_ENABLE;

	/* write value back with RXDCTL.ENABLE bit cleared */
	ufp_write_reg(ih, IXGBE_VFRXDCTL(reg_idx), rxdctl);

	/* the hardware may take up to 100us to really disable the rx queue */
	do {
		usleep(&ts, 10);
		rxdctl = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(reg_idx));
	} while (--wait_loop && (rxdctl & IXGBE_RXDCTL_ENABLE));

	if (!wait_loop) {
		printf("RXDCTL.ENABLE queue %d not cleared while polling\n",
			reg_idx);
	}
	return;
}

static void ufp_ixgbevf_configure_srrctl(struct ufp_handle *ih, uint8_t reg_idx)
{
	uint32_t srrctl;

	srrctl = IXGBE_SRRCTL_DROP_EN;

	/* As per the documentation for 82599 in order to support hardware RSC
	 * the header size must be set.
	 */
	srrctl |= IXGBEVF_RX_HDR_SIZE << IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT;
	srrctl |= IXGBEVF_RX_BUFSZ >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
	srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;

	ufp_write_reg(ih, IXGBE_VFSRRCTL(reg_idx), srrctl);
}

static void ufp_ixgbevf_rx_desc_queue_enable(struct ufp_handle *ih,
	uint8_t reg_idx)
{
	int wait_loop = IXGBEVF_MAX_RX_DESC_POLL;
	uint32_t rxdctl;
	struct timespec ts;

	do {
		msleep(&ts, 1);
		rxdctl = ufp_read_reg(ih, IXGBE_VFRXDCTL(reg_idx));
	} while (--wait_loop && !(rxdctl & IXGBE_RXDCTL_ENABLE));

	if (!wait_loop) {
		printf("RXDCTL.ENABLE queue %d not set while polling\n",
			reg_idx);
	}
	return;
}

void ufp_ixgbevf_configure_rx_ring(struct ufp_handle *ih,
	uint8_t reg_idx, struct ufp_ring *ring)
{
	uint32_t rxdctl;
	uint64_t addr_dma;

	addr_dma = (uint64_t)ring->addr_dma;

	/* disable queue to avoid issues while updating state */
	rxdctl = ufp_read_reg(ih, IXGBE_VFRXDCTL(reg_idx));
	ufp_ixgbevf_disable_rx_queue(adapter, reg_idx);

	ufp_write_reg(ih, IXGBE_VFRDBAL(reg_idx), addr_dma & DMA_BIT_MASK(32));
	ufp_write_reg(ih, IXGBE_VFRDBAH(reg_idx), addr_dma >> 32);
	ufp_write_reg(ih, IXGBE_VFRDLEN(reg_idx),
		ih->num_rx_desc * sizeof(union ixmap_adv_rx_desc);

	/* enable relaxed ordering */
	ufp_write_reg(ih, IXGBE_VFDCA_RXCTRL(reg_idx),
		IXGBE_DCA_RXCTRL_DESC_RRO_EN);

	/* reset head and tail pointers */
	ufp_write_reg(ih, IXGBE_VFRDH(reg_idx), 0);
	ufp_write_reg(ih, IXGBE_VFRDT(reg_idx), 0);
	ring->tail = ih->bar + IXGBE_VFRDT(reg_idx);

	ufp_ixgbevf_configure_srrctl(ih, reg_idx);

	/* allow any size packet since we can handle overflow */
	rxdctl &= ~IXGBE_RXDCTL_RLPML_EN;

	rxdctl |= IXGBE_RXDCTL_ENABLE | IXGBE_RXDCTL_VME;
	ufp_write_reg(ih, IXGBE_VFRXDCTL(reg_idx), rxdctl);

	ufp_ixgbevf_rx_desc_queue_enable(ih, reg_idx);
	return;
}
