void i40e_vlan_stripping_disable(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_aqc_vsi_properties_data data;
	i40e_status ret;

	data.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_VLAN_VALID);
	data.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_ALL |
				    I40E_AQ_VSI_PVLAN_EMOD_NOTHING;

	err = i40e_aqc_req_update_vsi(dev, iface, &data);
	if(err < 0)
		goto err_req_update;

	return 0;

err_req_update:
	return -1;
}

int i40e_vsi_rss_config(struct ufp_dev *dev, struct ufp_iface *iface)
{
	u8 seed[I40E_HKEY_ARRAY_SIZE];
	u8 lut[512];

	/* seed configuration */
	netdev_rss_key_fill((void *)seed, I40E_HKEY_ARRAY_SIZE);

	err = i40e_aq_cmd_xmit_setrsskey(dev, iface,
		seed, sizeof(seed));
	if(err < 0)
		goto err_set_rss_key;

	/* lut configuration */
	i40e_fill_rss_lut(pf, lut, sizeof(lut), dev->num_qp);

	err = i40e_aq_cmd_xmit_setrsslut(dev, iface,
		lut, sizeof(lut));
	if(err < 0)
		goto err_set_rss_lut;

	return 0;

err_set_rss_lut:
err_set_rss_key:
	return -1;
}

int i40e_vsi_promisc_mode(struct ufp_dev *dev, struct ufp_iface *iface)
{
	uint16_t promisc_flags;
	int err;

	promisc_flags = I40E_AQC_SET_VSI_PROMISC_MULTICAST |
		I40E_AQC_SET_VSI_PROMISC_BROADCAST;

	if(iface->promisc){
		promisc_flags |= I40E_AQC_SET_VSI_PROMISC_UNICAST;
	}

	err = i40e_aqc_req_promisc_mode(dev, iface, promisc_flags);
	if(err < 0)
		goto err_set_promisc_mode;

	return 0;

err_set_promisc_mode:
	return -1;
}

int i40e_vsi_configure_tx(struct ufp_dev *dev,
	struct ufp_iface *iface)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	struct ufp_ring *ring;
	struct i40e_hmc_ctx_tx ctx;
	uint16_t qp_idx;
	uint32_t qtx_ctl;
	int i, err;

	for (i = 0; i < iface->num_qps; i++){
		qp_idx = i40e_iface->base_qp + i;
		ring = &iface->tx_ring[i];

		/* clear the context structure first */
		memset(&ctx, 0, sizeof(struct i40e_hmc_ctx_tx));

		/*
		 * See 8.4.3.4.2 - Transmit Queue Context in FPM
		 */
		ctx.new_context = 1;
		ctx.base = (ring->addr_dma / 128);
		ctx.qlen = iface->num_tx_desc;

		/* XXX: Does it work? Is ctx.cpuid set by hardware correctly
		 * when socket id is not equeal 0?
		 */
		ctx.tphrdesc_ena = 1;
		ctx.tphwdesc_ena = 1;
		ctx.tphrpacket_ena = 1;

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
	}

	return 0;

err_set_ctx:
	return -1;
}

int i40e_vsi_configure_rx(struct ufp_dev *dev,
	struct ufp_iface *iface)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	struct ufp_ring *ring;
	struct i40e_hmc_ctx_rx ctx;
	uint16_t qp_idx;
	uint32_t qtx_ctl;
	int i, err;

	for (i = 0; i < iface->num_qps; i++){
		qp_idx = i40e_iface->base_qp + i;
		ring = &iface->rx_ring[i];

		/* clear the context structure first */
		memset(&ctx, 0, sizeof(struct i40e_hmc_ctx_rx));

		/*
		 * See 8.3.3.2.2 - Receive Queue Context in FPM
		 */
		ctx.dbuff = I40E_RXBUFFER_2048 >> I40E_RXQ_CTX_DBUFF_SHIFT;
		ctx.base = (ring->addr_dma / 128);
		ctx.qlen = iface->num_rx_desc;
		/* use 16 byte descriptors */
		ctx.dsize = 0;
		/* descriptor type is always zero */
		ctx.dtype = 0;
		ctx.hsplit_0 = 0;
		ctx.rxmax = iface->mtu_frame;

		/* XXX: Does it work? Is ctx.cpuid set by hardware correctly
		 * when socket id is not equeal 0?
		 */
		ctx.tphrdesc_ena = 1;
		ctx.tphwdesc_ena = 1;
		ctx.tphdata_ena = 1;
		ctx.tphhead_ena = 0;

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
	}

	return 0;

err_set_ctx:
	return -1;
}

void i40e_vsi_start_irq(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	uint16_t qp_idx, vector;
	uint32_t val;
	int i;

	vector = i40e_iface->base_qp * 2 + dev->num_misc_irqs;
	for (i = 0; i < iface->num_qps; i++, vector++){
		qp_idx = i40e_iface->base_qp + i;

		wr32(hw, I40E_PFINT_ITRN(I40E_RX_ITR, vector),
		     ITR_TO_REG(I40E_ITR_20K));

		wr32(hw, I40E_PFINT_RATEN(vector),
		     INTRL_USEC_TO_REG(vsi->int_rate_limit));

		/* Linked list for the queuepairs assigned to this vector */
		val = qp_idx << I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT |
			I40E_QUEUE_TYPE_TX << I40E_PFINT_LNKLSTN_FIRSTQ_TYPE_SHIFT;
		wr32(hw, I40E_PFINT_LNKLSTN(vector), val);

		val = I40E_QINT_RQCTL_CAUSE_ENA_MASK |
			(I40E_RX_ITR << I40E_QINT_RQCTL_ITR_INDX_SHIFT) |
			(vector << I40E_QINT_RQCTL_MSIX_INDX_SHIFT) |
			(I40E_QUEUE_END_OF_LIST << I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT);
		wr32(hw, I40E_QINT_RQCTL(qp_idx), val);

		/* originally in i40e_irq_dynamic_enable() */
		val = I40E_PFINT_DYN_CTLN_INTENA_MASK |
			I40E_PFINT_DYN_CTLN_CLEARPBA_MASK |
			(I40E_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT);
		wr32(hw, I40E_PFINT_DYN_CTLN(vector), val);
	}

	for (i = 0; i < iface->num_qps; i++, vector++){
		qp_idx = i40e_iface->base_qp + i;

		wr32(hw, I40E_PFINT_ITRN(I40E_TX_ITR, vector),
		     ITR_TO_REG(I40E_ITR_20K));

		wr32(hw, I40E_PFINT_RATEN(vector),
		     INTRL_USEC_TO_REG(vsi->int_rate_limit));

		/* Linked list for the queuepairs assigned to this vector */
		val = qp_idx << I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT |
			I40E_QUEUE_TYPE_RX << I40E_PFINT_LNKLSTN_FIRSTQ_TYPE_SHIFT;
		wr32(hw, I40E_PFINT_LNKLSTN(vector), val);

		val = I40E_QINT_TQCTL_CAUSE_ENA_MASK |
			(I40E_TX_ITR << I40E_QINT_TQCTL_ITR_INDX_SHIFT) |
			(vector << I40E_QINT_TQCTL_MSIX_INDX_SHIFT) |
			(I40E_QUEUE_END_OF_LIST << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT);
		wr32(hw, I40E_QINT_TQCTL(qp_idx), val);

		/* originally in i40e_irq_dynamic_enable() */
		val = I40E_PFINT_DYN_CTLN_INTENA_MASK |
			I40E_PFINT_DYN_CTLN_CLEARPBA_MASK |
			(I40E_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT);
		wr32(hw, I40E_PFINT_DYN_CTLN(vector), val);
	}

	i40e_flush(hw);
	return;
}

static void i40e_vsi_stop_irq(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	uint16_t qp_idx, vector;
	uint32_t val;
	int i;

	vector = i40e_iface->base_qp * 2 + dev->num_misc_irqs;
	for (i = 0; i < iface->num_qps; i++, vector++){
		qp_idx = i40e_iface->base_qp + i;

		wr32(hw, I40E_PFINT_DYN_CTLN(vector), 0);

		val = rd32(hw, I40E_PFINT_LNKLSTN(vector));
		val |= I40E_QUEUE_END_OF_LIST
			<< I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT;
		wr32(hw, I40E_PFINT_LNKLSTN(vector), val);

		val = rd32(hw, I40E_QINT_RQCTL(qp_idx));

		val &= ~(I40E_QINT_RQCTL_MSIX_INDX_MASK  |
			 I40E_QINT_RQCTL_MSIX0_INDX_MASK |
			 I40E_QINT_RQCTL_CAUSE_ENA_MASK  |
			 I40E_QINT_RQCTL_INTEVENT_MASK);

		val |= (I40E_QINT_RQCTL_ITR_INDX_MASK |
			 I40E_QINT_RQCTL_NEXTQ_INDX_MASK);

		wr32(hw, I40E_QINT_RQCTL(qp_idx), val);
	}

	for(i = 0; i < iface->num_qps; i++, vector++){
		qp_idx = i40e_iface->base_qp + i;

		wr32(hw, I40E_PFINT_DYN_CTLN(vector), 0);

		val = rd32(hw, I40E_PFINT_LNKLSTN(vector));
		val |= I40E_QUEUE_END_OF_LIST
			<< I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT;
		wr32(hw, I40E_PFINT_LNKLSTN(vector), val);

		val = rd32(hw, I40E_QINT_TQCTL(qp_idx));

		val &= ~(I40E_QINT_TQCTL_MSIX_INDX_MASK |
			 I40E_QINT_TQCTL_MSIX0_INDX_MASK |
			 I40E_QINT_TQCTL_CAUSE_ENA_MASK |
			 I40E_QINT_TQCTL_INTEVENT_MASK);

		val |= (I40E_QINT_TQCTL_ITR_INDX_MASK |
			 I40E_QINT_TQCTL_NEXTQ_INDX_MASK);

		wr32(hw, I40E_QINT_TQCTL(qp_idx), val);
	}

	i40e_flush(hw);
	return;
}

static int i40e_vsi_start_rx(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	uint32_t rx_reg;
	uint16_t qp_idx;
	int i;

	for (i = 0; i < iface->num_qps; i++){
		qp_idx = i40e_iface->base_qp + i;

		for (j = 0; j < 50; j++) {
			rx_reg = rd32(hw, I40E_QRX_ENA(pf_q));
			if (((rx_reg >> I40E_QRX_ENA_QENA_REQ_SHIFT) & 1) ==
			    ((rx_reg >> I40E_QRX_ENA_QENA_STAT_SHIFT) & 1))
				break;
			usleep_range(1000, 2000);
		}

		/* Skip if the queue is already in the requested state */
		if(rx_reg & I40E_QRX_ENA_QENA_STAT_MASK)
			continue;

		/* turn on/off the queue */
		rx_reg |= I40E_QRX_ENA_QENA_REQ_MASK;
		wr32(hw, I40E_QRX_ENA(pf_q), rx_reg);

		/* wait for the change to finish */
		for (retry = 0; retry < I40E_QUEUE_WAIT_RETRY_LIMIT; retry++) {
			rx_reg = rd32(&pf->hw, I40E_QRX_ENA(pf_q));
			if (rx_reg & I40E_QRX_ENA_QENA_STAT_MASK)
				break;

			usleep_range(10, 20);
		}
		if (retry >= I40E_QUEUE_WAIT_RETRY_LIMIT)
			goto err_vsi_start;
	}
	return 0;

err_vsi_start:
	return -1;
}

static int i40e_vsi_stop_rx(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	uint32_t rx_reg;
	uint16_t qp_idx;
	int i;

	for (i = 0; i < iface->num_qps; i++){
		qp_idx = i40e_iface->base_qp + i;

		for (j = 0; j < 50; j++) {
			rx_reg = rd32(hw, I40E_QRX_ENA(pf_q));
			if (((rx_reg >> I40E_QRX_ENA_QENA_REQ_SHIFT) & 1) ==
			    ((rx_reg >> I40E_QRX_ENA_QENA_STAT_SHIFT) & 1))
				break;
			usleep_range(1000, 2000);
		}

		/* Skip if the queue is already in the requested state */
		if(!(rx_reg & I40E_QRX_ENA_QENA_STAT_MASK))
			continue;

		/* turn on/off the queue */
		rx_reg &= ~I40E_QRX_ENA_QENA_REQ_MASK;
		wr32(hw, I40E_QRX_ENA(pf_q), rx_reg);

		/* wait for the change to finish */
		for (retry = 0; retry < I40E_QUEUE_WAIT_RETRY_LIMIT; retry++) {
			rx_reg = rd32(&pf->hw, I40E_QRX_ENA(pf_q));
			if(!(rx_reg & I40E_QRX_ENA_QENA_STAT_MASK))
				break;

			usleep_range(10, 20);
		}
		if (retry >= I40E_QUEUE_WAIT_RETRY_LIMIT)
			goto err_vsi_stop;
	}
	return 0;

err_vsi_stop:
	return -1;
}

static int i40e_vsi_start_tx(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	uint32_t tx_reg;
	uint16_t qp_idx;
	int i, retry;

	for (i = 0; i < iface->num_qps; i++){
		qp_idx = i40e_iface->base_qp + i;

		/* warn the TX unit of coming changes */
		i40e_pre_tx_queue_cfg(&pf->hw, pf_q, enable);
		if (!enable)
			usleep_range(10, 20);

		for (j = 0; j < 50; j++) {
			tx_reg = rd32(hw, I40E_QTX_ENA(qp_idx));
			if (((tx_reg >> I40E_QTX_ENA_QENA_REQ_SHIFT) & 1) ==
			    ((tx_reg >> I40E_QTX_ENA_QENA_STAT_SHIFT) & 1))
				break;
			usleep_range(1000, 2000);
		}
		/* Skip if the queue is already in the requested state */
		if (tx_reg & I40E_QTX_ENA_QENA_STAT_MASK)
			continue;

		/* turn on/off the queue */
		wr32(hw, I40E_QTX_HEAD(qp_idx), 0);
		tx_reg |= I40E_QTX_ENA_QENA_REQ_MASK;
		wr32(hw, I40E_QTX_ENA(qp_idx), tx_reg);

		/* wait for the change to finish */
		for (retry = 0; retry < I40E_QUEUE_WAIT_RETRY_LIMIT; retry++) {
			tx_reg = rd32(&pf->hw, I40E_QTX_ENA(qp_idx));
			if (tx_reg & I40E_QTX_ENA_QENA_STAT_MASK)
				break;

			usleep_range(10, 20);
		}
		if (retry >= I40E_QUEUE_WAIT_RETRY_LIMIT)
			goto err_vsi_start;
	}

	return 0;

err_vsi_start:
	return -1;
}

static int i40e_vsi_stop_tx(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_iface *i40e_iface = iface->drv_data;
	uint32_t tx_reg;
	uint16_t qp_idx;
	int i, retry;

	for (i = 0; i < iface->num_qps; i++){
		qp_idx = i40e_iface->base_qp + i;

		/* warn the TX unit of coming changes */
		i40e_pre_tx_queue_cfg(&pf->hw, pf_q, enable);
		if (!enable)
			usleep_range(10, 20);

		for (j = 0; j < 50; j++) {
			tx_reg = rd32(hw, I40E_QTX_ENA(qp_idx));
			if (((tx_reg >> I40E_QTX_ENA_QENA_REQ_SHIFT) & 1) ==
			    ((tx_reg >> I40E_QTX_ENA_QENA_STAT_SHIFT) & 1))
				break;
			usleep_range(1000, 2000);
		}
		/* Skip if the queue is already in the requested state */
		if (!(tx_reg & I40E_QTX_ENA_QENA_STAT_MASK))
			continue;

		/* turn on/off the queue */
		tx_reg &= ~I40E_QTX_ENA_QENA_REQ_MASK;
		wr32(hw, I40E_QTX_ENA(qp_idx), tx_reg);

		/* wait for the change to finish */
		for (retry = 0; retry < I40E_QUEUE_WAIT_RETRY_LIMIT; retry++) {
			tx_reg = rd32(&pf->hw, I40E_QTX_ENA(qp_idx));
			if (!(tx_reg & I40E_QTX_ENA_QENA_STAT_MASK))
				break;

			usleep_range(10, 20);
		}
		if (retry >= I40E_QUEUE_WAIT_RETRY_LIMIT)
			goto err_vsi_stop;
	}

	return 0;

err_vsi_stop:
	return -1;
}

int i40e_rx_desc_fetch(struct ufp_ring *rx_ring, uint16_t index,
	struct ufp_packet *packet)
{
	union ufp_i40e_rx_desc *rx_desc;
	uint64_t qword1;

	rx_desc = I40E_RX_DESC(rx_ring, index);
	qword1 = le64_to_cpu(rx_desc->wb.qword1.status_error_len);

	if (!(qword1 & BIT(I40E_RX_DESC_STATUS_DD_SHIFT)))
		goto not_received;

	/*
	 * This memory barrier is needed to keep us from reading
	 * any other fields out of the rx_desc until we know the
	 * RXD_STAT_DD bit is set
	 */
	rmb();

	packet->slot_size = (qword1 & I40E_RXD_QW1_LENGTH_PBUF_MASK) >>
		I40E_RXD_QW1_LENGTH_PBUF_SHIFT;
	packet->flag = 0;

	if(likely(qword1 & BIT(I40E_RX_DESC_STATUS_EOF_SHIFT)))
		packet->flag |= UFP_PACKET_EOP;

	if(unlikely(qword1 & I40E_RXD_QW1_ERROR_MASK))
		packet->flag |= UFP_PACKET_ERROR;

	return 0;

not_received:
	return -1;
}

int i40e_tx_desc_fetch(struct ufp_ring *tx_ring, uint16_t index)
{
	union ufp_i40e_tx_desc *tx_desc;
	uint64_t qword1;

	tx_desc = I40E_TX_DESC(tx_ring, index);
	qword1 = le64_to_cpu(tx_desc->cmd_type_offset_bsz);

	if ((qword1 & I40E_TXD_QW1_DTYPE_MASK) !=
		I40E_TX_DESC_DTYPE_DESC_DONE)
		goto not_sent;

	return 0;

not_sent:
	return -1;
}

void i40e_rx_desc_fill(struct ufp_ring *rx_ring, uint16_t index,
	uint64_t addr_dma)
{
	union ufp_i40e_rx_desc *rx_desc;

	rx_desc = I40E_RX_DESC(rx_ring, index);

	rx_desc->read.pkt_addr = htole64(addr_dma);
	rx_desc->read.hdr_addr = 0;

	/* clear the status bits for the next_to_use descriptor */
	rx_desc->wb.qword1.status_error_len = 0;

	return;
}

void i40e_tx_desc_fill(struct ufp_ring *tx_ring, uint16_t index,
	uint64_t addr_dma, struct ufp_packet *packet)
{
	union ufp_i40e_tx_desc *tx_desc;
	uint32_t tx_cmd = 0;
	uint32_t tx_offset = 0;
	uint32_t tx_tag = 0;

	tx_desc = IXGBE_TX_DESC(tx_ring, index);

	tx_cmd |= I40E_TX_DESC_CMD_ICRC | I40E_TX_DESC_CMD_RS;
	if(likely(packet->flag & UFP_PACKET_EOP)){
		tx_cmd |= I40E_TX_DESC_CMD_EOP;
	}

	/* XXX: The size limit for a transmit buffer in a descriptor is (16K - 1).
	 * In order to align with the read requests we will align the value to
	 * the nearest 4K which represents our maximum read request size.
	 */
	tx_desc->buffer_addr = htole64(addr_dma);
	tx_desc->cmd_type_offset_bsz = htole64(I40E_TX_DESC_DTYPE_DATA |
		((u64)tx_cmd << I40E_TXD_QW1_CMD_SHIFT) |
		((u64)tx_offset << I40E_TXD_QW1_OFFSET_SHIFT) |
		((u64)packet->slot_size << I40E_TXD_QW1_TX_BUF_SZ_SHIFT) |
		((u64)tx_tag << I40E_TXD_QW1_L2TAG1_SHIFT));

	return;
}
