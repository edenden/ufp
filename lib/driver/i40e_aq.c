#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

int ufp_i40e_aq_init(struct ufp_dev *dev)
{       
	int err;

	/* allocate the ASQ */
	err = i40e_aq_asq_init(dev);
	if(err < 0)
		goto err_init_asq;
	
	/* allocate the ARQ */
	err = i40e_aq_arq_init(dev);
	if(err < 0)
		goto err_init_arq;

	dev->num_misc_irqs += 1;
	
	return 0;

err_init_arq:
	i40e_aq_asq_shutdown(dev);
err_init_asq:
	return -1;
}

static int i40e_aq_asq_init(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring;
	int err;

	ring = malloc(sizeof(struct i40e_aq_ring));
	if(!ring)
		goto err_alloc_ring;

	ring->tail = I40E_PF_ATQT;
	ring->head = I40E_PF_ATQH;
	ring->len  = I40E_PF_ATQLEN;
	ring->bal  = I40E_PF_ATQBAL;
	ring->bah  = I40E_PF_ATQBAH;
	ring->num_desc = XXX;

	err = i40e_aq_ring_alloc(dev, ring);
	if(err < 0)
		goto err_alloc_ring;

	/* initialize base registers */
	err = i40e_aq_ring_configure(dev, ring);
	if (err < 0)
		goto err_regs_config;

	i40e_dev->aq.tx_ring = ring;
	return 0;

err_regs_config:
	i40e_aq_ring_release(dev, ring);
err_init_ring:
	free(ring);
err_alloc_ring:
	return -1;
}

static int i40e_aq_arq_init(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring;
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
	ring->num_desc = XXX;

	err = i40e_aq_ring_alloc(dev, ring);
	if(err < 0)
		goto err_alloc_ring;

	/* initialize base registers */
	err = i40e_aq_ring_configure(dev, ring);
	if (err < 0)
		goto err_regs_config;

	i40e_dev->aq.rx_ring = ring;
	i40e_aq_arq_assign(dev);
	return 0;

err_regs_config:
	i40e_aq_ring_release(dev, ring);
err_init_ring:
	free(ring);
err_alloc_ring:
	return -1;
}

static int i40e_aq_ring_alloc(struct ufp_dev *dev, struct ufp_i40e_aq_ring *ring)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	uint64_t size;
	int i;

	ring->next_to_use = 0;
	ring->next_to_clean = 0;

	/* allocate the ring memory */
	size = ring->num_desc * sizeof(struct i40e_aq_desc);
	size = ALIGN(size, I40E_ADMINQ_DESC_ALIGNMENT);
	if(size > sysconf(_SC_PAGESIZE))
		goto err_desc_size;

	ring->desc = i40e_page_alloc(dev);
	if(!ring->desc)
		goto err_desc_alloc;

	/* allocate buffers in the rings */
	ring->bufs = malloc(ring->num_desc * sizeof(struct i40e_page *));
	if(!ring->bufs)
		goto err_buf_alloc;

	return 0;

err_buf_alloc:
	i40e_page_release(ring->desc);
err_desc_alloc:
err_config:
err_already:
	return -1;
}

static int i40e_aq_ring_configure(struct ufp_dev *dev, struct ufp_i40e_aq_ring *ring)
{
	/* Clear Head and Tail */
	ufp_write_reg(hw, ring->head, 0);
	ufp_write_reg(hw, ring->tail, 0);

	/* set starting point */
	ufp_write_reg(hw, ring->len, (ring->num_desc | I40E_MASK(0x1, 31));
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

void i40e_aq_destroy(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	i40e_aq_queue_shutdown(dev, true);
	i40e_aq_asq_shutdown(dev);
	i40e_aq_arq_shutdown(dev);

	return;
}

static void i40e_aq_asq_shutdown(ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	/* Stop firmware AdminQ processing */
	ufp_write_reg(hw, ring->head, 0);
	ufp_write_reg(hw, ring->tail, 0);
	ufp_write_reg(hw, ring->len, 0);
	ufp_write_reg(hw, ring->bal, 0);
	ufp_write_reg(hw, ring->bah, 0);

	/* free ring buffers */
	i40e_aq_ring_release(dev, i40e_dev->aq.tx_ring);
	free(i40e_dev->aq.tx_ring);

	return;
}

static void i40e_aq_arq_shutdown(ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;

	/* Stop firmware AdminQ processing */
	ufp_write_reg(hw, ring->head, 0);
	ufp_write_reg(hw, ring->tail, 0);
	ufp_write_reg(hw, ring->len, 0);
	ufp_write_reg(hw, ring->bal, 0);
	ufp_write_reg(hw, ring->bah, 0);

	/* free ring buffers */
	i40e_aq_ring_release(dev, i40e_dev->aq.rx_ring);
	free(i40e_dev->aq.rx_ring);

	return;
}

static void i40e_aq_ring_release(struct ufp_dev *dev, struct ufp_i40e_aq_ring *ring)
{
	int i;

	for(i = 0; i < ring->num_desc; i++){
		if(ring->bufs[i])
			i40e_page_release(ring->bufs[i]);
	}
	free(ring->bufs);
	i40e_page_release(ring->desc);

	return;
}

static uint16_t i40e_aq_desc_unused(struct i40e_aq_ring *ring,
	uint16_t num_desc)
{
	uint16_t next_to_clean = ring->next_to_clean;
	uint16_t next_to_use = ring->next_to_use;

	return next_to_clean > next_to_use
		? next_to_clean - next_to_use - 1
		: (num_desc - next_to_use) + next_to_clean - 1;
}

void i40e_aq_asq_assign(struct ufp_dev *dev, uint16_t opcode, uint16_t flags,
	void *cmd, uint16_t cmd_size, void *data, uint16_t data_size)
{
	struct i40e_dev *i40e_dev = dev->drv_data; 
	struct i40e_aq_ring *ring = i40e_dev->aq.tx_ring;
	struct i40e_aq_desc *desc;
	struct i40e_page *buf;
	uint16_t next_to_use;
	uint16_t unused_count;

	unused_count = i40e_aq_desc_unused(ring, ring->num_desc);
	if(!unused_count)
		return;

	desc = &((struct ufp_aq_desc *)
		ring->desc->addr_virt)[ring->next_to_use];
	desc->flags = CPU_TO_LE16(flags);
	desc->opcode = CPU_TO_LE16(opcode);
	memcpy(&desc->params, cmd, cmd_size);

	if(flags & I40E_AQ_FLAG_BUF){
		buf = i40e_page_alloc(dev);
		if(!buf)
			goto err_page_alloc;
		ring->bufs[ring->next_to_use] = buf;

		if(flag & I40E_AQ_FLAG_RD){
			memcpy(buf->addr_virt, data, data_size);
			desc->datalen = CPU_TO_LE16(data_size);
		}else{
			desc->datalen = CPU_TO_LE16(I40E_AQ_LARGE_BUF);
		}

		/* Update the address values in the desc with the pa value
		 * for respective buffer
		 */
		desc->params.external.addr_high =
			CPU_TO_LE32(upper_32_bits(buf->addr_dma));
		desc->params.external.addr_low =
			CPU_TO_LE32(lower_32_bits(buf->addr_dma));
	}

	next_to_use = ring->next_to_use + 1;
	ring->next_to_use =
		(next_to_use < ring->num_desc) ? next_to_use : 0;

	ufp_write_reg(dev, ring->tail, ring->next_to_use);
	return;
}

void i40e_aq_arq_assign(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring = i40e_dev->aq.rx_ring;
	struct i40e_aq_desc *desc;
	struct i40e_page *buf;
	unsigned int total_allocated;
	uint16_t max_allocation;
	uint16_t next_to_use;

	max_allocation = i40e_aq_desc_unused(ring, ring->num_desc);
	if (!max_allocation)
		return;

	total_allocated = 0;
	while(total_allocated < max_allocation){
		buf = i40e_page_alloc(dev);
		if(!buf)
			break;

		ring->bufs[i] = buf;

		/* now configure the descriptors for use */
		desc = &((struct ufp_aq_desc *)
			ring->desc->addr_virt)[ring->next_to_use];

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

		next_to_use = ring->next_to_use + 1;
		ring->next_to_use =
			(next_to_use < ring->num_desc) ? next_to_use : 0;
	
		total_allocated++;
	}

	if(total_allocated){
		ufp_write_reg(dev, ring->tail, ring->next_to_use);
	}
	return;
}

void i40e_aq_asq_clean(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring = i40e_dev->aq.tx_ring;
	struct i40e_aq_desc *desc;
	struct i40e_page *buf;
	uint16_t next_to_clean;

	/* AQ designers suggest use of head for better
	 * timing reliability than DD bit
	 */
	while(ring->next_to_clean != ufp_read_reg(dev, ring->head)){
		desc = &((struct ufp_aq_desc *)
			ring->desc->addr_virt)[ring->next_to_clean];
		buf = ring->bufs[ring->next_to_clean];
		i40e_aq_asq_process(dev, desc, buf);
		if(desc->flags & I40E_AQ_FLAG_BUF)
			i40e_page_release(dev, buf);

		next_to_clean = ring->next_to_clean + 1;
		ring->next_to_clean =
			(next_to_clean < ring->num_desc) ? next_to_clean : 0;
	}

	return;
}

void i40e_aq_arq_clean(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring = i40e_dev->aq.rx_ring;
	struct i40e_aq_desc *desc;
	struct i40e_page *buf;
	uint16_t next_to_clean;

	while(ring->next_to_clean != (ufp_read_reg(hw, ring->head) & I40E_PF_ARQH_ARQH_MASK)){
		/* now clean the next descriptor */
		desc = &((struct ufp_aq_desc *)
			ring->desc->addr_virt)[ring->next_to_clean];
		buf = ring->bufs[ring->next_to_clean];
		i40e_arq_process(dev, desc, buf);
		i40e_page_release(dev, buf);

		next_to_clean = ring->next_to_clean + 1;
		ring->next_to_clean =
			(next_to_clean < ring->num_desc) ? next_to_clean : 0;
	}

	return;
}

static int i40e_aq_asq_process(struct ufp_dev *dev, struct i40e_aq_desc *desc,
	struct i40e_page *buf)
{
	uint16_t retval;
	uint16_t opcode;
	int err = -1;

	retval = LE16_TO_CPU(desc->retval);
	opcode = LE16_TO_CPU(desc->opcode);

	if(retval != 0)
		goto err_retval;

	switch(opcode){
	case i40e_aqc_opc_mac_addr:
		err = i40e_aq_cmd_clean_macaddr(dev,
			&desc->params, buf->addr_virt);
		break;
	case i40e_aqc_opc_clear_pxe:
		err = i40e_aq_cmd_clean_clearpxe(dev,
			&desc->params);
		break;
	case i40e_aqc_opc_add_vsi:
		err = i40e_aq_cmd_clean_addvsi(dev,
			&desc->params);
		break;
	default:
		break;
	}

err_retval:
	return err;
}

static int i40e_aq_arq_process(struct ufp_dev *dev, struct i40e_aq_desc *desc,
	struct i40e_page *buf)
{
	uint16_t len;
	uint16_t flags;
	uint16_t opcode;
	int err = -1;

	len = LE16_TO_CPU(desc->datalen);
	flags = LE16_TO_CPU(desc->flags);
	opcode = LE16_TO_CPU(desc->opcode);

	if (flags & I40E_AQ_FLAG_ERR) {
		goto err_flag;
	}

	switch(opcode){
	case :
		err = ;
		break;
	default:
		break;
	}

err_flag:
	return err;
}
