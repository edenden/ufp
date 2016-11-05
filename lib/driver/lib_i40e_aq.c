int ufp_i40e_aq_init(struct ufp_dev *dev)
{       
	int err;

	/* verify input for valid configuration */
	if ((hw->aq.num_arq_entries == 0) ||
	    (hw->aq.num_asq_entries == 0) ||
	    (hw->aq.arq_buf_size == 0) ||
	    (hw->aq.asq_buf_size == 0)) {
		err = I40E_ERR_CONFIG;
		goto init_adminq_exit;
	}
	
	/* allocate the ASQ */
	err = i40e_aq_asq_init(dev);
	if(err < 0)
		goto err_init_asq;
	
	/* allocate the ARQ */
	err = i40e_aq_arq_init(dev);
	if(err < 0)
		goto err_init_arq;

	dev->num_misc_irqs += 2;
	
	return 0;

err_init_arq:
	i40e_aq_asq_shutdown(dev);
err_init_asq:
	return -1;
}

int i40e_aq_asq_init(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_ring *ring;
	int err;

	ring = malloc(sizeof(struct i40e_aq_ring));
	if(!ring)
		goto err_alloc_ring;

	ring->tail = I40E_PF_ATQT;
	ring->head = I40E_PF_ATQH;
	ring->len  = I40E_PF_ATQLEN;
	ring->bal  = I40E_PF_ATQBAL;
	ring->bah  = I40E_PF_ATQBAH;

	err = i40e_aq_ring_alloc(dev, ring);
	if(err < 0)
		goto err_alloc_ring;

	/* initialize base registers */
	err = i40e_aq_ring_configure(dev, ring);
	if (err < 0)
		goto err_regs_config;

	i40e_dev->aq_tx_ring = ring;
	return 0;

err_regs_config:
	i40e_aq_ring_release(dev, ring);
err_init_ring:
	free(ring);
err_alloc_ring:
	return -1;
}

int i40e_aq_arq_init(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_i40e_ring *ring;
	struct ufp_irq_handle *irqh;
	int err;

	ring = malloc(sizeof(struct i40e_aq_ring));
	if(!ring)
		goto err_alloc_ring;

	ring->tail = I40E_PF_ARQT;
	ring->head = I40E_PF_ARQH;
	ring->len  = I40E_PF_ARQLEN;
	ring->bal  = I40E_PF_ARQBAL;
	ring->bah  = I40E_PF_ARQBAH;

	err = i40e_aq_ring_alloc(dev, ring);
	if(err < 0)
		goto err_alloc_ring;

	/* initialize base registers */
	err = i40e_aq_ring_configure(dev, ring);
	if (err < 0)
		goto err_regs_config;

	i40e_dev->aq_rx_ring = ring;
	i40e_fill_arq();
	return 0;

err_regs_config:
	i40e_aq_ring_release(dev, ring);
err_init_ring:
	free(ring);
err_alloc_ring:
	return -1;
}

int i40e_aq_ring_alloc(struct ufp_dev *dev, struct ufp_i40e_aq_ring *ring)
{
	uint64_t size;
	int i;

	/* verify input for valid configuration */
	if ((hw->aq.num_asq_entries == 0) ||
	    (hw->aq.asq_buf_size == 0)) {
		goto err_config;
	}

	hw->aq.asq.next_to_use = 0;
	hw->aq.asq.next_to_clean = 0;

	/* allocate the ring memory */
	size = ring->num_entries * sizeof(struct i40e_aq_desc);
	size = ALIGN(size, I40E_ADMINQ_DESC_ALIGNMENT);
	if(size > sysconf(_SC_PAGESIZE))
		goto err_desc_size;

	ring->desc = ufp_i40e_page_alloc(dev);
	if(!ring->desc)
		goto err_desc_alloc;

	/* allocate buffers in the rings */
	ring->bufs = malloc(ring->num_entries * sizeof(struct ufp_i40e_page *));
	if(!ring->bufs)
		goto err_buf_alloc;

	return 0;

err_buf_alloc:
	ufp_i40e_page_release(ring->desc);
err_desc_alloc:
err_config:
err_already:
	return -1;
}

int i40e_aq_ring_configure(struct ufp_dev *dev, struct ufp_i40e_aq_ring *ring)
{
	/* Clear Head and Tail */
	ufp_write_reg(hw, ring->head, 0);
	ufp_write_reg(hw, ring->tail, 0);

	/* set starting point */
	ufp_write_reg(hw, ring->len, (ring->num_entries | I40E_MASK(0x1, 31));
	ufp_write_reg(hw, ring->bal, lower_32_bits(ring->desc->addr_dma));
	ufp_write_reg(hw, ring->bah, upper_32_bits(ring->desc->addr_dma));

	/* Check one register to verify that config was applied */
	reg = ufp_read_reg(hw, ring->bal);
	if (reg != lower_32_bits(ring->desc->addr_dma))
		goto err_init_ring;

	return 0;

err_init_ring:
	return -1;
}

void ufp_i40e_aq_destroy(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;

	i40e_aq_queue_shutdown(dev, true);
	i40e_aq_asq_shutdown(dev);
	i40e_aq_arq_shutdown(dev);

	return;
}

void i40e_aq_asq_shutdown(ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;

	/* Stop firmware AdminQ processing */
	ufp_write_reg(hw, ring->head, 0);
	ufp_write_reg(hw, ring->tail, 0);
	ufp_write_reg(hw, ring->len, 0);
	ufp_write_reg(hw, ring->bal, 0);
	ufp_write_reg(hw, ring->bah, 0);

	/* free ring buffers */
	i40e_aq_ring_release(dev, i40e_dev->aq_tx_ring);
	free(i40e_dev->aq_tx_ring);

	return;
}

void i40e_aq_arq_shutdown(ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;

	/* Stop firmware AdminQ processing */
	ufp_write_reg(hw, ring->head, 0);
	ufp_write_reg(hw, ring->tail, 0);
	ufp_write_reg(hw, ring->len, 0);
	ufp_write_reg(hw, ring->bal, 0);
	ufp_write_reg(hw, ring->bah, 0);

	/* free ring buffers */
	i40e_aq_ring_release(dev, ring);
	free(i40e_dev->aq_rx_ring);

	return;
}

void i40e_aq_ring_release(struct ufp_dev *dev, struct ufp_i40e_aq_ring *ring)
{
	int i;

	for(i = 0; i < ring->num_entries; i++){
		if(ring->bufs[i])
			ufp_i40e_page_release(ring->bufs[i]);
	}
	free(ring->bufs);
	ufp_i40e_page_release(ring->desc);

	return;
}

void i40e_aq_asq_clean(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring;
	u16 ntc = asq->next_to_clean;
	struct i40e_aq_desc *desc;
	struct ufp_i40e_page *buf;

	ring = i40e_dev->aq_tx_ring;

	desc = I40E_ADMINQ_DESC(*asq, ntc);
	while (ufp_read_reg(hw, ring->head) != ntc) {
		buf = ring->bufs[ntc];
		i40e_aq_asq_process(dev, desc, buf);

		memset(desc, 0, sizeof(*desc));
		ntc++;
		if (ntc == asq->count)
			ntc = 0;
		desc = I40E_ADMINQ_DESC(*asq, ntc);
	}

	asq->next_to_clean = ntc;
	return;
}

int i40e_aq_asq_xmit(struct ufp_dev *dev, uint16_t opcode, uint16_t flags,
	void *cmd, uint16_t cmd_size, struct ufp_i40e_page *buf, uint16_t buf_size)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data; 
	struct i40e_aq_ring *ring;
	struct i40e_aq_desc *desc;

	ring = i40e_dev->aq_tx_ring;

	if(!I40E_DESC_UNUSED(asq)){
		/* queue is full */
		goto err_send;
	}

	desc = I40E_ADMINQ_DESC(hw->aq.asq, hw->aq.asq.next_to_use);
	desc->flags = CPU_TO_LE16(flags);
	desc->opcode = CPU_TO_LE16(opcode);
	memcpy(&desc->params, cmd, cmd_size);

	/* if buf is not NULL assume indirect command */
	if (buf) {
		ring->bufs[ntu] = buf;
		desc->datalen = CPU_TO_LE16(buf_size);

		/* Update the address values in the desc with the pa value
		 * for respective buffer
		 */
		desc->params.external.addr_high =
				CPU_TO_LE32(upper_32_bits(buf->addr_dma));
		desc->params.external.addr_low =
				CPU_TO_LE32(lower_32_bits(buf->addr_dma));
	}

	/* bump the tail */
	(hw->aq.asq.next_to_use)++;
	if (hw->aq.asq.next_to_use == hw->aq.num_entries)
		hw->aq.asq.next_to_use = 0;

	ufp_write_reg(hw, ring->tail, hw->aq.asq.next_to_use);

	return 0;

err_send:
	return -1;
}

int i40e_aq_arq_fill(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring;
	unsigned int desc_filled = 0;
	struct i40e_aq_desc *desc;
	struct ufp_i40e_page *buf;

	ring = i40e_dev->aq_rx_ring;

	/* allocate the mapped buffers */
	for (i = 0; i < ring->num_entries; i++, desc_filled++) {
		buf = ufp_i40e_page_alloc(dev);
		if(!buf)
			goto err_page_alloc;

		ring->bufs[i] = buf;

		/* now configure the descriptors for use */
		desc = I40E_ADMINQ_DESC(hw->aq.arq, i);

		desc->flags = CPU_TO_LE16(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_LB);
		desc->opcode = 0;
		/* This is in accordance with Admin queue design, there is no
		 * register for buffer size configuration
		 */
		desc->datalen = CPU_TO_LE16((u16)bi->size);
		desc->retval = 0;
		desc->cookie_high = 0;
		desc->cookie_low = 0;
		desc->params.external.addr_high =
			CPU_TO_LE32(upper_32_bits(buf->addr_dma));
		desc->params.external.addr_low =
			CPU_TO_LE32(lower_32_bits(buf->addr_dma));
		desc->params.external.param0 = 0;
		desc->params.external.param1 = 0;
	}

	/* Update tail in the HW to post pre-allocated buffers */
	ufp_write_reg(hw, ring->tail, ring->num_entries - 1);

	return 0;

err_page_alloc:
	for(i = 0; i < desc_filled; i++){
		ufp_i40e_page_release(ring->bufs[i]);
	}
	return -1;
}

int i40e_aq_arq_clean(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring;
	u16 ntc = ring->next_to_clean;
	struct i40e_aq_desc *desc;
	struct i40e_aq_page *buf;
	u16 ntu;
	u16 ntc;

	ring = i40e_dev->aq_rx_ring;

	/* set next_to_use to head */
	ntc = ring->next_to_clean;
	ntu = (ufp_read_reg(hw, ring->head) & I40E_PF_ARQH_ARQH_MASK);

	while(ntc != ntu){
		/* now clean the next descriptor */
		desc = ((i40e_aq_desc *)ring->desc->addr_virt)[ntc];
		buf = ring->bufs[ntc];
		i40e_arq_process(dev, desc, buf->addr_virt);

		/* Restore the original datalen and buffer address in the desc */
		memset((void *)desc, 0, sizeof(struct i40e_aq_desc));
		desc->flags = CPU_TO_LE16(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_LB);
		desc->datalen = CPU_TO_LE16((u16)bi->size);
		desc->params.external.addr_high = CPU_TO_LE32(upper_32_bits(buf->addr_dma));
		desc->params.external.addr_low = CPU_TO_LE32(lower_32_bits(buf->addr_dma));

		/* ntc is updated to tail + 1 */
		ntc++;
		if (ntc == hw->aq.num_arq_entries)
			ntc = 0;
	}

	/* set tail = the last cleaned desc index. */
	ufp_write_reg(hw, ring->tail, ntc);

	hw->aq.arq.next_to_clean = ntc;
	hw->aq.arq.next_to_use = ntu;

	return 0;

err_no_work:
	return -1;
}

void i40e_aq_asq_process(struct ufp_dev *dev, struct i40e_aq_desc *desc,
	void *buf)
{
	uint16_t retval;
	uint16_t opcode;
	int err;

	retval = LE16_TO_CPU(desc->retval);
	opcode = LE16_TO_CPU(desc->opcode);

	if(retval != 0)
		goto err_retval;

	switch(opcode){
	case i40e_aqc_opc_mac_addr:
		err = i40e_aq_cmd_clean_macaddr(dev,
			&desc->params, buf->addr_virt);
		if(err < 0)
			goto err_clean;
		break;
	case i40e_aqc_opc_clear_pxe:
		err = i40e_aq_cmd_clean_clearpxe(dev,
			&desc->params);
		if(err < 0)
			goto err_clean;
		break;
	case i40e_aqc_opc_add_vsi:
		err = i40e_aq_cmd_clean_addvsi(dev,
			&desc->params);
		if(err < 0)
			goto err_clean;
		break;
	default:
		break;
	}

err_clean:
err_retval:
	ufp_i40e_page_release(buf);
	return;
}

void i40e_aq_arq_process(struct ufp_dev *dev, struct i40e_aq_desc *desc,
	void *buf)
{
	uint16_t len;
	uint16_t flags;
	uint16_t opcode;

	len = LE16_TO_CPU(desc->datalen);
	flags = LE16_TO_CPU(desc->flags);
	opcode = LE16_TO_CPU(desc->opcode);

	if (flags & I40E_AQ_FLAG_ERR) {
		goto err_flag;
	}

	switch(opcode){
	case :
	default:
	}

	ufp_i40e_page_release(buf);

err_flag:
	return;
}
