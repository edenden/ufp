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
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	struct ufp_ring *ring = &iface->tx_ring[ring_idx];
	struct i40e_hmc_ctx_tx ctx;
	uint16_t qp_idx;
	uint32_t qtx_ctl;
	int err;

	/* clear the context structure first */
	memset(&ctx, 0, sizeof(struct i40e_hmc_ctx_tx));
	qp_idx = i40e_iface->base_qp + ring_idx;

	/*
	 * See 8.4.3.4.2 - Transmit Queue Context in FPM
	 */
	ctx.new_context = 1;
	ctx.base = (ring->addr_dma / 128);
	ctx.qlen = iface->num_tx_desc;

	/*
	 * This flag selects between Head WB and transmit descriptor WB:
	 * 0b - Descriptor Write Back
	 * 1b - Head Write Back
	 */
	ctx.head_wb_ena = 0;

	/*
	 * See 1.1.4 - Transmit scheduler
	 * The XL710 provides management interfaces that allow
	 * each LAN transmit queue to be placed into a queue set.
	 * A queue set is a list of transmit queues that belong to the same TC
	 * and are treated equally by the XL710 transmit scheduler.
	 */
	ctx.rdylist = le16_to_cpu(i40e_iface->qs_handle[0]);
	ctx.rdylist_act = 0;

	/* set the context in the HMC */
	err = i40e_hmc_set_ctx_tx(dev, &ctx, qp_idx);
	if(err < 0)
		goto err_set_ctx;

	/* Now associate this queue with this PCI function */
	qtx_ctl = I40E_QTX_CTL_PF_QUEUE;
	qtx_ctl |= ((i40e_dev->pf_id << I40E_QTX_CTL_PF_INDX_SHIFT) &
		    I40E_QTX_CTL_PF_INDX_MASK);
	wr32(hw, I40E_QTX_CTL(qp_idx), qtx_ctl);
	i40e_flush(dev);

	/* cache tail off for easier writes later */
	ring->tail = dev->bar + I40E_QTX_TAIL(qp_idx);

	return 0;

err_set_ctx:
	return -1;
}

static int i40e_configure_rx_ring(struct ufp_dev *dev,
	struct ufp_iface *iface, int ring_idx)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	struct ufp_ring *ring = &iface->rx_ring[ring_idx];
	struct i40e_hmc_ctx_rx ctx;
	uint16_t qp_idx;
	uint32_t qtx_ctl;
	int err;

	/* clear the context structure first */
	memset(&ctx, 0, sizeof(struct i40e_hmc_ctx_rx));
	qp_idx = i40e_iface->base_qp + ring_idx;

	/*
	 * See 8.3.3.2.2 - Receive Queue Context in FPM
	 */
	ctx.dbuff = iface->buf_size >> I40E_RXQ_CTX_DBUFF_SHIFT;
	ctx.base = (ring->addr_dma / 128);
	ctx.qlen = iface->num_rx_desc;
	/* use 32 byte descriptors */
	ctx.dsize = 1;
	/* descriptor type is always zero */
	ctx.dtype = 0;
	ctx.hsplit_0 = 0;
	ctx.rxmax = iface->mtu_frame;
	ctx.lrxqthresh = 2;
	ctx.crcstrip = 1;
	ctx.l2tsel = 1;
	/* this controls whether VLAN is stripped from inner headers */
	ctx.showiv = 0;
	/* set the prefena field to 1 because the manual says to */
	ctx.prefena = 1;

	/* set the context in the HMC */
	err = i40e_hmc_set_ctx_rx(dev, &ctx, qp_idx);
	if(err < 0)
		goto err_set_ctx;

	/* cache tail for quicker writes, and clear the reg before use */
	ring->tail = dev->bar + I40E_QRX_TAIL(qp_idx);
	writel(0, ring->tail);

	return 0;

err_set_ctx:
	return -1;
}

