// Usage Sample
static i40e_status i40e_aq_mac_address_read(struct i40e_hw *hw,
				   u16 *flags,
				   struct i40e_aqc_mac_address_read_data *addrs,
				   struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_mac_address_read *cmd_data =
		(struct i40e_aqc_mac_address_read *)&desc.params.raw;
	i40e_status status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_mac_address_read);
	desc.flags |= CPU_TO_LE16(I40E_AQ_FLAG_BUF);

	status = i40e_asq_send_command(hw, &desc, addrs,
				       sizeof(*addrs), cmd_details);
	*flags = LE16_TO_CPU(cmd_data->command_flags);

	return status;
}

int i40e_aq_init(struct i40e_hw *hw)
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
	err = i40e_aq_asq_init(hw, asq);
	if(err < 0)
		goto err_init_asq;
	
	/* allocate the ARQ */
	err = i40e_aq_arq_init(hw, arq);
	if(err < 0)
		goto err_init_arq;
	
	return 0;

err_init_arq:
	i40e_aq_ring_shutdown(asq);
err_init_asq:
	return -1;
}

int i40e_aq_asq_init(struct ufp_handle *ih, struct ufp_i40e_aq_ring *ring)
{
	int err;

	ring->tail = I40E_PF_ATQT;
	ring->head = I40E_PF_ATQH;
	ring->len  = I40E_PF_ATQLEN;
	ring->bal  = I40E_PF_ATQBAL;
	ring->bah  = I40E_PF_ATQBAH;

	err = i40e_aq_ring_alloc(ih, ring);
	if(err < 0)
		goto err_init_ring;

	/* initialize base registers */
	err = i40e_aq_ring_configure(ih, ring);
	if (err < 0)
		goto err_regs_config;

	return 0;

err_regs_config:
	i40e_aq_ring_release();
err_init_ring:
	return -1;
}

int i40e_aq_arq_init(struct ufp_handle *ih, struct ufp_i40e_aq_ring *ring)
{
	int err;

	ring->tail = I40E_PF_ARQT;
	ring->head = I40E_PF_ARQH;
	ring->len  = I40E_PF_ARQLEN;
	ring->bal  = I40E_PF_ARQBAL;
	ring->bah  = I40E_PF_ARQBAH;

	err = i40e_aq_ring_alloc(ih, ring);
	if(err < 0)
		goto err_init_ring;

	/* initialize base registers */
	err = i40e_aq_ring_configure(ih, ring);
	if (err < 0)
		goto err_regs_config;

	i40e_fill_arq();

	return 0;

err_regs_config:
	i40e_aq_ring_release();
err_init_ring:
	return -1;
}

int i40e_aq_ring_alloc(struct ufp_handle *ih, struct ufp_i40e_aq_ring *ring)
{
	uint64_t size;
	int i, page_allocated = 0;

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

	ring->desc = ufp_i40e_page_alloc(ih);
	if(!ring->desc)
		goto err_desc_alloc;

	/* allocate buffers in the rings */
	ring->bufs = malloc(ring->num_entries * sizeof(struct ufp_i40e_page *));
	if(!ring->bufs)
		goto err_buf_alloc;

	for(i = 0; i < ring->num_entries; i++, page_allocated++){
		ring->bufs[i] = ufp_i40e_page_alloc(ih);
		if(!ring->bufs[i])
			goto err_page_alloc;
	}

	return 0;

err_regs_config:
err_page_alloc:
	for(i = 0; i < page_allocated; i++)
		ufp_i40e_page_release(ring->bufs[i]);
	free(ring->bufs);
err_buf_alloc:
	ufp_i40e_page_release(ring->desc);
err_desc_alloc:
err_config:
err_already:
	return -1;
}

int i40e_aq_ring_configure(struct ufp_handle *ih, struct ufp_i40e_aq_ring *ring)
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

void i40e_aq_shutdown(struct i40e_hw *hw)
{
	if (i40e_check_asq_alive(hw))
		i40e_aq_queue_shutdown(hw, true);

	i40e_aq_ring_shutdown(asq);
	i40e_aq_ring_shutdown(arq);

	return;
}

void i40e_aq_ring_shutdown(struct i40e_hw *hw)
{
	/* Stop firmware AdminQ processing */
	ufp_write_reg(hw, ring->head, 0);
	ufp_write_reg(hw, ring->tail, 0);
	ufp_write_reg(hw, ring->len, 0);
	ufp_write_reg(hw, ring->bal, 0);
	ufp_write_reg(hw, ring->bah, 0);

	/* free ring buffers */
	i40e_aq_ring_release(hw);

	return;
}

void i40e_aq_ring_release(struct ufp_handle *ih, struct ufp_i40e_aq_ring *ring)
{
	int i;

	for(i = 0; i < ring->num_entries; i++)
		ufp_i40e_page_release(ring->bufs[i]);
	free(ring->bufs);
	ufp_i40e_page_release(ring->desc);

	return;
}

/**
 *  i40e_clean_asq - cleans Admin send queue
 *  @hw: pointer to the hardware structure
 *
 *  returns the number of free desc
 **/
u16 i40e_clean_asq(struct i40e_hw *hw)
{
	struct i40e_adminq_ring *asq = &(hw->aq.asq);
	u16 ntc = asq->next_to_clean;
	struct i40e_aq_desc *desc;

	desc = I40E_ADMINQ_DESC(*asq, ntc);
	while (ufp_read_reg(hw, ring->head) != ntc) {
		memset(desc, 0, sizeof(*desc));
		ntc++;
		if (ntc == asq->count)
			ntc = 0;
		desc = I40E_ADMINQ_DESC(*asq, ntc);
	}

	asq->next_to_clean = ntc;

	return I40E_DESC_UNUSED(asq);
}

int i40e_fill_arq()
{
	/* allocate the mapped buffers */
	for (i = 0; i < hw->aq.num_arq_entries; i++) {
		/* now configure the descriptors for use */
		desc = I40E_ADMINQ_DESC(hw->aq.arq, i);

		desc->flags = CPU_TO_LE16(I40E_AQ_FLAG_BUF);
		if (hw->aq.arq_buf_size > I40E_AQ_LARGE_BUF)
			desc->flags |= CPU_TO_LE16(I40E_AQ_FLAG_LB);
		desc->opcode = 0;
		/* This is in accordance with Admin queue design, there is no
		 * register for buffer size configuration
		 */
		desc->datalen = CPU_TO_LE16((u16)bi->size);
		desc->retval = 0;
		desc->cookie_high = 0;
		desc->cookie_low = 0;
		desc->params.external.addr_high =
			CPU_TO_LE32(upper_32_bits(bi->pa));
		desc->params.external.addr_low =
			CPU_TO_LE32(lower_32_bits(bi->pa));
		desc->params.external.param0 = 0;
		desc->params.external.param1 = 0;
	}

	/* Update tail in the HW to post pre-allocated buffers */
	ufp_write_reg(hw, ring->tail, hw->aq.num_arq_entries - 1);
}

i40e_status i40e_asq_send_command(struct i40e_hw *hw,
				struct i40e_aq_desc *desc,
				void *buff, /* can be NULL */
				u16  buff_size)
{
	i40e_status status = I40E_SUCCESS;
	struct i40e_dma_mem *dma_buff = NULL;
	struct i40e_aq_desc *desc_on_ring;
	bool cmd_completed = false;
	u16  retval = 0;
	u32  val = 0;

	struct timespec ts;
	uint32_t total_delay;

	hw->aq.asq_last_status = I40E_AQ_RC_OK;

	val = ufp_read_reg(hw, ring->head);
	if (val >= hw->aq.num_asq_entries) {
		i40e_debug(hw, I40E_DEBUG_AQ_MESSAGE,
			   "AQTX: head overrun at %d\n", val);
		status = I40E_ERR_QUEUE_EMPTY;
		goto asq_send_command_error;
	}

	if (buff_size > hw->aq.asq_buf_size) {
		i40e_debug(hw,
			   I40E_DEBUG_AQ_MESSAGE,
			   "AQTX: Invalid buffer size: %d.\n",
			   buff_size);
		status = I40E_ERR_INVALID_SIZE;
		goto asq_send_command_error;
	}

	if (i40e_clean_asq(hw) == 0) {
		i40e_debug(hw,
			   I40E_DEBUG_AQ_MESSAGE,
			   "AQTX: Error queue is full.\n");
		status = I40E_ERR_ADMIN_QUEUE_FULL;
		goto asq_send_command_error;
	}

	desc_on_ring = I40E_ADMINQ_DESC(hw->aq.asq, hw->aq.asq.next_to_use);
	memcpy(desc_on_ring, desc, sizeof(struct i40e_aq_desc));

	/* if buff is not NULL assume indirect command */
	if (buff != NULL) {
		dma_buff = &(hw->aq.asq.r.asq_bi[hw->aq.asq.next_to_use]);
		/* copy the user buff into the respective DMA buff */
		memcpy(dma_buff->va, buff, buff_size);
		desc_on_ring->datalen = CPU_TO_LE16(buff_size);

		/* Update the address values in the desc with the pa value
		 * for respective buffer
		 */
		desc_on_ring->params.external.addr_high =
				CPU_TO_LE32(upper_32_bits(dma_buff->pa));
		desc_on_ring->params.external.addr_low =
				CPU_TO_LE32(lower_32_bits(dma_buff->pa));
	}

	/* bump the tail */
	(hw->aq.asq.next_to_use)++;
	if (hw->aq.asq.next_to_use == hw->aq.num_entries)
		hw->aq.asq.next_to_use = 0;

	ufp_write_reg(hw, ring->tail, hw->aq.asq.next_to_use);

	do {
		if (ufp_read_reg(hw, ring->head) == ring->next_to_use)
			break;

		usleep(&ts, 1000);
		total_delay++;
	} while (total_delay < I40E_ASQ_CMD_TIMEOUT);

	if(total_delay == I40E_ASQ_CMD_TIMEOUT)
		goto err_timeout;

	memcpy(desc, desc_on_ring, sizeof(struct i40e_aq_desc));
	if (buff != NULL)
		memcpy(buff, dma_buff->va, buff_size);

	retval = LE16_TO_CPU(desc->retval);
	if(retval != 0)
		goto err_send;

	return 0;

err_send:
err_timeout:
	return -1;
}

/**
 *  i40e_fill_default_direct_cmd_desc - AQ descriptor helper function
 *  @desc:     pointer to the temp descriptor (non DMA mem)
 *  @opcode:   the opcode can be used to decide which flags to turn off or on
 *
 *  Fill the desc with default values
 **/
void i40e_fill_default_direct_cmd_desc(struct i40e_aq_desc *desc,
				       u16 opcode)
{
	/* zero out the desc */
	memset((void *)desc, 0, sizeof(struct i40e_aq_desc));
	desc->opcode = CPU_TO_LE16(opcode);
	desc->flags = CPU_TO_LE16(I40E_AQ_FLAG_SI);
}

int i40e_arq_clean(struct ufp_handle *ih, struct ufp_i40e_aq_ring *ring)
{
	u16 ntc = ring->next_to_clean;
	struct i40e_aq_desc *desc;
	struct i40e_aq_page *buf;
	u16 ntu;

	/* set next_to_use to head */
	ntu = (ufp_read_reg(hw, ring->head) & I40E_PF_ARQH_ARQH_MASK);
	if (ntu == ntc) {
		goto err_no_work;
	}

	/* now clean the next descriptor */
	desc = ((i40e_aq_desc *)ring->desc->addr_virt)[ntc];
	buf = ring->bufs[ntc];
	i40e_arq_process(desc, buf->addr_virt);

	/* Restore the original datalen and buffer address in the desc */
	memset((void *)desc, 0, sizeof(struct i40e_aq_desc));
	desc->flags = CPU_TO_LE16(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_LB);
	desc->datalen = CPU_TO_LE16((u16)bi->size);
	desc->params.external.addr_high = CPU_TO_LE32(upper_32_bits(buf->addr_dma));
	desc->params.external.addr_low = CPU_TO_LE32(lower_32_bits(buf->addr_dma));

	/* set tail = the last cleaned desc index. */
	ufp_write_reg(hw, ring->tail, ntc);
	/* ntc is updated to tail + 1 */
	ntc++;
	if (ntc == hw->aq.num_arq_entries)
		ntc = 0;
	hw->aq.arq.next_to_clean = ntc;
	hw->aq.arq.next_to_use = ntu;

	return 0;

err_no_work:
	return -1;
}

void i40e_arq_process(struct i40e_aq_desc *desc, void *buf)
{
	uint16_t len;
	uint16_t flags;

	len = LE16_TO_CPU(desc->datalen);
	flags = LE16_TO_CPU(desc->flags);

	if (flags & I40E_AQ_FLAG_ERR) {
		goto err_flag;
	}

err_flag:
	return;
}
