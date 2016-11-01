int i40e_vsi_configure_tx(struct ufp_dev *dev,
	struct ufp_iface *iface)
{
	int i, err;

	for (i = 0; i < iface->num_qps; i++){
		err = i40e_configure_tx_ring(dev, iface, i);
		if(err < 0)
			goto err_configure_tx_ring;
	}

	return 0;

err_configure_tx_ring:
	return -1;
}

int i40e_vsi_configure_rx(struct ufp_dev *dev,
	struct ufp_iface *iface)
{
	int i, err;

	for (i = 0; i < iface->num_qps; i++){
		err = i40e_configure_rx_ring(dev, iface, i);
		if(err < 0)
			goto err_configure_rx_ring;
	}

	return 0;

err_configure_rx_ring:
	return -1;
}

static int i40e_configure_tx_ring(struct ufp_dev *dev,
	struct ufp_iface *iface, int ring_idx)
{
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	struct ufp_ring *ring = &iface->tx_ring[ring_idx];
	struct i40e_hmc_obj_txq tx_ctx;
	uint16_t queue_idx;
	uint32_t qtx_ctl;
	int err;

	/* clear the context structure first */
	memset(&tx_ctx, 0, sizeof(tx_ctx));
	queue_idx = i40e_iface->base_queue + ring_idx;

	/*
	 * See 8.4.3.4.2 - Transmit Queue Context in FPM
	 */
	tx_ctx.new_context = 1;
	tx_ctx.base = (ring->addr_dma / 128);
	tx_ctx.qlen = iface->num_tx_desc;

	/*
	 * This flag selects between Head WB and transmit descriptor WB:
	 * 0b - Descriptor Write Back
	 * 1b - Head Write Back
	 */
	tx_ctx.head_wb_ena = 0;

	/*
	 * See 1.1.4 - Transmit scheduler
	 * The XL710 provides management interfaces that allow
	 * each LAN transmit queue to be placed into a queue set.
	 * A queue set is a list of transmit queues that belong to the same TC
	 * and are treated equally by the XL710 transmit scheduler.
	 */
	tx_ctx.rdylist = le16_to_cpu(i40e_iface->qs_handle[0]);
	tx_ctx.rdylist_act = 0;

	/* clear the context in the HMC */
	err = i40e_clear_lan_tx_queue_context(hw, pf_q);
	if(err < 0)
		goto err_clear_lan_tx_queue_ctx;

	/* set the context in the HMC */
	err = i40e_set_lan_tx_queue_context(hw, pf_q, &tx_ctx);
	if(err < 0)
		goto err_set_lan_tx_queue_ctx;

	/* Now associate this queue with this PCI function */
	qtx_ctl = I40E_QTX_CTL_PF_QUEUE;
	qtx_ctl |= ((hw->pf_id << I40E_QTX_CTL_PF_INDX_SHIFT) &
		    I40E_QTX_CTL_PF_INDX_MASK);
	queue_idx = i40e_iface->base_queue + ring_idx;
	wr32(hw, I40E_QTX_CTL(queue_idx), qtx_ctl);
	i40e_flush(dev);

	/* cache tail off for easier writes later */
	ring->tail = dev->bar + I40E_QTX_TAIL(queue_idx);

	return 0;
}

static int i40e_configure_rx_ring(struct ufp_dev *dev,
	struct ufp_iface *iface, int ring_idx)
{
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	struct ufp_ring *ring = &iface->rx_ring[ring_idx];
	struct i40e_hmc_obj_rxq rx_ctx;
	uint16_t queue_idx;
	uint32_t qtx_ctl;
	int err;

	/* clear the context structure first */
	memset(&rx_ctx, 0, sizeof(rx_ctx));
	queue_idx = i40e_iface->base_queue + ring_idx;

	/*
	 * See 8.3.3.2.2 - Receive Queue Context in FPM
	 */
	rx_ctx.dbuff = iface->buf_size >> I40E_RXQ_CTX_DBUFF_SHIFT;
	rx_ctx.base = (ring->addr_dma / 128);
	rx_ctx.qlen = iface->num_rx_desc;
	/* use 32 byte descriptors */
	rx_ctx.dsize = 1;
	/* descriptor type is always zero */
	rx_ctx.dtype = 0;
	rx_ctx.hsplit_0 = 0;
	rx_ctx.rxmax = iface->mtu_frame;
	rx_ctx.lrxqthresh = 2;
	rx_ctx.crcstrip = 1;
	rx_ctx.l2tsel = 1;
	/* this controls whether VLAN is stripped from inner headers */
	rx_ctx.showiv = 0;
	/* set the prefena field to 1 because the manual says to */
	rx_ctx.prefena = 1;

	/* clear the context in the HMC */
	err = i40e_clear_lan_rx_queue_context(hw, pf_q);
	if(err < 0)
		goto err_clear_lan_rx_queue_ctx;

	/* set the context in the HMC */
	err = i40e_set_lan_rx_queue_context(hw, pf_q, &rx_ctx);
	if(err < 0)
		goto err_set_lan_rx_queue_ctx;

	/* cache tail for quicker writes, and clear the reg before use */
	ring->tail = dev->bar + I40E_QRX_TAIL(queue_idx);
	writel(0, ring->tail);

	return 0;
}
