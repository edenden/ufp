

void ufp_fm10k_configure_irq(struct ufp_handle *ih, uint32_t rate)
{
	uint32_t itr, txint, rxint;
	int vector, queue_idx;

	/* Enable auto-mask and clear the current mask */
	itr = FM10K_ITR_ENABLE;

	/* Store Tx itr in timer slot 0 */
	itr |= (rate & FM10K_ITR_MAX);

	/* Shift Rx itr to timer slot 1 */
	itr |= (rate & FM10K_ITR_MAX) << FM10K_ITR_INTERVAL1_SHIFT;

	for(queue_idx = 0; queue_idx < ih->num_qps; queue_idx++){
		/* Enable q_vector */
		ufp_write_reg(FM10K_ITR(queue_idx), itr);
	}

	for(queue_idx = 0, vector = 0; queue_idx < ih->num_qps;
	queue_idx++, vector++){
		rxint = vector | FM10K_INT_MAP_TIMER1;
		fm10k_write_reg(hw, FM10K_RXINT(queue_idx), rxint);
	}

	for(queue_idx = 0; queue_idx < ih->num_qps;
	queue_idx++, vector++){
		txint = vector | FM10K_INT_MAP_TIMER0;
		fm10k_write_reg(hw, FM10K_TXINT(queue_idx), txint);
	}

	return 0;
}

void ufp_fm10k_configure_tx(struct ufp_handle *ih)
{
	int i;

	/* Setup the HW Tx Head and Tail descriptor pointers */
	for (i = 0; i < ih->num_qps; i++)
		ufp_fm10k_configure_tx_ring(ih, i);

	/* poll here to verify that Tx rings are now enabled */
	for (i = 0; i < ih->num_qps; i++)
		ufp_fm10k_enable_tx_ring(ih, i);
}

static void ufp_fm10k_configure_tx_ring(struct ufp_handle *ih,
	uint8_t reg_idx)
{
	uint64_t addr_dma;
	struct ufp_ring *ring;
	uint32_t size;
	u32 txdctl = BIT(FM10K_TXDCTL_MAX_TIME_SHIFT) | FM10K_TXDCTL_ENABLE;

	addr_dma = (uint64_t)ring->addr_dma;
	ring = &ih->tx_ring[i];
	size = ih->num_tx_desc * sizeof(struct ufp_fm10k_tx_desc);

	/* disable queue to avoid issues while updating state */
	fm10k_write_reg(hw, FM10K_TXDCTL(reg_idx), 0);
	fm10k_write_flush(hw);

	/* possible poll here to verify ring resources have been cleaned */

	/* set location and size for descriptor ring */
	fm10k_write_reg(hw, FM10K_TDBAL(reg_idx), addr_dma & DMA_BIT_MASK(32));
	fm10k_write_reg(hw, FM10K_TDBAH(reg_idx), addr_dma >> 32);
	fm10k_write_reg(hw, FM10K_TDLEN(reg_idx), size);

	/* reset head and tail pointers */
	fm10k_write_reg(hw, FM10K_TDH(reg_idx), 0);
	fm10k_write_reg(hw, FM10K_TDT(reg_idx), 0);

	/* store tail pointer */
	ring->tail = ih->bar + FM10K_TDT(reg_idx);

	/* enable use of FTAG bit in Tx descriptor, register is RO for VF */
	fm10k_write_reg(hw, FM10K_PFVTCTL(reg_idx),
			FM10K_PFVTCTL_FTAG_DESC_ENABLE);

	/* enable queue */
	fm10k_write_reg(hw, FM10K_TXDCTL(reg_idx), txdctl);

	return;
}

static void ufp_fm10k_enable_tx_ring(struct ufp_handle *ih,
	uint8_t reg_idx)
{
	int wait_loop = 10;
	struct timespec ts;
	u32 txdctl;

	/* poll to verify queue is enabled */
	do {
		usleep(&ts, 1000);
		txdctl = fm10k_read_reg(hw, FM10K_TXDCTL(reg_idx));
	} while (!(txdctl & FM10K_TXDCTL_ENABLE) && --wait_loop);
	if (!wait_loop)
		printf("Could not enable Tx Queue %d\n", reg_idx);

	return;
}

static s32 fm10k_configure_dglort_map_pf(struct fm10k_hw *hw,
					 struct fm10k_dglort_cfg *dglort)
{
	u16 glort, queue_count, vsi_count, pc_count;
	u16 vsi, queue, pc, q_idx;
	u32 txqctl, dglortdec, dglortmap;

	/* verify the dglort pointer */
	if (!dglort)
		return FM10K_ERR_PARAM;

	/* verify the dglort values */
	if ((dglort->idx > 7) || (dglort->rss_l > 7) || (dglort->pc_l > 3) ||
	    (dglort->vsi_l > 6) || (dglort->vsi_b > 64) ||
	    (dglort->queue_l > 8) || (dglort->queue_b >= 256))
		return FM10K_ERR_PARAM;

	/* determine count of VSIs and queues */
	queue_count = BIT(dglort->rss_l + dglort->pc_l);
	vsi_count = BIT(dglort->vsi_l + dglort->queue_l);
	glort = dglort->glort;
	q_idx = dglort->queue_b;

	/* configure SGLORT for queues */
	for (vsi = 0; vsi < vsi_count; vsi++, glort++) {
		for (queue = 0; queue < queue_count; queue++, q_idx++) {
			if (q_idx >= FM10K_MAX_QUEUES)
				break;

			fm10k_write_reg(hw, FM10K_TX_SGLORT(q_idx), glort);
			fm10k_write_reg(hw, FM10K_RX_SGLORT(q_idx), glort);
		}
	}

	/* determine count of PCs and queues */
	queue_count = BIT(dglort->queue_l + dglort->rss_l + dglort->vsi_l);
	pc_count = BIT(dglort->pc_l);

	/* configure PC for Tx queues */
	for (pc = 0; pc < pc_count; pc++) {
		q_idx = pc + dglort->queue_b;
		for (queue = 0; queue < queue_count; queue++) {
			if (q_idx >= FM10K_MAX_QUEUES)
				break;

			txqctl = fm10k_read_reg(hw, FM10K_TXQCTL(q_idx));
			txqctl &= ~FM10K_TXQCTL_PC_MASK;
			txqctl |= pc << FM10K_TXQCTL_PC_SHIFT;
			fm10k_write_reg(hw, FM10K_TXQCTL(q_idx), txqctl);

			q_idx += pc_count;
		}
	}

	/* configure DGLORTDEC */
	dglortdec = ((u32)(dglort->rss_l) << FM10K_DGLORTDEC_RSSLENGTH_SHIFT) |
		    ((u32)(dglort->queue_b) << FM10K_DGLORTDEC_QBASE_SHIFT) |
		    ((u32)(dglort->pc_l) << FM10K_DGLORTDEC_PCLENGTH_SHIFT) |
		    ((u32)(dglort->vsi_b) << FM10K_DGLORTDEC_VSIBASE_SHIFT) |
		    ((u32)(dglort->vsi_l) << FM10K_DGLORTDEC_VSILENGTH_SHIFT) |
		    ((u32)(dglort->queue_l));
	if (dglort->inner_rss)
		dglortdec |=  FM10K_DGLORTDEC_INNERRSS_ENABLE;

	/* configure DGLORTMAP */
	dglortmap = (dglort->idx == fm10k_dglort_default) ?
			FM10K_DGLORTMAP_ANY : FM10K_DGLORTMAP_ZERO;
	dglortmap <<= dglort->vsi_l + dglort->queue_l + dglort->shared_l;
	dglortmap |= dglort->glort;

	/* write values to hardware */
	fm10k_write_reg(hw, FM10K_DGLORTDEC(dglort->idx), dglortdec);
	fm10k_write_reg(hw, FM10K_DGLORTMAP(dglort->idx), dglortmap);

	return 0;
}

static void fm10k_configure_dglort(struct fm10k_intfc *interface)
{
	struct fm10k_dglort_cfg dglort = { 0 };
	struct fm10k_hw *hw = &interface->hw;
	int i;
	u32 mrqc;

	/* Fill out hash function seeds */
	for (i = 0; i < FM10K_RSSRK_SIZE; i++)
		fm10k_write_reg(hw, FM10K_RSSRK(0, i), interface->rssrk[i]);

	/* Write RETA table to hardware */
	for (i = 0; i < FM10K_RETA_SIZE; i++)
		fm10k_write_reg(hw, FM10K_RETA(0, i), interface->reta[i]);

	/* Generate RSS hash based on packet types, TCP/UDP
	 * port numbers and/or IPv4/v6 src and dst addresses
	 */
	mrqc =	FM10K_MRQC_IPV4 |
		FM10K_MRQC_TCP_IPV4 |
		FM10K_MRQC_UDP_IPV4 |
		FM10K_MRQC_IPV6 |
		FM10K_MRQC_TCP_IPV6 |
		FM10K_MRQC_UDP_IPV6;

	fm10k_write_reg(hw, FM10K_MRQC(0), mrqc);

	/* configure default DGLORT mapping for RSS/DCB */
	dglort.inner_rss = 1;
	dglort.rss_l = fls(interface->ring_feature[RING_F_RSS].mask);
	dglort.pc_l = fls(interface->ring_feature[RING_F_QOS].mask);
	hw->mac.ops.configure_dglort_map(hw, &dglort);

	/* assign GLORT per queue for queue mapped testing */
	if (interface->glort_count > 64) {
		memset(&dglort, 0, sizeof(dglort));
		dglort.inner_rss = 1;
		dglort.glort = interface->glort + 64;
		dglort.idx = fm10k_dglort_pf_queue;
		dglort.queue_l = fls(interface->num_rx_queues - 1);
		hw->mac.ops.configure_dglort_map(hw, &dglort);
	}

	/* assign glort value for RSS/DCB specific to this interface */
	memset(&dglort, 0, sizeof(dglort));
	dglort.inner_rss = 1;
	dglort.glort = interface->glort;
	dglort.rss_l = fls(interface->ring_feature[RING_F_RSS].mask);
	dglort.pc_l = fls(interface->ring_feature[RING_F_QOS].mask);
	/* configure DGLORT mapping for RSS/DCB */
	dglort.idx = fm10k_dglort_pf_rss;
#ifdef NETIF_F_HW_L2FW_DOFFLOAD
	if (interface->l2_accel)
		dglort.shared_l = fls(interface->l2_accel->size);
#endif
	hw->mac.ops.configure_dglort_map(hw, &dglort);
}

static void ufp_fm10k_configure_rx_ring(struct ufp_handle *ih,
	uint8_t reg_idx)
{
	uint64_t addr_dma;
	struct ufp_ring *ring;
	uint32_t size;

	u32 rxqctl, rxdctl = FM10K_RXDCTL_WRITE_BACK_MIN_DELAY;
	u32 srrctl = FM10K_SRRCTL_BUFFER_CHAINING_EN;
	u8 rx_pause = interface->rx_pause;

	addr_dma = (uint64_t)ring->addr_dma;
	ring = &ih->rx_ring[i];
	size = ih->num_rx_desc * sizeof(union fm10k_rx_desc);

	/* disable queue to avoid issues while updating state */
	rxqctl = fm10k_read_reg(hw, FM10K_RXQCTL(reg_idx));
	rxqctl &= ~FM10K_RXQCTL_ENABLE;
	fm10k_write_flush(hw);

	/* possible poll here to verify ring resources have been cleaned */

	/* set location and size for descriptor ring */
	fm10k_write_reg(hw, FM10K_RDBAL(reg_idx), addr_dma & DMA_BIT_MASK(32));
	fm10k_write_reg(hw, FM10K_RDBAH(reg_idx), addr_dma >> 32);
	fm10k_write_reg(hw, FM10K_RDLEN(reg_idx), size);

	/* reset head and tail pointers */
	fm10k_write_reg(hw, FM10K_RDH(reg_idx), 0);
	fm10k_write_reg(hw, FM10K_RDT(reg_idx), 0);

	/* store tail pointer */
	ring->tail = ih->bar + FM10K_RDT(reg_idx);

	/* Configure the Rx buffer size for one buff without split */
	srrctl |= FM10K_RX_BUFSZ >> FM10K_SRRCTL_BSIZEPKT_SHIFT;

	/* Configure the Rx ring to suppress loopback packets */
	srrctl |= FM10K_SRRCTL_LOOPBACK_SUPPRESS;
	fm10k_write_reg(hw, FM10K_SRRCTL(reg_idx), srrctl);

	/* Enable drop on empty */
	rxdctl |= FM10K_RXDCTL_DROP_ON_EMPTY;
	fm10k_write_reg(hw, FM10K_RXDCTL(reg_idx), rxdctl);

	/* enable queue */
	rxqctl = fm10k_read_reg(hw, FM10K_RXQCTL(reg_idx));
	rxqctl |= FM10K_RXQCTL_ENABLE;
	fm10k_write_reg(hw, FM10K_RXQCTL(reg_idx), rxqctl);
}

static void ufp_fm10k_configure_rx(struct ufp_handle *ih)
{
	int i;

	/* Configure RSS and DGLORT map */
	fm10k_configure_dglort(interface);

	/* Setup the HW Rx Head and Tail descriptor pointers */
	for (i = 0; i < ih->num_qps; i++)
		ufp_fm10k_configure_rx_ring(ih, i);

	/* possible poll here to verify that Rx rings are now enabled */
}

