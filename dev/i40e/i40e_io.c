#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <lib_main.h>
#include <lib_io.h>

#include "i40e_main.h"
#include "i40e_aq.h"
#include "i40e_aqc.h"
#include "i40e_hmc.h"
#include "i40e_regs.h"
#include "i40e_io.h"

static int i40e_set_rsskey(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *key, uint16_t key_size);
static int i40e_set_rsslut(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *lut, uint16_t lut_size);
static void i40e_vsi_configure_irq_rx(struct ufp_dev *dev,
	struct ufp_iface *iface, uint16_t qp_idx, uint16_t irq_idx);
static void i40e_vsi_configure_irq_tx(struct ufp_dev *dev,
	struct ufp_iface *iface, uint16_t qp_idx, uint16_t irq_idx);
static void i40e_vsi_shutdown_irq_rx(struct ufp_dev *dev,
	uint16_t qp_idx, uint16_t irq_idx);
static void i40e_vsi_shutdown_irq_tx(struct ufp_dev *dev,
	uint16_t qp_idx, uint16_t irq_idx);
static void i40e_pre_tx_queue_enable(struct ufp_dev *dev, uint32_t qp_idx);
static void i40e_pre_tx_queue_disable(struct ufp_dev *dev, uint32_t qp_idx);

static int i40e_set_rsskey(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *key, uint16_t key_size)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_set_rsskey(dev, iface, key, key_size, session);
	err = i40e_aqc_wait_cmd(dev, session);
	if(err < 0)
		goto err_wait_cmd;

	if(session->retval != I40E_AQ_RC_OK
	&& session->retval != I40E_AQ_RC_ESRCH)
		goto err_retval;

	if(session->retval == I40E_AQ_RC_ESRCH){
		/* XXX: set rsskey by register */
	}

	i40e_aq_session_delete(session);
	return 0;

err_retval:
err_wait_cmd:
	i40e_aq_session_delete(session);
err_alloc_session:
	return -1;
}

static int i40e_set_rsslut(struct ufp_dev *dev,
	struct ufp_iface *iface, uint8_t *lut, uint16_t lut_size)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_set_rsslut(dev, iface, lut, lut_size, session);
	err = i40e_aqc_wait_cmd(dev, session);
	if(err < 0)
		goto err_wait_cmd;

	if(session->retval != I40E_AQ_RC_OK
	&& session->retval != I40E_AQ_RC_ESRCH)
		goto err_retval;

	if(session->retval == I40E_AQ_RC_ESRCH){
		/* XXX: set rsslut by register */
	}

	i40e_aq_session_delete(session);
	return 0;

err_retval:
err_wait_cmd:
	i40e_aq_session_delete(session);
err_alloc_session:
	return -1;
}

int i40e_vsi_update(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_aq_session *session;
	struct i40e_aq_buf_vsi_data data = {0};
	struct i40e_iface *i40e_iface = iface->drv_data;
	uint16_t tc_qps_offset;
	int i, err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	/* Turn off vlan stripping for the VSI */
	data.valid_sections = htole16(I40E_AQ_VSI_PROP_VLAN_VALID);
	data.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_ALL
		| I40E_AQ_VSI_PVLAN_EMOD_NOTHING;

	/* Setup VSI queue mapping */
	data.valid_sections |= htole16(I40E_AQ_VSI_PROP_QUEUE_MAP_VALID);
	data.mapping_flags = htole16(I40E_AQ_VSI_QUE_MAP_CONTIG);
	data.queue_mapping[0] = htole16(i40e_iface->base_qp);

	tc_qps_offset = 0;
	for(i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++){
		uint16_t tc_map;
		uint32_t num_qps_tc;
		uint8_t num_qps_tc_pow;

		/* XXX: Support multiple TCs */
		if(i == 0){
			num_qps_tc = iface->num_qps;
		}else{
			num_qps_tc = 0;
		}

		/* find the next higher power-of-2 of num_qps */
		num_qps_tc_pow = 0;
		while(BIT_ULL(num_qps_tc_pow) < num_qps_tc){
			num_qps_tc_pow++;
		}

		tc_map =
			(num_qps_tc ? tc_qps_offset : 0
				<< I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT) |
			(num_qps_tc_pow
				<< I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT);
		data.tc_mapping[i] = htole16(tc_map);

		tc_qps_offset += num_qps_tc;
	}

	i40e_aqc_req_update_vsi(dev, iface, &data, session);
	err = i40e_aqc_wait_cmd(dev, session);
	if(err < 0)
		goto err_wait_cmd;

	if(session->retval != I40E_AQ_RC_OK)
		goto err_retval;

	i40e_aq_session_delete(session);
	return 0;

err_retval:
err_wait_cmd:
	i40e_aq_session_delete(session);
err_alloc_session:
	return -1;
}

int i40e_vsi_get(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_get_vsi(dev, iface, session);
	err = i40e_aqc_wait_cmd(dev, session);
	if(err < 0)
		goto err_wait_cmd;

	if(session->retval != I40E_AQ_RC_OK)
		goto err_retval;

	i40e_aq_session_delete(session);
	return 0;

err_retval:
err_wait_cmd:
	i40e_aq_session_delete(session);
err_alloc_session:
	return -1;
}

int i40e_vsi_promisc_mode(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_aq_session *session;
	uint16_t promisc_flags;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_session_create;

	promisc_flags = I40E_AQC_SET_VSI_PROMISC_MULTICAST |
		I40E_AQC_SET_VSI_PROMISC_BROADCAST;

	if(iface->promisc){
		promisc_flags |= I40E_AQC_SET_VSI_PROMISC_UNICAST;
	}

	i40e_aqc_req_promisc_mode(dev, iface, promisc_flags, session);
	err = i40e_aqc_wait_cmd(dev, session);
	if(err < 0)
		goto err_wait_cmd;

	if(session->retval != I40E_AQ_RC_OK)
		goto err_retval;

	i40e_aq_session_delete(session);
	return 0;

err_retval:
err_wait_cmd:
	i40e_aq_session_delete(session);
err_session_create:
	return -1;
}

int i40e_vsi_rss_config(struct ufp_dev *dev, struct ufp_iface *iface)
{
	int i, err;

	/* Default seed is retrieved from DPDK i40e PMD */
	uint32_t seed[I40E_PFQF_HKEY_MAX_INDEX + 1] = {
		0x6b793944, 0x23504cb5, 0x5bea75b6, 0x309f4f12,
		0x3dc0a2b8, 0x024ddcdf, 0x339b8ca0, 0x4c4af64a,
		0x34fac605, 0x55d85839, 0x3a58997d, 0x2ec938e1,
		0x66031581
	};
	uint32_t lut[I40E_PFQF_HLUT_MAX_INDEX + 1];

	err = i40e_set_rsskey(dev, iface, (uint8_t *)seed, sizeof(seed));
	if(err < 0)
		goto err_set_rsskey;

	/* LUT (Look Up Table) configuration */
	for (i = 0; i < sizeof(lut); i++) {
		((uint8_t *)lut)[i] = i % iface->num_qps;
	}

	err = i40e_set_rsslut(dev, iface, (uint8_t *)lut, sizeof(lut));
	if(err < 0)
		goto err_set_rsslut;

	return 0;

err_set_rsslut:
err_set_rsskey:
	return -1;
}

int i40e_vsi_configure_tx(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_iface *i40e_iface = iface->drv_data;
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
		ctx.tphrdesc_en = 1;
		ctx.tphwdesc_en = 1;
		ctx.tphrpkt_en = 1;

		/*
		 * This flag selects between Head WB and
		 * transmit descriptor WB:
		 * 0b - Descriptor Write Back
		 * 1b - Head Write Back
		 */
		ctx.head_wb_en = 0;

		/*
		 * See 1.1.4 - Transmit scheduler
		 * The XL710 provides management interfaces that allow each
		 * LAN transmit queue to be placed into a queue set.
		 * A queue set is a list of transmit queues that belong to
		 * the same TC and are treated equally
		 * by the XL710 transmit scheduler.
		 */
		ctx.rdylist = le16toh(i40e_iface->qs_handles[0]);
		ctx.rdylist_act = 0;

		/* set the context in the HMC */
		err = i40e_hmc_set_ctx_tx(dev, &ctx, qp_idx);
		if(err < 0)
			goto err_set_ctx;

		/* Now associate this queue with this PCI function */
		qtx_ctl = I40E_QTX_CTL_PF_QUEUE;
		qtx_ctl |= (
			(i40e_dev->pf_id << I40E_QTX_CTL_PF_INDX_SHIFT)
			& I40E_QTX_CTL_PF_INDX_MASK);
		UFP_WRITE32(dev, I40E_QTX_CTL(qp_idx), qtx_ctl);
		i40e_flush(dev);

		/* cache tail off for easier writes later */
		ring->tail = dev->bar + I40E_QTX_TAIL(qp_idx);
	}

	return 0;

err_set_ctx:
	return -1;
}

int i40e_vsi_configure_rx(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	struct ufp_ring *ring;
	struct i40e_hmc_ctx_rx ctx;
	uint16_t qp_idx;
	int i, err;

	for (i = 0; i < iface->num_qps; i++){
		qp_idx = i40e_iface->base_qp + i;
		ring = &iface->rx_ring[i];

		/* clear the context structure first */
		memset(&ctx, 0, sizeof(struct i40e_hmc_ctx_rx));

		/*
		 * See 8.3.3.2.2 - Receive Queue Context in FPM
		 */
		if((iface->buf_size % BIT(I40E_RXQ_CTX_DBUFF_SHIFT))
		|| iface->buf_size < I40E_MIN_RX_BUFFER
		|| iface->buf_size > I40E_MAX_RX_BUFFER)
			goto err_buf_size;
		ctx.dbuff = iface->buf_size >> I40E_RXQ_CTX_DBUFF_SHIFT;

		ctx.base = (ring->addr_dma / 128);
		ctx.qlen = iface->num_rx_desc;
		/* use 16 byte descriptors */
		ctx.dsize = 0;
		/* descriptor type is always zero */
		ctx.dtype = 0;
		ctx.hsplit_0 = 0;

		if(iface->mtu_frame > min((uint32_t)I40E_MAX_MTU,
			I40E_MAX_CHAINED_RX_BUFFERS * iface->buf_size))
			goto err_mtu_size;
		ctx.rxmax = iface->mtu_frame;

		/* XXX: Does it work? Is ctx.cpuid set by hardware correctly
		 * when socket id is not equeal 0?
		 */
		ctx.tphrdesc_en = 1;
		ctx.tphwdesc_en = 1;
		ctx.tphdata_en = 1;
		ctx.tphhead_en = 0;

		ctx.lrxqthresh = 2;
		ctx.crcstrip = 1;
		ctx.l2tsel = 1;
		/*
		 * this controls whether VLAN is
		 * stripped from inner headers
		 */
		ctx.showiv = 0;
		/*
		 * set the prefena field to 1
		 * because the manual says to
		 */
		ctx.pref_en = 1;

		/* set the context in the HMC */
		err = i40e_hmc_set_ctx_rx(dev, &ctx, qp_idx);
		if(err < 0)
			goto err_set_ctx;

		/*
		 * cache tail for quicker writes,
		 * and clear the reg before use
		 */
		ring->tail = dev->bar + I40E_QRX_TAIL(qp_idx);
		ufp_writel(0, ring->tail);
	}

	return 0;

err_set_ctx:
err_mtu_size:
err_buf_size:
	return -1;
}

static void i40e_vsi_configure_irq_rx(struct ufp_dev *dev,
	struct ufp_iface *iface, uint16_t qp_idx, uint16_t irq_idx)
{
	uint32_t val;

	/*
	 * In Intel x710 document,
	 * "ITR" term just means "Interrupt Throttling".
	 * x710 supports 3 ITR values per IRQ, but we use only ITR0
	 * because we use 1 IRQ per queue
	 * (not queue-pair like vanilla driver).
	 */
	UFP_WRITE32(dev, I40E_PFINT_ITRN(I40E_IDX_ITR0, irq_idx - 1),
		ITR_TO_REG(I40E_ITR_20K));

	UFP_WRITE32(dev, I40E_PFINT_RATEN(irq_idx - 1),
		INTRL_USEC_TO_REG(iface->irq_rate));

	/* Linked list for the queuepairs assigned to this IRQ */
	val =	qp_idx << I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT
		| I40E_QUEUE_TYPE_RX
			<< I40E_PFINT_LNKLSTN_FIRSTQ_TYPE_SHIFT;
	UFP_WRITE32(dev, I40E_PFINT_LNKLSTN(irq_idx - 1), val);

	val =	I40E_QINT_RQCTL_CAUSE_ENA_MASK
		| (I40E_IDX_ITR0 << I40E_QINT_RQCTL_ITR_INDX_SHIFT)
		| (irq_idx << I40E_QINT_RQCTL_MSIX_INDX_SHIFT)
		| (I40E_QUEUE_END_OF_LIST
			<< I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT);
	UFP_WRITE32(dev, I40E_QINT_RQCTL(qp_idx), val);
	return;
}

static void i40e_vsi_configure_irq_tx(struct ufp_dev *dev,
	struct ufp_iface *iface, uint16_t qp_idx, uint16_t irq_idx)
{
	uint32_t val;

	UFP_WRITE32(dev, I40E_PFINT_ITRN(I40E_IDX_ITR0, irq_idx - 1),
		ITR_TO_REG(I40E_ITR_20K));

	UFP_WRITE32(dev, I40E_PFINT_RATEN(irq_idx - 1),
		INTRL_USEC_TO_REG(iface->irq_rate));

	val =	qp_idx << I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT
		| I40E_QUEUE_TYPE_TX
			<< I40E_PFINT_LNKLSTN_FIRSTQ_TYPE_SHIFT;
	UFP_WRITE32(dev, I40E_PFINT_LNKLSTN(irq_idx - 1), val);

	val =	I40E_QINT_TQCTL_CAUSE_ENA_MASK
		| (I40E_IDX_ITR0 << I40E_QINT_TQCTL_ITR_INDX_SHIFT)
		| (irq_idx << I40E_QINT_TQCTL_MSIX_INDX_SHIFT)
		| (I40E_QUEUE_END_OF_LIST
			<< I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT);
	UFP_WRITE32(dev, I40E_QINT_TQCTL(qp_idx), val);
	return;
}

static void i40e_vsi_shutdown_irq_rx(struct ufp_dev *dev,
	uint16_t qp_idx, uint16_t irq_idx)
{
	uint32_t val;

	val = UFP_READ32(dev, I40E_PFINT_LNKLSTN(irq_idx - 1));
	val |= I40E_QUEUE_END_OF_LIST
		<< I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT;
	UFP_WRITE32(dev, I40E_PFINT_LNKLSTN(irq_idx - 1), val);

	val = UFP_READ32(dev, I40E_QINT_RQCTL(qp_idx));

	val &= ~(I40E_QINT_RQCTL_MSIX_INDX_MASK
		| I40E_QINT_RQCTL_MSIX0_INDX_MASK
		| I40E_QINT_RQCTL_CAUSE_ENA_MASK
		| I40E_QINT_RQCTL_INTEVENT_MASK);

	val |= (I40E_QINT_RQCTL_ITR_INDX_MASK
		| I40E_QINT_RQCTL_NEXTQ_INDX_MASK);

	UFP_WRITE32(dev, I40E_QINT_RQCTL(qp_idx), val);
	return;
}

static void i40e_vsi_shutdown_irq_tx(struct ufp_dev *dev,
	uint16_t qp_idx, uint16_t irq_idx)
{
	uint32_t val;

	val = UFP_READ32(dev, I40E_PFINT_LNKLSTN(irq_idx - 1));
	val |= I40E_QUEUE_END_OF_LIST
		<< I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT;
	UFP_WRITE32(dev, I40E_PFINT_LNKLSTN(irq_idx - 1), val);

	val = UFP_READ32(dev, I40E_QINT_TQCTL(qp_idx));

	val &= ~(I40E_QINT_TQCTL_MSIX_INDX_MASK
		| I40E_QINT_TQCTL_MSIX0_INDX_MASK
		| I40E_QINT_TQCTL_CAUSE_ENA_MASK
		| I40E_QINT_TQCTL_INTEVENT_MASK);

	val |= (I40E_QINT_TQCTL_ITR_INDX_MASK
		| I40E_QINT_TQCTL_NEXTQ_INDX_MASK);

	UFP_WRITE32(dev, I40E_QINT_TQCTL(qp_idx), val);
	return;
}

void i40e_vsi_configure_irq(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	uint16_t qp_idx, irq_idx, rx_done = 0, tx_done = 0;
	int i;

	irq_idx = i40e_iface->base_qp * 2;
	irq_idx += I40E_NUM_MISC_IRQS;

	/*
	 * The interrupt indexing is offset by 1 in the PFINT_ITRn
	 * and PFINT_LNKLSTn registers.
	 * e.g. PFINT_ITRn[0..n-1] gets msix1..msixn (qpair interrupts)
	 */
	for (i = 0; i < iface->num_qps; i++, irq_idx++, rx_done++){
		qp_idx = i40e_iface->base_qp + i;
		i40e_vsi_configure_irq_rx(dev,
			iface, qp_idx, irq_idx);
	}

	for (i = 0; i < iface->num_qps; i++, irq_idx++, tx_done++){
		qp_idx = i40e_iface->base_qp + i;
		i40e_vsi_configure_irq_tx(dev,
			iface, qp_idx, irq_idx);
	}

	i40e_flush(dev);
	return;
}

void i40e_vsi_shutdown_irq(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	uint16_t qp_idx, irq_idx;
	int i;

	irq_idx = i40e_iface->base_qp * 2;
	irq_idx += I40E_NUM_MISC_IRQS;

	for (i = 0; i < iface->num_qps; i++, irq_idx++){
		qp_idx = i40e_iface->base_qp + i;
		i40e_vsi_shutdown_irq_rx(dev, qp_idx, irq_idx);
	}

	for(i = 0; i < iface->num_qps; i++, irq_idx++){
		qp_idx = i40e_iface->base_qp + i;
		i40e_vsi_shutdown_irq_tx(dev, qp_idx, irq_idx);
	}

	i40e_flush(dev);
	return;
}

void i40e_vsi_start_irq(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	uint16_t irq_idx;
	uint32_t val;
	int i;

	irq_idx = i40e_iface->base_qp * 2;
	irq_idx += I40E_NUM_MISC_IRQS;

	for (i = 0; i < iface->num_qps * 2; i++, irq_idx++){
		/* definitely clear the Pending Interrupt Array(PBA) here,
		 * as this function is meant to clean out all previous interrupts
		 * AND enable the interrupt
		 */
		val = I40E_PFINT_DYN_CTLN_INTENA_MASK |
			I40E_PFINT_DYN_CTLN_CLEARPBA_MASK |
			(I40E_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT);
		UFP_WRITE32(dev, I40E_PFINT_DYN_CTLN(irq_idx - 1), val);
	}

	i40e_flush(dev);
	return;
}

void i40e_vsi_stop_irq(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	uint16_t irq_idx;
	int i;

	irq_idx = i40e_iface->base_qp * 2;
	irq_idx += I40E_NUM_MISC_IRQS;

	for (i = 0; i < iface->num_qps * 2; i++, irq_idx++){
		UFP_WRITE32(dev, I40E_PFINT_DYN_CTLN(irq_idx - 1), 0);
	}

	i40e_flush(dev);
	return;
}

void i40e_update_enable_itr(void *bar, uint16_t entry_idx)
{
	uint32_t val;

	val = I40E_PFINT_DYN_CTLN_INTENA_MASK
		/* Don't clear PBA because that can cause lost interrupts that
		 * came in while we were cleaning/polling
		 */
		| (I40E_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT);
	ufp_writel(val, bar + I40E_PFINT_DYN_CTLN(entry_idx - 1));
}

int i40e_vsi_start_rx(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	uint32_t rx_reg;
	uint16_t qp_idx;
	int i, retry;

	for (i = 0; i < iface->num_qps; i++){
		qp_idx = i40e_iface->base_qp + i;

		for(retry = 0; retry < 50; retry++){
			rx_reg = UFP_READ32(dev, I40E_QRX_ENA(qp_idx));
			if (((rx_reg >> I40E_QRX_ENA_QENA_REQ_SHIFT) & 1) ==
			    ((rx_reg >> I40E_QRX_ENA_QENA_STAT_SHIFT) & 1))
				break;
			usleep(1000);
		}

		/* Skip if the queue is already in the requested state */
		if(rx_reg & I40E_QRX_ENA_QENA_STAT_MASK)
			continue;

		/* turn on the queue */
		rx_reg |= I40E_QRX_ENA_QENA_REQ_MASK;
		UFP_WRITE32(dev, I40E_QRX_ENA(qp_idx), rx_reg);

		/* wait for the change to finish */
		for(retry = 0; retry < I40E_QUEUE_WAIT_RETRY_LIMIT; retry++){
			rx_reg = UFP_READ32(dev, I40E_QRX_ENA(qp_idx));
			if (rx_reg & I40E_QRX_ENA_QENA_STAT_MASK)
				break;
			usleep(10);
		}
		if (retry >= I40E_QUEUE_WAIT_RETRY_LIMIT)
			goto err_vsi_start;
	}
	return 0;

err_vsi_start:
	return -1;
}

int i40e_vsi_stop_rx(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	uint32_t rx_reg;
	uint16_t qp_idx;
	int i, retry;

	for (i = 0; i < iface->num_qps; i++){
		qp_idx = i40e_iface->base_qp + i;

		for (retry = 0; retry < 50; retry++) {
			rx_reg = UFP_READ32(dev, I40E_QRX_ENA(qp_idx));
			if (((rx_reg >> I40E_QRX_ENA_QENA_REQ_SHIFT) & 1) ==
			    ((rx_reg >> I40E_QRX_ENA_QENA_STAT_SHIFT) & 1))
				break;
			usleep(1000);
		}

		/* Skip if the queue is already in the requested state */
		if(!(rx_reg & I40E_QRX_ENA_QENA_STAT_MASK))
			continue;

		/* turn off the queue */
		rx_reg &= ~I40E_QRX_ENA_QENA_REQ_MASK;
		UFP_WRITE32(dev, I40E_QRX_ENA(qp_idx), rx_reg);

		/* wait for the change to finish */
		for(retry = 0; retry < I40E_QUEUE_WAIT_RETRY_LIMIT; retry++){
			rx_reg = UFP_READ32(dev, I40E_QRX_ENA(qp_idx));
			if(!(rx_reg & I40E_QRX_ENA_QENA_STAT_MASK))
				break;
			usleep(10);
		}
		if(retry >= I40E_QUEUE_WAIT_RETRY_LIMIT)
			goto err_vsi_stop;
	}
	return 0;

err_vsi_stop:
	return -1;
}

int i40e_vsi_start_tx(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	uint32_t tx_reg;
	uint16_t qp_idx;
	int i, retry;

	for (i = 0; i < iface->num_qps; i++){
		qp_idx = i40e_iface->base_qp + i;

		/* warn the TX unit of coming changes */
		i40e_pre_tx_queue_enable(dev, qp_idx);

		for(retry = 0; retry < 50; retry++){
			tx_reg = UFP_READ32(dev, I40E_QTX_ENA(qp_idx));
			if (((tx_reg >> I40E_QTX_ENA_QENA_REQ_SHIFT) & 1) ==
			    ((tx_reg >> I40E_QTX_ENA_QENA_STAT_SHIFT) & 1))
				break;
			usleep(1000);
		}
		/* Skip if the queue is already in the requested state */
		if (tx_reg & I40E_QTX_ENA_QENA_STAT_MASK)
			continue;

		/* turn on the queue */
		UFP_WRITE32(dev, I40E_QTX_HEAD(qp_idx), 0);
		tx_reg |= I40E_QTX_ENA_QENA_REQ_MASK;
		UFP_WRITE32(dev, I40E_QTX_ENA(qp_idx), tx_reg);

		/* wait for the change to finish */
		for(retry = 0; retry < I40E_QUEUE_WAIT_RETRY_LIMIT; retry++){
			tx_reg = UFP_READ32(dev, I40E_QTX_ENA(qp_idx));
			if (tx_reg & I40E_QTX_ENA_QENA_STAT_MASK)
				break;
			usleep(10);
		}
		if(retry >= I40E_QUEUE_WAIT_RETRY_LIMIT)
			goto err_vsi_start;
	}

	return 0;

err_vsi_start:
	return -1;
}

int i40e_vsi_stop_tx(struct ufp_dev *dev, struct ufp_iface *iface)
{
	struct i40e_iface *i40e_iface = iface->drv_data;
	uint32_t tx_reg;
	uint16_t qp_idx;
	int i, retry;

	for (i = 0; i < iface->num_qps; i++){
		qp_idx = i40e_iface->base_qp + i;

		/* warn the TX unit of coming changes */
		i40e_pre_tx_queue_disable(dev, qp_idx);
		usleep(10);

		for(retry = 0; retry < 50; retry++){
			tx_reg = UFP_READ32(dev, I40E_QTX_ENA(qp_idx));
			if (((tx_reg >> I40E_QTX_ENA_QENA_REQ_SHIFT) & 1) ==
			    ((tx_reg >> I40E_QTX_ENA_QENA_STAT_SHIFT) & 1))
				break;
			usleep(1000);
		}
		/* Skip if the queue is already in the requested state */
		if (!(tx_reg & I40E_QTX_ENA_QENA_STAT_MASK))
			continue;

		/* turn off the queue */
		tx_reg &= ~I40E_QTX_ENA_QENA_REQ_MASK;
		UFP_WRITE32(dev, I40E_QTX_ENA(qp_idx), tx_reg);

		/* wait for the change to finish */
		for (retry = 0; retry < I40E_QUEUE_WAIT_RETRY_LIMIT; retry++) {
			tx_reg = UFP_READ32(dev, I40E_QTX_ENA(qp_idx));
			if (!(tx_reg & I40E_QTX_ENA_QENA_STAT_MASK))
				break;
			usleep(10);
		}
		if (retry >= I40E_QUEUE_WAIT_RETRY_LIMIT)
			goto err_vsi_stop;
	}

	return 0;

err_vsi_stop:
	return -1;
}

static void i40e_pre_tx_queue_enable(struct ufp_dev *dev, uint32_t qp_idx)
{
	uint32_t first_queue;
	uint32_t abs_queue_idx;
	uint32_t reg_block;
	uint32_t reg_val;

	reg_val = UFP_READ32(dev, I40E_PFLAN_QALLOC);
	first_queue = (reg_val & I40E_PFLAN_QALLOC_FIRSTQ_MASK) >>
		I40E_PFLAN_QALLOC_FIRSTQ_SHIFT;
	abs_queue_idx = first_queue + qp_idx;

	reg_block	= abs_queue_idx / 128;
	abs_queue_idx	= abs_queue_idx % 128;

	reg_val = UFP_READ32(dev, I40E_GLLAN_TXPRE_QDIS(reg_block));
	reg_val &= ~I40E_GLLAN_TXPRE_QDIS_QINDX_MASK;
	reg_val |= (abs_queue_idx << I40E_GLLAN_TXPRE_QDIS_QINDX_SHIFT);
	reg_val |= I40E_GLLAN_TXPRE_QDIS_CLEAR_QDIS_MASK;
	UFP_WRITE32(dev, I40E_GLLAN_TXPRE_QDIS(reg_block), reg_val);

	return;
}

static void i40e_pre_tx_queue_disable(struct ufp_dev *dev, uint32_t qp_idx)
{
	uint32_t first_queue;
	uint32_t abs_queue_idx;
	uint32_t reg_block;
	uint32_t reg_val;

	reg_val = UFP_READ32(dev, I40E_PFLAN_QALLOC);
	first_queue = (reg_val & I40E_PFLAN_QALLOC_FIRSTQ_MASK) >>
		I40E_PFLAN_QALLOC_FIRSTQ_SHIFT;
	abs_queue_idx = first_queue + qp_idx;

	reg_block       = abs_queue_idx / 128;
	abs_queue_idx   = abs_queue_idx % 128;

	reg_val = UFP_READ32(dev, I40E_GLLAN_TXPRE_QDIS(reg_block));
	reg_val &= ~I40E_GLLAN_TXPRE_QDIS_QINDX_MASK;
	reg_val |= (abs_queue_idx << I40E_GLLAN_TXPRE_QDIS_QINDX_SHIFT);
	reg_val |= I40E_GLLAN_TXPRE_QDIS_SET_QDIS_MASK;
	UFP_WRITE32(dev, I40E_GLLAN_TXPRE_QDIS(reg_block), reg_val);

	return;
}


int i40e_rx_desc_fetch(struct ufp_ring *rx_ring, uint16_t index,
	struct ufp_packet *packet)
{
	struct i40e_rx_desc *rx_desc;
	struct i40e_rx_desc_wb *rx_desc_wb;
	uint64_t qword1;

	rx_desc = I40E_RX_DESC(rx_ring, index);
	rx_desc_wb = (struct i40e_rx_desc_wb *)rx_desc;

	qword1 = le64toh(rx_desc_wb->qword1.status_error_len);

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
		packet->flag |= UFP_PACKET_EOF;

	if(unlikely(qword1 & I40E_RXD_QW1_ERROR_MASK))
		packet->flag |= UFP_PACKET_ERROR;

	return 0;

not_received:
	return -1;
}

int i40e_tx_desc_fetch(struct ufp_ring *tx_ring, uint16_t index)
{
	struct i40e_tx_desc *tx_desc;
	uint64_t qword1;

	tx_desc = I40E_TX_DESC(tx_ring, index);
	qword1 = le64toh(tx_desc->cmd_type_offset_bsz);

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
	struct i40e_rx_desc *rx_desc;
	struct i40e_rx_desc_wb *rx_desc_wb;

	rx_desc = I40E_RX_DESC(rx_ring, index);
	rx_desc_wb = (struct i40e_rx_desc_wb *)rx_desc;

	rx_desc->pkt_addr = htole64(addr_dma);
	rx_desc->hdr_addr = 0;

	/* clear the status bits for the next_to_use descriptor */
	rx_desc_wb->qword1.status_error_len = 0;

	return;
}

void i40e_tx_desc_fill(struct ufp_ring *tx_ring, uint16_t index,
	uint64_t addr_dma, struct ufp_packet *packet)
{
	struct i40e_tx_desc *tx_desc;
	uint32_t tx_cmd = 0;
	uint32_t tx_offset = 0;
	uint32_t tx_tag = 0;

	tx_desc = I40E_TX_DESC(tx_ring, index);

	tx_cmd |= I40E_TX_DESC_CMD_ICRC | I40E_TX_DESC_CMD_RS;
	if(likely(packet->flag & UFP_PACKET_EOF)){
		tx_cmd |= I40E_TX_DESC_CMD_EOP;
	}

	/* XXX: The size limit for a transmit buffer in a descriptor is (16K - 1).
	 * In order to align with the read requests we will align the value to
	 * the nearest 4K which represents our maximum read request size.
	 */
	tx_desc->buffer_addr = htole64(addr_dma);
	tx_desc->cmd_type_offset_bsz = htole64(I40E_TX_DESC_DTYPE_DATA |
		((uint64_t)tx_cmd << I40E_TXD_QW1_CMD_SHIFT) |
		((uint64_t)tx_offset << I40E_TXD_QW1_OFFSET_SHIFT) |
		((uint64_t)packet->slot_size << I40E_TXD_QW1_TX_BUF_SZ_SHIFT) |
		((uint64_t)tx_tag << I40E_TXD_QW1_L2TAG1_SHIFT));

	return;
}
