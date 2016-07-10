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

void ufp_ixgbevf_stop_adapter(struct ufp_handle *ih)
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

void ufp_ixgbevf_set_eitr(struct ufp_handle *ih, int vector)
{
	uint32_t itr_reg;

	itr_reg = ih->num_interrupt_rate & IXGBE_MAX_EITR;

	/*
	 * set the WDIS bit to not clear the timer bits and cause an
	 * immediate assertion of the interrupt
	 */
	itr_reg |= IXGBE_EITR_CNT_WDIS;
	ufp_write_reg(ih, IXGBE_VTEITR(vector), itr_reg);
}

void ufp_ixgbevf_set_ivar(struct ufp_handle *ih,
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
	if (msgbuf[0] != (IXGBE_VF_UPDATE_XCAST_MODE | IXGBE_VT_MSGTYPE_ACK))
		goto err_ack;

	return 0;

err_ack:
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

int ufp_ixgbevf_set_rlpml(struct ufp_handle *ih)
{
	uint32_t msgbuf[2];
	uint32_t retmsg[IXGBE_VFMAILBOX_SIZE];
	int err;

	/* adjust max frame to be at least the size of a standard frame */
	if(ih->mtu_frame < (VLAN_ETH_FRAME_LEN + ETH_FCS_LEN))
		ih->mtu_frame = (VLAN_ETH_FRAME_LEN + ETH_FCS_LEN);

	if(ih->mtu_frame > IXGBE_MAX_JUMBO_FRAME_SIZE)
		ih->mtu_frame = IXGBE_MAX_JUMBO_FRAME_SIZE;

	msgbuf[0] = IXGBE_VF_SET_LPE;
	msgbuf[1] = ih->mtu_frame;

        err = mbx->ops.write_posted(hw, msgbuf, 2);
	if(err)
		goto err_write;

	err = mbx->ops.read_posted(hw, retmsg, 2);
	if(err)
		goto err_read;

	msgbuf[0] &= ~IXGBE_VT_MSGTYPE_CTS;
	if (msgbuf[0] != (IXGBE_VF_SET_LPE | IXGBE_VT_MSGTYPE_ACK))
		goto err_ack;

	return 0;

err_ack:
err_read:
err_write:
	return -1;
}

static void ixgbevf_configure_tx_ring(struct ixgbevf_adapter *adapter,
                                      struct ixgbevf_ring *ring)
{
        struct ixgbe_hw *hw = &adapter->hw;
        u64 tdba = ring->dma;
        int wait_loop = 10;
        u32 txdctl = IXGBE_TXDCTL_ENABLE;
        u8 reg_idx = ring->reg_idx;

        /* disable queue to avoid issues while updating state */
        IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(reg_idx), IXGBE_TXDCTL_SWFLSH);
        IXGBE_WRITE_FLUSH(hw);

        IXGBE_WRITE_REG(hw, IXGBE_VFTDBAL(reg_idx), tdba & DMA_BIT_MASK(32));
        IXGBE_WRITE_REG(hw, IXGBE_VFTDBAH(reg_idx), tdba >> 32);
        IXGBE_WRITE_REG(hw, IXGBE_VFTDLEN(reg_idx),
                        ring->count * sizeof(union ixgbe_adv_tx_desc));

        /* disable head writeback */
        IXGBE_WRITE_REG(hw, IXGBE_VFTDWBAH(reg_idx), 0);
        IXGBE_WRITE_REG(hw, IXGBE_VFTDWBAL(reg_idx), 0);


        /* enable relaxed ordering */
        IXGBE_WRITE_REG(hw, IXGBE_VFDCA_TXCTRL(reg_idx),
                        (IXGBE_DCA_TXCTRL_DESC_RRO_EN |
                         IXGBE_DCA_TXCTRL_DATA_RRO_EN));

        /* reset head and tail pointers */
        IXGBE_WRITE_REG(hw, IXGBE_VFTDH(reg_idx), 0);
        IXGBE_WRITE_REG(hw, IXGBE_VFTDT(reg_idx), 0);
        ring->tail = adapter->io_addr + IXGBE_VFTDT(reg_idx);

        /* reset ntu and ntc to place SW in sync with hardwdare */
        ring->next_to_clean = 0;
        ring->next_to_use = 0;

        /* set WTHRESH to encourage burst writeback, it should not be set
         * higher than 1 when ITR is 0 as it could cause false TX hangs
         *
         * In order to avoid issues WTHRESH + PTHRESH should always be equal
         * to or less than the number of on chip descriptors, which is
         * currently 40.
         */
        if (!ring->q_vector || (ring->q_vector->itr < 8))
                txdctl |= (1 << 16);    /* WTHRESH = 1 */
        else
                txdctl |= (8 << 16);    /* WTHRESH = 8 */

        /* Setting PTHRESH to 32 both improves performance */
        txdctl |= (1 << 8) |    /* HTHRESH = 1 */
                   32;          /* PTHRESH = 32 */

        clear_bit(__IXGBEVF_HANG_CHECK_ARMED, &ring->state);

        IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(reg_idx), txdctl);

        /* poll to verify queue is enabled */
        do {
                msleep(1);
                txdctl = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(reg_idx));
        }  while (--wait_loop && !(txdctl & IXGBE_TXDCTL_ENABLE));
        if (!wait_loop)
                DPRINTK(PROBE, ERR, "Could not enable Tx Queue %d\n", reg_idx);
}

static void ufp_ixgbevf_disable_rx_queue(struct ufp_handle *ih, uint8_t reg_idx)
{
	int wait_loop = IXGBEVF_MAX_RX_DESC_POLL;
        uint32_t rxdctl;
	struct timespec ts;

	ts.tv_sec = 0;
	ts.tv_nsec = 10000;

	rxdctl = ufp_read_reg(ih, IXGBE_VFRXDCTL(reg_idx));
	rxdctl &= ~IXGBE_RXDCTL_ENABLE;

	/* write value back with RXDCTL.ENABLE bit cleared */
	ufp_write_reg(ih, IXGBE_VFRXDCTL(reg_idx), rxdctl);

	/* the hardware may take up to 100us to really disable the rx queue */
	do {
		nanosleep(&ts, NULL);
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

	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;

	do {
		nanosleep(&ts, NULL);
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
