

void ufp_fm10k_configure_irq(struct ufp_handle *ih, uint32_t rate)
{
	uint32_t itr;
	int queue_idx;

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

	return 0;
}

static void fm10k_configure_tx(struct fm10k_intfc *interface)
{
	int i;

	/* Setup the HW Tx Head and Tail descriptor pointers */
	for (i = 0; i < interface->num_tx_queues; i++)
		fm10k_configure_tx_ring(interface, interface->tx_ring[i]);

	/* poll here to verify that Tx rings are now enabled */
	for (i = 0; i < interface->num_tx_queues; i++)
		fm10k_enable_tx_ring(interface, interface->tx_ring[i]);
}

 **/
static void fm10k_configure_tx_ring(struct fm10k_intfc *interface,
				    struct fm10k_ring *ring)
{
	struct fm10k_hw *hw = &interface->hw;
	u64 tdba = ring->dma;
	u32 size = ring->count * sizeof(struct fm10k_tx_desc);
	u32 txint = FM10K_INT_MAP_DISABLE;
	u32 txdctl = BIT(FM10K_TXDCTL_MAX_TIME_SHIFT) | FM10K_TXDCTL_ENABLE;
	u8 reg_idx = ring->reg_idx;

	/* disable queue to avoid issues while updating state */
	fm10k_write_reg(hw, FM10K_TXDCTL(reg_idx), 0);
	fm10k_write_flush(hw);

	/* possible poll here to verify ring resources have been cleaned */

	/* set location and size for descriptor ring */
	fm10k_write_reg(hw, FM10K_TDBAL(reg_idx), tdba & DMA_BIT_MASK(32));
	fm10k_write_reg(hw, FM10K_TDBAH(reg_idx), tdba >> 32);
	fm10k_write_reg(hw, FM10K_TDLEN(reg_idx), size);

	/* reset head and tail pointers */
	fm10k_write_reg(hw, FM10K_TDH(reg_idx), 0);
	fm10k_write_reg(hw, FM10K_TDT(reg_idx), 0);

	/* store tail pointer */
	ring->tail = &interface->uc_addr[FM10K_TDT(reg_idx)];

	/* reset ntu and ntc to place SW in sync with hardware */
	ring->next_to_clean = 0;
	ring->next_to_use = 0;

	/* Map interrupt */
	if (ring->q_vector) {
		txint = ring->q_vector->v_idx + NON_Q_VECTORS(hw);
		txint |= FM10K_INT_MAP_TIMER0;
	}

	fm10k_write_reg(hw, FM10K_TXINT(reg_idx), txint);

	/* enable use of FTAG bit in Tx descriptor, register is RO for VF */
	fm10k_write_reg(hw, FM10K_PFVTCTL(reg_idx),
			FM10K_PFVTCTL_FTAG_DESC_ENABLE);

	/* Initialize XPS */
	if (!test_and_set_bit(__FM10K_TX_XPS_INIT_DONE, &ring->state) &&
	    ring->q_vector)
		netif_set_xps_queue(ring->netdev,
				    &ring->q_vector->affinity_mask,
				    ring->queue_index);

	/* enable queue */
	fm10k_write_reg(hw, FM10K_TXDCTL(reg_idx), txdctl);
}

static void fm10k_enable_tx_ring(struct fm10k_intfc *interface,
				 struct fm10k_ring *ring)
{
	struct fm10k_hw *hw = &interface->hw;
	int wait_loop = 10;
	u32 txdctl;
	u8 reg_idx = ring->reg_idx;

	/* if we are already enabled just exit */
	if (fm10k_read_reg(hw, FM10K_TXDCTL(reg_idx)) & FM10K_TXDCTL_ENABLE)
		return;

	/* poll to verify queue is enabled */
	do {
		usleep_range(1000, 2000);
		txdctl = fm10k_read_reg(hw, FM10K_TXDCTL(reg_idx));
	} while (!(txdctl & FM10K_TXDCTL_ENABLE) && --wait_loop);
	if (!wait_loop)
		netif_err(interface, drv, interface->netdev,
			  "Could not enable Tx Queue %d\n", reg_idx);
}

static void fm10k_configure_swpri_map(struct fm10k_intfc *interface)
{
	struct net_device *netdev = interface->netdev;
	struct fm10k_hw *hw = &interface->hw;
	int i;

	/* clear flag indicating update is needed */
	interface->flags &= ~FM10K_FLAG_SWPRI_CONFIG;

	/* these registers are only available on the PF */
	if (hw->mac.type != fm10k_mac_pf)
		return;

	/* configure SWPRI to PC map */
	for (i = 0; i < FM10K_SWPRI_MAX; i++)
		fm10k_write_reg(hw, FM10K_SWPRI_MAP(i),
				netdev_get_prio_tc_map(netdev, i));
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
	mrqc = FM10K_MRQC_IPV4 |
	       FM10K_MRQC_TCP_IPV4 |
	       FM10K_MRQC_IPV6 |
	       FM10K_MRQC_TCP_IPV6;

	if (interface->flags & FM10K_FLAG_RSS_FIELD_IPV4_UDP)
		mrqc |= FM10K_MRQC_UDP_IPV4;
	if (interface->flags & FM10K_FLAG_RSS_FIELD_IPV6_UDP)
		mrqc |= FM10K_MRQC_UDP_IPV6;

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

static void fm10k_configure_rx_ring(struct fm10k_intfc *interface,
				    struct fm10k_ring *ring)
{
	u64 rdba = ring->dma;
	struct fm10k_hw *hw = &interface->hw;
	u32 size = ring->count * sizeof(union fm10k_rx_desc);
	u32 rxqctl, rxdctl = FM10K_RXDCTL_WRITE_BACK_MIN_DELAY;
	u32 srrctl = FM10K_SRRCTL_BUFFER_CHAINING_EN;
	u32 rxint = FM10K_INT_MAP_DISABLE;
	u8 rx_pause = interface->rx_pause;
	u8 reg_idx = ring->reg_idx;

	/* disable queue to avoid issues while updating state */
	rxqctl = fm10k_read_reg(hw, FM10K_RXQCTL(reg_idx));
	rxqctl &= ~FM10K_RXQCTL_ENABLE;
	fm10k_write_flush(hw);

	/* possible poll here to verify ring resources have been cleaned */

	/* set location and size for descriptor ring */
	fm10k_write_reg(hw, FM10K_RDBAL(reg_idx), rdba & DMA_BIT_MASK(32));
	fm10k_write_reg(hw, FM10K_RDBAH(reg_idx), rdba >> 32);
	fm10k_write_reg(hw, FM10K_RDLEN(reg_idx), size);

	/* reset head and tail pointers */
	fm10k_write_reg(hw, FM10K_RDH(reg_idx), 0);
	fm10k_write_reg(hw, FM10K_RDT(reg_idx), 0);

	/* store tail pointer */
	ring->tail = &interface->uc_addr[FM10K_RDT(reg_idx)];

	/* reset ntu and ntc to place SW in sync with hardware */
	ring->next_to_clean = 0;
	ring->next_to_use = 0;
	ring->next_to_alloc = 0;

	/* Configure the Rx buffer size for one buff without split */
	srrctl |= FM10K_RX_BUFSZ >> FM10K_SRRCTL_BSIZEPKT_SHIFT;

	/* Configure the Rx ring to suppress loopback packets */
	srrctl |= FM10K_SRRCTL_LOOPBACK_SUPPRESS;
	fm10k_write_reg(hw, FM10K_SRRCTL(reg_idx), srrctl);

	/* Enable drop on empty */
#ifdef HAVE_DCBNL_IEEE
#ifdef CONFIG_DCB
	if (interface->pfc_en)
		rx_pause = interface->pfc_en;
#endif
#endif /* HAVE_DCBNL_IEEE */
	if (!(rx_pause & BIT(ring->qos_pc)))
		rxdctl |= FM10K_RXDCTL_DROP_ON_EMPTY;

	fm10k_write_reg(hw, FM10K_RXDCTL(reg_idx), rxdctl);

#ifndef HAVE_VLAN_RX_REGISTER
	/* assign default VLAN to queue */
	ring->vid = hw->mac.default_vid;

	/* if we have an active VLAN, disable default VLAN ID */
	if (test_bit(hw->mac.default_vid, interface->active_vlans))
		ring->vid |= FM10K_VLAN_CLEAR;
#endif

	/* Map interrupt */
	if (ring->q_vector) {
		rxint = ring->q_vector->v_idx + NON_Q_VECTORS(hw);
		rxint |= FM10K_INT_MAP_TIMER1;
	}

	fm10k_write_reg(hw, FM10K_RXINT(reg_idx), rxint);

	/* enable queue */
	rxqctl = fm10k_read_reg(hw, FM10K_RXQCTL(reg_idx));
	rxqctl |= FM10K_RXQCTL_ENABLE;
	fm10k_write_reg(hw, FM10K_RXQCTL(reg_idx), rxqctl);

	/* place buffers on ring for receive data */
	fm10k_alloc_rx_buffers(ring, fm10k_desc_unused(ring));
}

static void fm10k_configure_rx(struct fm10k_intfc *interface)
{
	int i;

	/* Configure SWPRI to PC map */
	fm10k_configure_swpri_map(interface);

	/* Configure RSS and DGLORT map */
	fm10k_configure_dglort(interface);

	/* Setup the HW Rx Head and Tail descriptor pointers */
	for (i = 0; i < interface->num_rx_queues; i++)
		fm10k_configure_rx_ring(interface, interface->rx_ring[i]);

	/* possible poll here to verify that Rx rings are now enabled */
}

