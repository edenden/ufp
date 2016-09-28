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

/**
 *  i40e_alloc_arq_bufs - Allocate pre-posted buffers for the receive queue
 *  @hw: pointer to the hardware structure
 **/
static i40e_status i40e_alloc_arq_bufs(struct i40e_hw *hw)
{
	i40e_status err;
	struct i40e_aq_desc *desc;
	struct i40e_dma_mem *bi;
	int i;

	/* We'll be allocating the buffer info memory first, then we can
	 * allocate the mapped buffers for the event processing
	 */

	/* buffer_info structures do not need alignment */
	err = i40e_allocate_virt_mem(hw, &hw->aq.arq.dma_head,
		(hw->aq.num_arq_entries * sizeof(struct i40e_dma_mem)));
	if (err)
		goto alloc_arq;
	hw->aq.arq.r.arq_bi = (struct i40e_dma_mem *)hw->aq.arq.dma_head.va;

	/* allocate the mapped buffers */
	for (i = 0; i < hw->aq.num_arq_entries; i++) {
		bi = &hw->aq.arq.r.arq_bi[i];
		err = i40e_allocate_dma_mem(hw, bi,
						 i40e_mem_arq_buf,
						 hw->aq.arq_buf_size,
						 I40E_ADMINQ_DESC_ALIGNMENT);
		if (err)
			goto unwind_alloc_arq;

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

alloc_arq:
	return err;

unwind_alloc_arq:
	/* don't try to free the one that failed... */
	i--;
	for (; i >= 0; i--)
		i40e_free_dma_mem(hw, &hw->aq.arq.r.arq_bi[i]);
	i40e_free_virt_mem(hw, &hw->aq.arq.dma_head);

	return err;
}

/**
 *  i40e_alloc_asq_bufs - Allocate empty buffer structs for the send queue
 *  @hw: pointer to the hardware structure
 **/
static i40e_status i40e_alloc_asq_bufs(struct i40e_hw *hw)
{
	i40e_status err;
	struct i40e_dma_mem *bi;
	int i;

	/* No mapped memory needed yet, just the buffer info structures */
	err = i40e_allocate_virt_mem(hw, &hw->aq.asq.dma_head,
		(hw->aq.num_asq_entries * sizeof(struct i40e_dma_mem)));
	if (err)
		goto alloc_asq_bufs;
	hw->aq.asq.r.asq_bi = (struct i40e_dma_mem *)hw->aq.asq.dma_head.va;

	/* allocate the mapped buffers */
	for (i = 0; i < hw->aq.num_asq_entries; i++) {
		bi = &hw->aq.asq.r.asq_bi[i];
		err = i40e_allocate_dma_mem(hw, bi,
						 i40e_mem_asq_buf,
						 hw->aq.asq_buf_size,
						 I40E_ADMINQ_DESC_ALIGNMENT);
		if (err)
			goto unwind_alloc_asq_bufs;
	}
alloc_asq_bufs:
	return err;

unwind_alloc_asq_bufs:
	/* don't try to free the one that failed... */
	i--;
	for (; i >= 0; i--)
		i40e_free_dma_mem(hw, &hw->aq.asq.r.asq_bi[i]);
	i40e_free_virt_mem(hw, &hw->aq.asq.dma_head);

	return err;
}

/**
 *  i40e_free_arq - Free receive queue buffer info elements
 *  @hw: pointer to the hardware structure
 **/
static void i40e_free_arq(struct i40e_hw *hw)
{
	int i;

	/* free descriptors */
	for (i = 0; i < hw->aq.num_arq_entries; i++)
		i40e_free_dma_mem(hw, &hw->aq.arq.r.arq_bi[i]);

	/* free the descriptor memory */
	ufp_mem_free(hw->aq.arq.desc_buf.va);

	/* free the dma header */
	i40e_free_virt_mem(hw, &hw->aq.arq.dma_head);
}

/**
 *  i40e_free_asq - Free send queue buffer info elements
 *  @hw: pointer to the hardware structure
 **/
static void i40e_free_asq(struct i40e_hw *hw)
{
	int i;

	/* only unmap if the address is non-NULL */
	for (i = 0; i < hw->aq.num_asq_entries; i++)
		if (hw->aq.asq.r.asq_bi[i].pa)
			i40e_free_dma_mem(hw, &hw->aq.asq.r.asq_bi[i]);

	/* free the descriptor memory */
	ufp_mem_free(hw->aq.asq.desc_buf.va);

	/* free the dma header */
	i40e_free_virt_mem(hw, &hw->aq.asq.dma_head);
}

/**
 *  i40e_config_asq_regs - configure ASQ registers
 *  @hw: pointer to the hardware structure
 *
 *  Configure base address and length registers for the transmit queue
 **/
static i40e_status i40e_config_asq_regs(struct i40e_hw *hw)
{
	i40e_status err = I40E_SUCCESS;
	u32 reg = 0;

	/* Clear Head and Tail */
	ufp_write_reg(hw, I40E_PF_ATQH, 0);
	ufp_write_reg(hw, I40E_PF_ATQT, 0);

	/* set starting point */
	ufp_write_reg(hw, I40E_PF_ATQLEN, (hw->aq.num_asq_entries |
				  I40E_PF_ATQLEN_ATQENABLE_MASK));
	ufp_write_reg(hw, I40E_PF_ATQBAL, lower_32_bits(hw->aq.asq.desc_buf.pa));
	ufp_write_reg(hw, I40E_PF_ATQBAH, upper_32_bits(hw->aq.asq.desc_buf.pa));

	/* Check one register to verify that config was applied */
	reg = ufp_read_reg(hw, I40E_PF_ATQBAL);
	if (reg != lower_32_bits(hw->aq.asq.desc_buf.pa))
		err = I40E_ERR_ADMIN_QUEUE_ERROR;

	return err;
}

/**
 *  i40e_config_arq_regs - ARQ register configuration
 *  @hw: pointer to the hardware structure
 *
 * Configure base address and length registers for the receive (event queue)
 **/
static i40e_status i40e_config_arq_regs(struct i40e_hw *hw)
{
	i40e_status err = I40E_SUCCESS;
	u32 reg = 0;

	/* Clear Head and Tail */
	ufp_write_reg(hw, I40E_PF_ARQH, 0);
	ufp_write_reg(hw, I40E_PF_ARQT, 0);

	/* set starting point */
	ufp_write_reg(hw, I40E_PF_ARQLEN, (hw->aq.num_arq_entries |
				  I40E_PF_ARQLEN_ARQENABLE_MASK));
	ufp_write_reg(hw, I40E_PF_ARQBAL, lower_32_bits(hw->aq.arq.desc_buf.pa));
	ufp_write_reg(hw, I40E_PF_ARQBAH, upper_32_bits(hw->aq.arq.desc_buf.pa));

	/* Update tail in the HW to post pre-allocated buffers */
	ufp_write_reg(hw, I40E_PF_ARQT, hw->aq.num_arq_entries - 1);

	/* Check one register to verify that config was applied */
	reg = ufp_read_reg(hw, I40E_PF_ARQBAL);
	if (reg != lower_32_bits(hw->aq.arq.desc_buf.pa))
		err = I40E_ERR_ADMIN_QUEUE_ERROR;

	return err;
}

int i40e_init_asq(struct i40e_hw *hw)
{
	uint64_t size;

	if (hw->aq.asq.count > 0) {
		/* queue already initialized */
		goto err_already;
	}

	/* verify input for valid configuration */
	if ((hw->aq.num_asq_entries == 0) ||
	    (hw->aq.asq_buf_size == 0)) {
		goto err_config;
	}

	hw->aq.asq.next_to_use = 0;
	hw->aq.asq.next_to_clean = 0;

	/* allocate the ring memory */
	size = hw->aq.num_asq_entries * sizeof(struct i40e_aq_desc);
	size = ALIGN(size, I40E_ADMINQ_DESC_ALIGNMENT);
	hw->aq.asq.desc_buf.va = ufp_mem_alloc(desc, size, I40E_ADMINQ_DESC_ALIGNMENT);
	if(!hw->aq.asq.desc_buf.va)
		goto err_desc_alloc;

	/* allocate buffers in the rings */
	err = i40e_alloc_asq_bufs(hw);
	if (err != I40E_SUCCESS)
		goto err_buf_alloc;

	/* initialize base registers */
	err = i40e_config_asq_regs(hw);
	if (err != I40E_SUCCESS)
		goto err_regs_config;

	/* success! */
	hw->aq.asq.count = hw->aq.num_asq_entries;

	return 0;

err_regs_config:
err_buf_alloc:
	ufp_mem_free(hw->aq.asq.desc_buf.va);
err_desc_alloc:
err_config:
err_already:
	return -1;
}

int i40e_init_arq(struct i40e_hw *hw)
{
	uint64_t size;

	if (hw->aq.arq.count > 0) {
		/* queue already initialized */
		goto err_already;
	}

	/* verify input for valid configuration */
	if ((hw->aq.num_arq_entries == 0) ||
	    (hw->aq.arq_buf_size == 0)) {
		goto err_config;
	}

	hw->aq.arq.next_to_use = 0;
	hw->aq.arq.next_to_clean = 0;

	/* allocate the ring memory */
	size = hw->aq.num_arq_entries * sizeof(struct i40e_aq_desc);
	size = ALIGN(size, I40E_ADMINQ_DESC_ALIGNMENT);
	hw->aq.arq.desc_buf.va = ufp_mem_alloc(desc, size, I40E_ADMINQ_DESC_ALIGNMENT);
	if(!hw->aq.arq.desc_buf.va)
		goto err_desc_alloc;

	/* allocate buffers in the rings */
	err = i40e_alloc_arq_bufs(hw);
	if (err != I40E_SUCCESS)
		goto err_buf_alloc;

	/* initialize base registers */
	err = i40e_config_arq_regs(hw);
	if (err != I40E_SUCCESS)
		goto err_regs_config;

	/* success! */
	hw->aq.arq.count = hw->aq.num_arq_entries;

	return 0;

err_regs_config:
err_buf_alloc:
	ufp_mem_free(hw->aq.arq.desc_buf.va);
err_desc_alloc:
err_config:
err_already:
	return -1;
}

/**
 *  i40e_shutdown_asq - shutdown the ASQ
 *  @hw: pointer to the hardware structure
 *
 *  The main shutdown routine for the Admin Send Queue
 **/
i40e_status i40e_shutdown_asq(struct i40e_hw *hw)
{
	i40e_status err = I40E_SUCCESS;

	if (hw->aq.asq.count == 0) {
		err = I40E_ERR_NOT_READY;
		goto shutdown_asq_out;
	}

	/* Stop firmware AdminQ processing */
	ufp_write_reg(hw, I40E_PF_ATQH, 0);
	ufp_write_reg(hw, I40E_PF_ATQT, 0);
	ufp_write_reg(hw, I40E_PF_ATQLEN, 0);
	ufp_write_reg(hw, I40E_PF_ATQBAL, 0);
	ufp_write_reg(hw, I40E_PF_ATQBAH, 0);

	hw->aq.asq.count = 0; /* to indicate uninitialized queue */

	/* free ring buffers */
	i40e_free_asq(hw);

shutdown_asq_out:
	return err;
}

/**
 *  i40e_shutdown_arq - shutdown ARQ
 *  @hw: pointer to the hardware structure
 *
 *  The main shutdown routine for the Admin Receive Queue
 **/
i40e_status i40e_shutdown_arq(struct i40e_hw *hw)
{
	i40e_status err = I40E_SUCCESS;

	if (hw->aq.arq.count == 0) {
		err = I40E_ERR_NOT_READY;
		goto shutdown_arq_out;
	}

	/* Stop firmware AdminQ processing */
	ufp_write_reg(hw, I40E_PF_ARQH, 0);
	ufp_write_reg(hw, I40E_PF_ARQT, 0);
	ufp_write_reg(hw, I40E_PF_ARQLEN, 0);
	ufp_write_reg(hw, I40E_PF_ARQBAL, 0);
	ufp_write_reg(hw, I40E_PF_ARQBAH, 0);

	hw->aq.arq.count = 0; /* to indicate uninitialized queue */

	/* free ring buffers */
	i40e_free_arq(hw);

shutdown_arq_out:
	return err;
}

int i40e_init_adminq(struct i40e_hw *hw)
{
	u16 cfg_ptr, oem_hi, oem_lo;
	u16 eetrack_lo, eetrack_hi;
	i40e_status err;
	int retry = 0;
	int err;

	/* verify input for valid configuration */
	if ((hw->aq.num_arq_entries == 0) ||
	    (hw->aq.num_asq_entries == 0) ||
	    (hw->aq.arq_buf_size == 0) ||
	    (hw->aq.asq_buf_size == 0)) {
		err = I40E_ERR_CONFIG;
		goto init_adminq_exit;
	}

	/* setup ASQ command write back timeout */
	hw->aq.asq_cmd_timeout = I40E_ASQ_CMD_TIMEOUT;

	/* allocate the ASQ */
	err = i40e_init_asq(hw);
	if(err < 0)
		goto err_init_asq;

	/* allocate the ARQ */
	err = i40e_init_arq(hw);
	if(err < 0)
		goto err_init_arq;

	return 0;

err_init_arq:
	i40e_shutdown_asq(hw);
err_init_asq:
	return -1;
}

/**
 *  i40e_shutdown_adminq - shutdown routine for the Admin Queue
 *  @hw: pointer to the hardware structure
 **/
i40e_status i40e_shutdown_adminq(struct i40e_hw *hw)
{
	i40e_status err = I40E_SUCCESS;

	if (i40e_check_asq_alive(hw))
		i40e_aq_queue_shutdown(hw, true);

	i40e_shutdown_asq(hw);
	i40e_shutdown_arq(hw);

	if (hw->nvm_buff.va)
		i40e_free_virt_mem(hw, &hw->nvm_buff);

	return err;
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
	while (ufp_read_reg(hw, I40E_PF_ATQH) != ntc) {
		memset(desc, 0, sizeof(*desc));
		ntc++;
		if (ntc == asq->count)
			ntc = 0;
		desc = I40E_ADMINQ_DESC(*asq, ntc);
	}

	asq->next_to_clean = ntc;

	return I40E_DESC_UNUSED(asq);
}

/**
 *  i40e_asq_done - check if FW has processed the Admin Send Queue
 *  @hw: pointer to the hw struct
 *
 *  Returns true if the firmware has processed all descriptors on the
 *  admin send queue. Returns false if there are still requests pending.
 **/
static bool i40e_asq_done(struct i40e_hw *hw)
{
	/* AQ designers suggest use of head for better
	 * timing reliability than DD bit
	 */
	return ufp_read_reg(hw, I40E_PF_ATQH) == hw->aq.asq.next_to_use;

}

/**
 *  i40e_asq_send_command - send command to Admin Queue
 *  @hw: pointer to the hw struct
 *  @desc: prefilled descriptor describing the command (non DMA mem)
 *  @buff: buffer to use for indirect commands
 *  @buff_size: size of buffer for indirect commands
 *  @cmd_details: pointer to command details structure
 *
 *  This is the main send command driver routine for the Admin Queue send
 *  queue.  It runs the queue, cleans the queue, etc
 **/
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

	if (hw->aq.asq.count == 0) {
		i40e_debug(hw, I40E_DEBUG_AQ_MESSAGE,
			   "AQTX: Admin queue not initialized.\n");
		status = I40E_ERR_QUEUE_EMPTY;
		goto asq_send_command_error;
	}

	val = ufp_read_reg(hw, I40E_PF_ATQH);
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

	/* call clean and check queue available function to reclaim the
	 * descriptors that were processed by FW, the function returns the
	 * number of desc available
	 */
	/* the clean function called here could be called in a separate thread
	 * in case of asynchronous completions
	 */
	if (i40e_clean_asq(hw) == 0) {
		i40e_debug(hw,
			   I40E_DEBUG_AQ_MESSAGE,
			   "AQTX: Error queue is full.\n");
		status = I40E_ERR_ADMIN_QUEUE_FULL;
		goto asq_send_command_error;
	}

	/* initialize the temp desc pointer with the right desc */
	desc_on_ring = I40E_ADMINQ_DESC(hw->aq.asq, hw->aq.asq.next_to_use);

	/* if the desc is available copy the temp desc to the right place */
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
	i40e_debug(hw, I40E_DEBUG_AQ_MESSAGE, "AQTX: desc and buffer:\n");
	i40e_debug_aq(hw, I40E_DEBUG_AQ_COMMAND, (void *)desc_on_ring,
		      buff, buff_size);
	(hw->aq.asq.next_to_use)++;
	if (hw->aq.asq.next_to_use == hw->aq.asq.count)
		hw->aq.asq.next_to_use = 0;

	ufp_write_reg(hw, I40E_PF_ATQT, hw->aq.asq.next_to_use);

	do {
		/* AQ designers suggest use of head for better
		 * timing reliability than DD bit
		 */
		if (i40e_asq_done(hw))
			break;
		usleep(&ts, 1000);
		total_delay++;
	} while (total_delay < I40E_ASQ_CMD_TIMEOUT);

	/* if ready, copy the desc back to temp */
	if (i40e_asq_done(hw)) {
		memcpy(desc, desc_on_ring, sizeof(struct i40e_aq_desc));
		if (buff != NULL)
			memcpy(buff, dma_buff->va, buff_size);
		retval = LE16_TO_CPU(desc->retval);
		if (retval != 0) {
			i40e_debug(hw,
				   I40E_DEBUG_AQ_MESSAGE,
				   "AQTX: Command completed with error 0x%X.\n",
				   retval);

			/* strip off FW internal code */
			retval &= 0xff;
		}
		cmd_completed = true;
		if ((enum i40e_admin_queue_err)retval == I40E_AQ_RC_OK)
			status = I40E_SUCCESS;
		else
			status = I40E_ERR_ADMIN_QUEUE_ERROR;
		hw->aq.asq_last_status = (enum i40e_admin_queue_err)retval;
	}

	i40e_debug(hw, I40E_DEBUG_AQ_MESSAGE,
		   "AQTX: desc and buffer writeback:\n");
	i40e_debug_aq(hw, I40E_DEBUG_AQ_COMMAND, (void *)desc, buff, buff_size);

	/* update the error if time out occurred */
	if (!cmd_completed){
		goto err_timeout;
	}

	return 0;

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

/**
 *  i40e_clean_arq_element
 *  @hw: pointer to the hw struct
 *  @e: event info from the receive descriptor, includes any buffers
 *  @pending: number of events that could be left to process
 *
 *  This function cleans one Admin Receive Queue element and returns
 *  the contents through e.  It can also return how many events are
 *  left to process through 'pending'
 **/
i40e_status i40e_clean_arq_element(struct i40e_hw *hw,
					     struct i40e_arq_event_info *e,
					     u16 *pending)
{
	i40e_status err = I40E_SUCCESS;
	u16 ntc = hw->aq.arq.next_to_clean;
	struct i40e_aq_desc *desc;
	struct i40e_dma_mem *bi;
	u16 desc_idx;
	u16 datalen;
	u16 flags;
	u16 ntu;

	/* pre-clean the event info */
	memset(&e->desc, 0, sizeof(e->desc));

	if (hw->aq.arq.count == 0) {
		i40e_debug(hw, I40E_DEBUG_AQ_MESSAGE,
			   "AQRX: Admin queue not initialized.\n");
		err = I40E_ERR_QUEUE_EMPTY;
		goto clean_arq_element_err;
	}

	/* set next_to_use to head */
	ntu = (ufp_read_reg(hw, I40E_PF_ARQH) & I40E_PF_ARQH_ARQH_MASK);
	if (ntu == ntc) {
		/* nothing to do - shouldn't need to update ring's values */
		err = I40E_ERR_ADMIN_QUEUE_NO_WORK;
		goto clean_arq_element_out;
	}

	/* now clean the next descriptor */
	desc = I40E_ADMINQ_DESC(hw->aq.arq, ntc);
	desc_idx = ntc;

	flags = LE16_TO_CPU(desc->flags);
	if (flags & I40E_AQ_FLAG_ERR) {
		err = I40E_ERR_ADMIN_QUEUE_ERROR;
		hw->aq.arq_last_status =
			(enum i40e_admin_queue_err)LE16_TO_CPU(desc->retval);
		i40e_debug(hw,
			   I40E_DEBUG_AQ_MESSAGE,
			   "AQRX: Event received with error 0x%X.\n",
			   hw->aq.arq_last_status);
	}

	memcpy(&e->desc, desc, sizeof(struct i40e_aq_desc));
	datalen = LE16_TO_CPU(desc->datalen);
	e->msg_len = min(datalen, e->buf_len);
	if (e->msg_buf != NULL && (e->msg_len != 0))
		memcpy(e->msg_buf, hw->aq.arq.r.arq_bi[desc_idx].va, e->msg_len);

	i40e_debug(hw, I40E_DEBUG_AQ_MESSAGE, "AQRX: desc and buffer:\n");
	i40e_debug_aq(hw, I40E_DEBUG_AQ_COMMAND, (void *)desc, e->msg_buf,
		      hw->aq.arq_buf_size);

	/* Restore the original datalen and buffer address in the desc,
	 * FW updates datalen to indicate the event message
	 * size
	 */
	bi = &hw->aq.arq.r.arq_bi[ntc];
	memset((void *)desc, 0, sizeof(struct i40e_aq_desc));

	desc->flags = CPU_TO_LE16(I40E_AQ_FLAG_BUF);
	if (hw->aq.arq_buf_size > I40E_AQ_LARGE_BUF)
		desc->flags |= CPU_TO_LE16(I40E_AQ_FLAG_LB);
	desc->datalen = CPU_TO_LE16((u16)bi->size);
	desc->params.external.addr_high = CPU_TO_LE32(upper_32_bits(bi->pa));
	desc->params.external.addr_low = CPU_TO_LE32(lower_32_bits(bi->pa));

	/* set tail = the last cleaned desc index. */
	ufp_write_reg(hw, I40E_PF_ARQT, ntc);
	/* ntc is updated to tail + 1 */
	ntc++;
	if (ntc == hw->aq.num_arq_entries)
		ntc = 0;
	hw->aq.arq.next_to_clean = ntc;
	hw->aq.arq.next_to_use = ntu;

	i40e_nvmupd_check_wait_event(hw, LE16_TO_CPU(e->desc.opcode));
clean_arq_element_out:
	/* Set pending if needed, unlock and return */
	if (pending != NULL)
		*pending = (ntc > ntu ? hw->aq.arq.count : 0) + (ntu - ntc);
clean_arq_element_err:

	return err;
}

