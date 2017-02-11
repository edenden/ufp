#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <lib_main.h>

#include "i40e_regs.h"
#include "i40e_main.h"
#include "i40e_aq.h"
#include "i40e_aqc.h"

static int i40e_aq_asq_init(struct ufp_dev *dev);
static int i40e_aq_arq_init(struct ufp_dev *dev);
static int i40e_aq_ring_alloc(struct ufp_dev *dev,
	struct i40e_aq_ring *ring);
static int i40e_aq_ring_configure(struct ufp_dev *dev,
	struct i40e_aq_ring *ring);
static void i40e_aq_asq_shutdown(struct ufp_dev *dev);
static void i40e_aq_arq_shutdown(struct ufp_dev *dev);
static void i40e_aq_ring_release(struct ufp_dev *dev,
	struct i40e_aq_ring *ring);
static uint16_t i40e_aq_desc_unused(struct i40e_aq_ring *ring,
	uint16_t num_desc);
static void i40e_aq_asq_process(struct ufp_dev *dev,
	struct i40e_aq_desc *desc, struct i40e_page *buf,
	struct i40e_aq_session *session);
static void i40e_aq_arq_process(struct ufp_dev *dev,
	struct i40e_aq_desc *desc, struct i40e_page *buf);

int i40e_aq_init(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	int err;

	/* allocate the ASQ */
	err = i40e_aq_asq_init(dev);
	if(err < 0)
		goto err_init_asq;

	/* allocate the ARQ */
	err = i40e_aq_arq_init(dev);
	if(err < 0)
		goto err_init_arq;

	list_init(&i40e_dev->aq.session);
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
	ring->num_desc = I40E_AQ_LEN;

	err = i40e_aq_ring_alloc(dev, ring);
	if(err < 0)
		goto err_aq_alloc_ring;

	/* initialize base registers */
	err = i40e_aq_ring_configure(dev, ring);
	if (err < 0)
		goto err_regs_config;

	i40e_dev->aq.tx_ring = ring;
	return 0;

err_regs_config:
	i40e_aq_ring_release(dev, ring);
err_aq_alloc_ring:
	free(ring);
err_alloc_ring:
	return -1;
}

static int i40e_aq_arq_init(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring;
	int err;

	ring = malloc(sizeof(struct i40e_aq_ring));
	if(!ring)
		goto err_alloc_ring;

	ring->tail = I40E_PF_ARQT;
	ring->head = I40E_PF_ARQH;
	ring->len  = I40E_PF_ARQLEN;
	ring->bal  = I40E_PF_ARQBAL;
	ring->bah  = I40E_PF_ARQBAH;
	ring->num_desc = I40E_AQ_LEN;

	err = i40e_aq_ring_alloc(dev, ring);
	if(err < 0)
		goto err_aq_alloc_ring;

	/* initialize base registers */
	err = i40e_aq_ring_configure(dev, ring);
	if (err < 0)
		goto err_regs_config;

	i40e_dev->aq.rx_ring = ring;
	i40e_aq_arq_assign(dev);
	return 0;

err_regs_config:
	i40e_aq_ring_release(dev, ring);
err_aq_alloc_ring:
	free(ring);
err_alloc_ring:
	return -1;
}

static int i40e_aq_ring_alloc(struct ufp_dev *dev,
	struct i40e_aq_ring *ring)
{
	uint64_t size;
	unsigned int buf_allocated = 0;
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

	for(i = 0; i < ring->num_desc; i++, buf_allocated++){
		ring->bufs[i] =  i40e_page_alloc(dev);
		if(!ring->bufs[i])
			goto err_page_alloc;
	}

	return 0;

err_page_alloc:
	for(i = 0; i < buf_allocated; i++){
		i40e_page_release(dev, ring->bufs[i]);
	}
err_buf_alloc:
	i40e_page_release(dev, ring->desc);
err_desc_alloc:
err_desc_size:
	return -1;
}

static int i40e_aq_ring_configure(struct ufp_dev *dev,
	struct i40e_aq_ring *ring)
{
	uint32_t reg;

	/* Clear Head and Tail */
	UFP_WRITE32(dev, ring->head, 0);
	UFP_WRITE32(dev, ring->tail, 0);

	/* set starting point */
	UFP_WRITE32(dev, ring->len,
		(ring->num_desc | I40E_MASK(0x1, 31)));
	UFP_WRITE32(dev, ring->bal, lower32(ring->desc->addr_dma));
	UFP_WRITE32(dev, ring->bah, upper32(ring->desc->addr_dma));

	/* Check one register to verify that config was applied */
	reg = UFP_READ32(dev, ring->bal);
	if (reg != lower32(ring->desc->addr_dma))
		goto err_init_ring;

	return 0;

err_init_ring:
	return -1;
}

void i40e_aq_destroy(struct ufp_dev *dev)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_session_create;

	i40e_aqc_req_queue_shutdown(dev, session);
	err = i40e_aqc_wait_cmd(dev, session);
	if(err < 0)
		goto err_wait_cmd;

err_wait_cmd:
	i40e_aq_session_delete(session);
err_session_create:

	/* Shutdown ASQ/ARQ anyway */
	i40e_aq_asq_shutdown(dev);
	i40e_aq_arq_shutdown(dev);

	return;
}

static void i40e_aq_asq_shutdown(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring = i40e_dev->aq.tx_ring;

	/* Stop firmware AdminQ processing */
	UFP_WRITE32(dev, ring->head, 0);
	UFP_WRITE32(dev, ring->tail, 0);
	UFP_WRITE32(dev, ring->len, 0);
	UFP_WRITE32(dev, ring->bal, 0);
	UFP_WRITE32(dev, ring->bah, 0);

	/* free ring buffers */
	i40e_aq_ring_release(dev, ring);
	free(ring);

	return;
}

static void i40e_aq_arq_shutdown(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring = i40e_dev->aq.rx_ring;

	/* Stop firmware AdminQ processing */
	UFP_WRITE32(dev, ring->head, 0);
	UFP_WRITE32(dev, ring->tail, 0);
	UFP_WRITE32(dev, ring->len, 0);
	UFP_WRITE32(dev, ring->bal, 0);
	UFP_WRITE32(dev, ring->bah, 0);

	/* free ring buffers */
	i40e_aq_ring_release(dev, ring);
	free(ring);

	return;
}

static void i40e_aq_ring_release(struct ufp_dev *dev,
	struct i40e_aq_ring *ring)
{
	int i;

	for(i = 0; i < ring->num_desc; i++){
		i40e_page_release(dev, ring->bufs[i]);
	}
	free(ring->bufs);
	i40e_page_release(dev, ring->desc);

	return;
}

struct i40e_aq_session *i40e_aq_session_create(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_session *session;

	session = malloc(sizeof(struct i40e_aq_session));
	if(!session)
		goto err_alloc_session;

	session->retval = 0xffff;
	list_add_last(&i40e_dev->aq.session, &session->list);
	return session;

err_alloc_session:
	return NULL;
}

void i40e_aq_session_delete(struct i40e_aq_session *session)
{
	list_del(&session->list);
	free(session);
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
	void *cmd, uint16_t cmd_size, void *data, uint16_t data_size,
	uint64_t cookie)
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

	desc = &((struct i40e_aq_desc *)
		ring->desc->addr_virt)[ring->next_to_use];
	desc->flags = htole16(flags);
	desc->opcode = htole16(opcode);
	memcpy(&desc->params, cmd, cmd_size);

	if(flags & I40E_AQ_FLAG_BUF){
		buf = ring->bufs[ring->next_to_use];

		if(flags & I40E_AQ_FLAG_RD){
			memcpy(buf->addr_virt, data, data_size);
			desc->datalen = htole16(data_size);
			if (data_size > I40E_AQ_LARGE_BUF)
				desc->flags |= htole16(I40E_AQ_FLAG_LB);
		}else{
			desc->datalen = htole16(I40E_MAX_AQ_BUF_SIZE);
			desc->flags |= htole16(I40E_AQ_FLAG_LB);
		}

		/* Update the address values in the desc with the pa value
		 * for respective buffer
		 */
		desc->params.external.addr_high =
			htole32(upper32(buf->addr_dma));
		desc->params.external.addr_low =
			htole32(lower32(buf->addr_dma));
	}


	desc->cookie_high = htole32(upper32(cookie));
	desc->cookie_low = htole32(lower32(cookie));

	next_to_use = ring->next_to_use + 1;
	ring->next_to_use =
		(next_to_use < ring->num_desc) ? next_to_use : 0;

	UFP_WRITE32(dev, ring->tail, ring->next_to_use);
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
		buf = ring->bufs[ring->next_to_use];

		/* now configure the descriptors for use */
		desc = &((struct i40e_aq_desc *)
			ring->desc->addr_virt)[ring->next_to_use];

		desc->flags = htole16(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_LB);
		desc->opcode = 0;
		/* This is in accordance with Admin queue design, there is no
		 * register for buffer size configuration
		 */
		desc->datalen = htole16(I40E_MAX_AQ_BUF_SIZE);
		desc->retval = 0;
		desc->cookie_high = 0;
		desc->cookie_low = 0;
		desc->params.external.addr_high =
			htole32(upper32(buf->addr_dma));
		desc->params.external.addr_low =
			htole32(lower32(buf->addr_dma));
		desc->params.external.param0 = 0;
		desc->params.external.param1 = 0;

		next_to_use = ring->next_to_use + 1;
		ring->next_to_use =
			(next_to_use < ring->num_desc) ? next_to_use : 0;

		total_allocated++;
	}

	if(total_allocated){
		UFP_WRITE32(dev, ring->tail, ring->next_to_use);
	}
	return;
}

void i40e_aq_asq_clean(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_aq_ring *ring = i40e_dev->aq.tx_ring;
	struct i40e_aq_desc *desc;
	struct i40e_page *buf;
	struct i40e_aq_session *_session, *session;
	uint64_t cookie;
	uint16_t next_to_clean;

	/* AQ designers suggest use of head for better
	 * timing reliability than DD bit
	 */
	while(ring->next_to_clean != UFP_READ32(dev, ring->head)){
		desc = &((struct i40e_aq_desc *)
			ring->desc->addr_virt)[ring->next_to_clean];
		buf = ring->bufs[ring->next_to_clean];
		session = NULL;

		cookie = ((uint64_t)le32toh(desc->cookie_low))
			| (((uint64_t)le32toh(desc->cookie_high)) << 32);
		if(cookie){
			list_for_each(&i40e_dev->aq.session, _session, list){
				if(_session == (void *)cookie){
					session = _session;
					break;
				}
			}
		}

		i40e_aq_asq_process(dev, desc, buf, session);

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

	while(ring->next_to_clean
	!= (UFP_READ32(dev, ring->head) & I40E_PF_ARQH_ARQH_MASK)){
		/* now clean the next descriptor */
		desc = &((struct i40e_aq_desc *)
			ring->desc->addr_virt)[ring->next_to_clean];
		buf = ring->bufs[ring->next_to_clean];

		i40e_aq_arq_process(dev, desc, buf);

		next_to_clean = ring->next_to_clean + 1;
		ring->next_to_clean =
			(next_to_clean < ring->num_desc) ? next_to_clean : 0;
	}

	return;
}

static void i40e_aq_asq_process(struct ufp_dev *dev,
	struct i40e_aq_desc *desc, struct i40e_page *buf,
	struct i40e_aq_session *session)
{
	uint16_t retval;
	uint16_t opcode;

	retval = le16toh(desc->retval);
	opcode = le16toh(desc->opcode);

	if(retval != 0)
		goto err_retval;

	if(session)
		session->retval = 0;

	switch(opcode){
	case i40e_aq_opc_macaddr_read:
		i40e_aqc_resp_macaddr_read(dev, &desc->params,
			buf->addr_virt, session);
		break;
	case i40e_aq_opc_clear_pxemode:
		i40e_aqc_resp_clear_pxemode(dev);
		break;
	case i40e_aq_opc_get_swconf:
		i40e_aqc_resp_get_swconf(dev, &desc->params,
			buf->addr_virt, session);
		break;
	case i40e_aq_opc_rxctl_read:
		i40e_aqc_resp_rxctl_read(dev, &desc->params,
			session);
		break;
	case i40e_aq_opc_update_vsi:
		i40e_aqc_resp_update_vsi(dev, &desc->params,
			buf->addr_virt, session);
		break;
	default:
		goto err_opcode;
		break;
	}
	return;

err_opcode:
err_retval:
	if(session)
		session->retval = -1;
	return;
}

static void i40e_aq_arq_process(struct ufp_dev *dev,
	struct i40e_aq_desc *desc, struct i40e_page *buf)
{
	uint16_t opcode;
	uint16_t flags;

	opcode = le16toh(desc->opcode);
	flags = le16toh(desc->flags);

	if (flags & I40E_AQ_FLAG_ERR) {
		goto err_flag;
	}

	switch(opcode){
	default:
		goto err_opcode;
		break;
	}
	return;

err_opcode:
err_flag:
	return;
}
