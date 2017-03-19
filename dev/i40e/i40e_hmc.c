#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <lib_main.h>
#include <lib_dev.h>

#include "i40e_main.h"
#include "i40e_regs.h"
#include "i40e_hmc.h"

static int i40e_hmc_configure(struct ufp_dev *dev);
static void i40e_hmc_shutdown(struct ufp_dev *dev);
static int i40e_hmc_sd_allocate(struct ufp_dev *dev,
	struct i40e_hmc_sd_entry *sd_entry, uint32_t sd_index);
static void i40e_hmc_sd_release(struct ufp_dev *dev,
	struct i40e_hmc_sd_entry *sd_entry, uint32_t sd_index);
static void i40e_set_pf_sd_entry(struct ufp_dev *dev, unsigned long pa,
	uint32_t sd_index,  enum i40e_sd_entry_type type);
static void i40e_clear_pf_sd_entry(struct ufp_dev *dev,
	uint32_t sd_index, enum i40e_sd_entry_type type);
static void *i40e_hmc_va(struct ufp_dev *dev, struct i40e_hmc_obj *obj,
	uint16_t qp_idx);
static void i40e_hmc_write(uint8_t *hmc_bits,
	struct i40e_hmc_ce *ce, uint8_t *host_buf);
static void i40e_hmc_read(uint8_t *hmc_bits,
	struct i40e_hmc_ce *ce, uint8_t *host_buf);

int i40e_hmc_init(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_hmc *hmc = &i40e_dev->hmc;
	uint64_t fpm_size, obj_size_tx, obj_size_rx;
	uint32_t queue_max;
	int err;

	queue_max = UFP_READ32(dev, I40E_GLHMC_LANQMAX);

	obj_size_tx = BIT_ULL(UFP_READ32(dev, I40E_GLHMC_LANTXOBJSZ));
	obj_size_rx = BIT_ULL(UFP_READ32(dev, I40E_GLHMC_LANRXOBJSZ));
	fpm_size = obj_size_tx + obj_size_rx;

	/* Tx queue context information */
	hmc->obj_tx.count = queue_max;
	hmc->obj_tx.base = 0;
	hmc->obj_tx.size = obj_size_tx;

	/* Rx queue context information */
	hmc->obj_rx.count = queue_max;
	hmc->obj_rx.base = hmc->obj_tx.base +
		ALIGN(queue_max * obj_size_tx,
		I40E_HMC_L2OBJ_BASE_ALIGNMENT);
	hmc->obj_rx.size = obj_size_rx;

	/* allocate memory for SD entry table */
	hmc->sd_table.sd_count = (fpm_size + I40E_HMC_DIRECT_BP_SIZE - 1)
		/ I40E_HMC_DIRECT_BP_SIZE;
	hmc->sd_table.sd_entry = malloc(
		sizeof(struct i40e_hmc_sd_entry) * hmc->sd_table.sd_count);
	if(!hmc->sd_table.sd_entry)
		goto err_alloc_sd_entry;

	err = i40e_hmc_configure(dev);
	if(err < 0)
		goto err_hmc_configure;

	return 0;

err_hmc_configure:
	free(hmc->sd_table.sd_entry);
err_alloc_sd_entry:
	return -1;
}

void i40e_hmc_destroy(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_hmc *hmc = &i40e_dev->hmc;

	i40e_hmc_shutdown(dev);
	free(hmc->sd_table.sd_entry);

	return;
}

static int i40e_hmc_configure(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_hmc *hmc = &i40e_dev->hmc;
	struct i40e_hmc_sd_entry *sd_entry;
	unsigned int sd_allocated = 0;
	uint32_t hmc_base, hmc_count;
	int i, err;

	for (i = 0; i < hmc->sd_table.sd_count; i++, sd_allocated++){
		sd_entry = &hmc->sd_table.sd_entry[i];

		err = i40e_hmc_sd_allocate(dev, sd_entry, i);
		if(err < 0)
			goto err_alloc_sd;
	}

	/*
	 * Configure and program the FPM registers
	 * so objects can be created
	 */
	hmc_base = (hmc->obj_tx.base
		& I40E_GLHMC_LANTXBASE_FPMLANTXBASE_MASK)
		/ 512;
	hmc_count = hmc->obj_tx.count;
	UFP_WRITE32(dev, I40E_GLHMC_LANTXBASE(i40e_dev->pf_id), hmc_base);
	UFP_WRITE32(dev, I40E_GLHMC_LANTXCNT(i40e_dev->pf_id), hmc_count);

	hmc_base = (hmc->obj_rx.base
		& I40E_GLHMC_LANRXBASE_FPMLANRXBASE_MASK)
		/ 512;
	hmc_count = hmc->obj_rx.count;
	UFP_WRITE32(dev, I40E_GLHMC_LANRXBASE(i40e_dev->pf_id), hmc_base);
	UFP_WRITE32(dev, I40E_GLHMC_LANRXCNT(i40e_dev->pf_id), hmc_count);

	return 0;

err_alloc_sd:
	for(i = 0; i < sd_allocated; i++){
		sd_entry = &hmc->sd_table.sd_entry[i];
		i40e_hmc_sd_release(dev, sd_entry, i);
	}
	return -1;
}

static void i40e_hmc_shutdown(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_hmc *hmc = &i40e_dev->hmc;
	struct i40e_hmc_sd_entry *sd_entry;
	int i;

	for (i = 0; i < hmc->sd_table.sd_count; i++){
		sd_entry = &hmc->sd_table.sd_entry[i];
		i40e_hmc_sd_release(dev, sd_entry, i);
	}

	return;
}

static int i40e_hmc_sd_allocate(struct ufp_dev *dev,
	struct i40e_hmc_sd_entry *sd_entry, uint32_t sd_index)
{
	unsigned int pd_allocated = 0;
	struct ufp_dev_buf *pd;
	uint64_t *pd_addr;
	int i;

	sd_entry->pd_addrs = ufp_dev_dma_alloc(I40E_HMC_PAGED_BP_SIZE);
	if(!sd_entry->pd_addrs)
		goto err_alloc_pd_addrs;

	for(i = 0; i < I40E_HMC_MAX_BP_COUNT; i++, pd_allocated++){
		pd = ufp_dev_dma_alloc(I40E_HMC_PAGED_BP_SIZE);
		if(!pd)
			goto err_alloc_pd;

		pd_addr = &(((uint64_t *)(sd_entry->pd_addrs->addr_virt))[i]);
		/* Set page address and valid bit */
		*pd_addr = pd->addr_dma | 0x1;

		sd_entry->pd[i] = pd;
	}

	i40e_set_pf_sd_entry(dev, sd_entry->pd_addrs->addr_dma, sd_index,
		I40E_SD_TYPE_PAGED);

	return 0;

err_alloc_pd:
	for(i = 0; i < pd_allocated; i++){
		ufp_dev_dma_free(sd_entry->pd[i]);
	}
	ufp_dev_dma_free(sd_entry->pd_addrs);
err_alloc_pd_addrs:
	return -1;
}

static void i40e_hmc_sd_release(struct ufp_dev *dev,
	struct i40e_hmc_sd_entry *sd_entry, uint32_t sd_index)
{
	struct ufp_dev_buf *pd;
	int i;

	i40e_clear_pf_sd_entry(dev, sd_index, I40E_SD_TYPE_PAGED);

	for(i = 0; i < I40E_HMC_MAX_BP_COUNT; i++){
		pd = sd_entry->pd[i];

		UFP_WRITE32(dev, I40E_PFHMC_PDINV,
			(sd_index << I40E_PFHMC_PDINV_PMSDIDX_SHIFT)
			| (i << I40E_PFHMC_PDINV_PMPDIDX_SHIFT));

		ufp_dev_dma_free(pd);
	}

	ufp_dev_dma_free(sd_entry->pd_addrs);
	return;
}

static void i40e_set_pf_sd_entry(struct ufp_dev *dev, unsigned long pa,
	uint32_t sd_index,  enum i40e_sd_entry_type type)
{
	uint32_t val1, val2, val3;

	val1 = upper32(pa);
	val2 = lower32(pa);
	val2 |= I40E_HMC_MAX_BP_COUNT
		<< I40E_PFHMC_SDDATALOW_PMSDBPCOUNT_SHIFT;
	val2 |= (((type) == I40E_SD_TYPE_PAGED) ? 0 : 1)
		<< I40E_PFHMC_SDDATALOW_PMSDTYPE_SHIFT;
	val2 |= BIT(I40E_PFHMC_SDDATALOW_PMSDVALID_SHIFT);
	val3 = (sd_index) | BIT_ULL(I40E_PFHMC_SDCMD_PMSDWR_SHIFT);

	UFP_WRITE32(dev, I40E_PFHMC_SDDATAHIGH, val1);
	UFP_WRITE32(dev, I40E_PFHMC_SDDATALOW, val2);
	UFP_WRITE32(dev, I40E_PFHMC_SDCMD, val3);

	return;
}

static void i40e_clear_pf_sd_entry(struct ufp_dev *dev,
	uint32_t sd_index, enum i40e_sd_entry_type type)
{
	uint32_t val2, val3;

	val2 = I40E_HMC_MAX_BP_COUNT
		<< I40E_PFHMC_SDDATALOW_PMSDBPCOUNT_SHIFT;
	val2 |= (((type) == I40E_SD_TYPE_PAGED) ? 0 : 1)
		<< I40E_PFHMC_SDDATALOW_PMSDTYPE_SHIFT;

	val3 = (sd_index) | BIT_ULL(I40E_PFHMC_SDCMD_PMSDWR_SHIFT);

	UFP_WRITE32(dev, I40E_PFHMC_SDDATAHIGH, 0);
	UFP_WRITE32(dev, I40E_PFHMC_SDDATALOW, val2);
	UFP_WRITE32(dev, I40E_PFHMC_SDCMD, val3);

	return;
}

static void *i40e_hmc_va(struct ufp_dev *dev, struct i40e_hmc_obj *obj,
	uint16_t qp_idx)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_hmc *hmc = &i40e_dev->hmc;
	struct i40e_hmc_sd_entry *sd_entry;
	struct ufp_dev_buf *pd;
	uint32_t page_idx, sd_idx, pd_idx;
	uint64_t addr_fpm, offset;
	void *hmc_va;

	addr_fpm = (qp_idx * obj->size) + obj->base;
	page_idx = addr_fpm / I40E_HMC_PAGED_BP_SIZE;
	sd_idx = page_idx / I40E_HMC_MAX_BP_COUNT;
	pd_idx = page_idx % I40E_HMC_MAX_BP_COUNT;
	offset = addr_fpm % I40E_HMC_PAGED_BP_SIZE;

	if(sd_idx >= hmc->sd_table.sd_count)
		goto err_sd_idx;

	sd_entry = &hmc->sd_table.sd_entry[sd_idx];
	pd = sd_entry->pd[pd_idx];
	hmc_va = pd->addr_virt + offset;

	return hmc_va;

err_sd_idx:
	return NULL;
}

int i40e_hmc_set_ctx_tx(struct ufp_dev *dev, struct i40e_hmc_ctx_tx *ctx,
	uint16_t qp_idx)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_hmc *hmc = &i40e_dev->hmc;
	struct i40e_hmc_ce ce[] = I40E_HMC_CE_TX;
	void *hmc_va;
	int i;

	hmc_va = i40e_hmc_va(dev, &hmc->obj_tx, qp_idx);
	if(!hmc_va)
		goto err_hmc_va;

	memset(hmc_va, 0, hmc->obj_tx.size);
	for(i = 0; ce[i].width != 0; i++){
		i40e_hmc_write(hmc_va, &ce[i], (uint8_t *)ctx);
	}

	return 0;

err_hmc_va:
	return -1;
}

int i40e_hmc_set_ctx_rx(struct ufp_dev *dev, struct i40e_hmc_ctx_rx *ctx,
	uint16_t qp_idx)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_hmc *hmc = &i40e_dev->hmc;
	struct i40e_hmc_ce ce[] = I40E_HMC_CE_RX;
	void *hmc_va;
	int i;

	hmc_va = i40e_hmc_va(dev, &hmc->obj_rx, qp_idx);
	if(!hmc_va)
		goto err_hmc_va;

	memset(hmc_va, 0, hmc->obj_rx.size);
	for(i = 0; ce[i].width != 0; i++){
		i40e_hmc_write(hmc_va, &ce[i], (uint8_t *)ctx);
	}

	return 0;

err_hmc_va:
	return -1;
}

static void i40e_hmc_write(uint8_t *hmc_bits,
	struct i40e_hmc_ce *ce, uint8_t *host_buf)
{
	uint8_t data[8], mask[8];
	uint8_t *src, *dst;
	uint8_t byte_data, prev_data, byte_mask, prev_mask;
	uint16_t shift_width, width_remain;
	int i;

	/* copy from the next struct field */
	src = host_buf + ce->offset;

	for(i = 0; i < ce->size_of; i++){
#if __BYTE_ORDER == __LITTLE_ENDIAN
		data[i] = src[i];
#elif __BYTE_ORDER == __BIG_ENDIAN
		data[i] = src[ce->size_of - (i + 1)];
#endif

		width_remain = ce->width - (i * 8);
		mask[i] = (1 << min(width_remain, (uint16_t)8)) - 1;
	}

	/* prepare the bits and mask */
	shift_width = ce->lsb % 8;

	/* get the current bits from the target bit string */
	dst = hmc_bits + (ce->lsb / 8);

	prev_data = 0;
	prev_mask = 0;
	for(i = 0; i < ce->size_of; i++){
		byte_data = data[i] << shift_width;
		byte_data |= prev_data;

		byte_mask = mask[i] << shift_width;
		byte_mask |= prev_mask;

		dst[i] = byte_data | (~byte_mask & dst[i]);

		prev_data = data[i] >> (8 - shift_width);
		prev_mask = mask[i] >> (8 - shift_width);
	}

	return;
}

__attribute__((unused))
static void i40e_hmc_read(uint8_t *hmc_bits,
	struct i40e_hmc_ce *ce, uint8_t *host_buf)
{
	uint8_t data[8], mask[8];
	uint8_t *src, *dst;
	uint8_t byte_data, next_data;
	uint16_t shift_width, width_remain;
	int i;

	/* prepare the bits and mask */
	shift_width = ce->lsb % 8;

	/* get the current bits from the src bit string */
	src = hmc_bits + (ce->lsb / 8);

	for(i = 0; i < ce->size_of; i++){
		width_remain = ce->width - (i * 8);
		mask[i] = (1 << min(width_remain, (uint16_t)8)) - 1;
	}

	next_data = 0;
	for(i = ce->size_of - 1; i >= 0; i--){
		byte_data = src[i] >> shift_width;
		byte_data |= next_data;

		data[i] = byte_data & mask[i];

		next_data = src[i] << (8 - shift_width);
	}

	/* get the address from the struct field */
	dst = host_buf + ce->offset;

	for(i = 0; i < ce->size_of; i++){
#if __BYTE_ORDER == __LITTLE_ENDIAN
		dst[i] = data[i];
#elif __BYTE_ORDER == __BIG_ENDIAN
		dst[i] = data[ce->size_of - (i + 1)];
#endif
	}

	return;
}

