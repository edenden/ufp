#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>

#include <lib_main.h>
#include <lib_vfio.h>

#include "i40e_main.h"
#include "i40e_regs.h"
#include "i40e_ops.h"
#include "i40e_aq.h"
#include "i40e_aqc.h"
#include "i40e_hmc.h"
#include "i40e_io.h"

static int i40e_rxctl_write(struct ufp_dev *dev,
	uint32_t reg_addr, uint32_t reg_val);
static int i40e_rxctl_read(struct ufp_dev *dev,
	uint32_t reg_addr, uint32_t *reg_val);
static int i40e_firmware_version(struct ufp_dev *dev);
static int i40e_driver_version(struct ufp_dev *dev);
static int i40e_clear_pxemode(struct ufp_dev *dev);
static int i40e_stop_lldp(struct ufp_dev *dev);
static int i40e_macaddr_read(struct ufp_dev *dev);
static int i40e_set_phyintmask(struct ufp_dev *dev,
	uint16_t mask);
static int i40e_set_swconf(struct ufp_dev *dev,
	uint16_t flags, uint16_t valid_flags);
static void i40e_clear_swconf(struct ufp_dev *dev);
static int i40e_get_swconf(struct ufp_dev *dev);
static int i40e_reset_hw(struct ufp_dev *dev);
static void i40e_clear_hw(struct ufp_dev *dev);
static int i40e_configure_pf(struct ufp_dev *dev);
static void i40e_set_pf_id(struct ufp_dev *dev);
static int i40e_configure_filter(struct ufp_dev *dev);
static int i40e_configure_rss(struct ufp_dev *dev);
static int i40e_setup_pf_switch(struct ufp_dev *dev);

int i40e_open(struct ufp_dev *dev)
{
	int err;

	i40e_set_pf_id(dev);
	i40e_clear_hw(dev);

	err = i40e_reset_hw(dev);
	if(err < 0)
		goto err_reset_hw;

	err = i40e_aq_init(dev);
	if(err < 0)
		goto err_init_aq;

	err = i40e_configure_pf(dev);
	if(err)
		goto err_configure_pf;

	err = i40e_setup_pf_switch(dev);
	if (err)
		goto err_setup_pf_switch;

	err = i40e_hmc_init(dev);
	if(err < 0)
		goto err_hmc_init;

	return 0;

err_hmc_init:
err_setup_pf_switch:
err_configure_pf:
	i40e_aq_destroy(dev);
err_init_aq:
err_reset_hw:
	return -1;
}

void i40e_close(struct ufp_dev *dev)
{
	i40e_hmc_destroy(dev);
	i40e_clear_swconf(dev);
	i40e_aq_destroy(dev);

	return;
}

int i40e_up(struct ufp_dev *dev, struct ufp_iface *iface)
{
	int err;

	err = i40e_vsi_update(dev, iface);
	if(err < 0)
		goto err_vsi_update;

	err = i40e_vsi_get(dev, iface);
	if(err < 0)
		goto err_vsi_get;

	err = i40e_vsi_rss_config(dev, iface);
	if(err < 0)
		goto err_vsi_rss_config;

	err = i40e_vsi_promisc_mode(dev, iface);
	if(err < 0)
		goto err_configure_filter;

	err = i40e_vsi_configure_rx(dev, iface);
	if(err < 0)
		goto err_configure_rx;

	err = i40e_vsi_configure_tx(dev, iface);
	if(err < 0)
		goto err_configure_tx;

	i40e_vsi_configure_irq(dev, iface);

	err = i40e_vsi_start_rx(dev, iface);
	if(err < 0)
		goto err_start_rx;

	err = i40e_vsi_start_tx(dev, iface);
	if(err < 0)
		goto err_start_tx;

	i40e_vsi_start_irq(dev, iface);

	return 0;

err_start_tx:
	i40e_vsi_stop_rx(dev, iface);
err_start_rx:
	i40e_vsi_shutdown_irq(dev, iface);
err_configure_tx:
err_configure_rx:
err_configure_filter:
err_vsi_rss_config:
err_vsi_get:
err_vsi_update:
	return -1;
}

int i40e_down(struct ufp_dev *dev, struct ufp_iface *iface)
{
	int err;

	i40e_vsi_stop_irq(dev, iface);

	err = i40e_vsi_stop_tx(dev, iface);
	if(err < 0)
		goto err_stop_tx;

	err = i40e_vsi_stop_rx(dev, iface);
	if(err < 0)
		goto err_stop_rx;

	i40e_vsi_shutdown_irq(dev, iface);
	return 0;

err_stop_rx:
err_stop_tx:
	return -1;
}

static int i40e_rxctl_write(struct ufp_dev *dev,
	uint32_t reg_addr, uint32_t reg_val)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_rxctl_write(dev, reg_addr, reg_val, session);
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

static int i40e_rxctl_read(struct ufp_dev *dev,
	uint32_t reg_addr, uint32_t *reg_val)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_rxctl_read(dev, reg_addr, session);
	err = i40e_aqc_wait_cmd(dev, session);
	if(err < 0)
		goto err_wait_cmd;

	if(session->retval != I40E_AQ_RC_OK)
		goto err_retval;

	*reg_val = session->data.read_val;
	i40e_aq_session_delete(session);
	return 0;

err_retval:
err_wait_cmd:
	i40e_aq_session_delete(session);
err_alloc_session:
	return -1;
}

static int i40e_firmware_version(struct ufp_dev *dev)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_firmware_version(dev, session);
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

static int i40e_driver_version(struct ufp_dev *dev)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_driver_version(dev, session);
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

static int i40e_clear_pxemode(struct ufp_dev *dev)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_clear_pxemode(dev, session);
	err = i40e_aqc_wait_cmd(dev, session);
	if(err < 0)
		goto err_wait_cmd;

	if(session->retval != I40E_AQ_RC_OK
	&& session->retval != I40E_AQ_RC_EEXIST)
		goto err_retval;

	i40e_aq_session_delete(session);
	return 0;

err_retval:
err_wait_cmd:
	i40e_aq_session_delete(session);
err_alloc_session:
	return -1;
}

static int i40e_stop_lldp(struct ufp_dev *dev)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_stop_lldp(dev, session);
	err = i40e_aqc_wait_cmd(dev, session);
	if(err < 0)
		goto err_wait_cmd;

	if(session->retval != I40E_AQ_RC_OK
	&& session->retval != I40E_AQ_RC_EPERM)
		goto err_retval;

	i40e_aq_session_delete(session);
	return 0;

err_retval:
err_wait_cmd:
	i40e_aq_session_delete(session);
err_alloc_session:
	return -1;
}

static int i40e_macaddr_read(struct ufp_dev *dev)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_macaddr_read(dev, session);
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

static int i40e_set_phyintmask(struct ufp_dev *dev,
	uint16_t mask)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_set_phyintmask(dev, mask, session);
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

static int i40e_set_swconf(struct ufp_dev *dev,
	uint16_t flags, uint16_t valid_flags)
{
	struct i40e_aq_session *session;
	int err;

	session = i40e_aq_session_create(dev);
	if(!session)
		goto err_alloc_session;

	i40e_aqc_req_set_swconf(dev, flags, valid_flags, session);
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

static void i40e_clear_swconf(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct i40e_elem *elem, *temp;

	list_for_each_safe(&i40e_dev->elem, elem, list, temp){
		list_del(&elem->list);
		free(elem);
	}

	return;
}

static int i40e_get_swconf(struct ufp_dev *dev)
{
	struct i40e_aq_session *session;
	uint16_t seid_offset = 0;
	int err;

	i40e_clear_swconf(dev);
	do{
		session = i40e_aq_session_create(dev);
		if(!session)
			goto err_session_create;

		session->data.seid_offset = seid_offset;
		i40e_aqc_req_get_swconf(dev, session);
		err = i40e_aqc_wait_cmd(dev, session);
		if(err < 0)
			goto err_wait_cmd;

		if(session->retval != I40E_AQ_RC_OK)
			goto err_retval;

		seid_offset = session->data.seid_offset;
		i40e_aq_session_delete(session);
		continue;

err_retval:
err_wait_cmd:
		i40e_aq_session_delete(session);
err_session_create:
		goto err_fetch;
	}while(seid_offset);

	return 0;

err_fetch:
	i40e_clear_swconf(dev);
	return -1;
}

static int i40e_reset_hw(struct ufp_dev *dev)
{
	uint32_t grst_del, reg;
	unsigned int timeout;

	/* Poll for Global Reset steady state in case of recent GRST.
	 * The grst delay value is in 100ms units, and we'll wait a
	 * couple counts longer to be sure we don't just miss the end.
	 */
	grst_del = (UFP_READ32(dev, I40E_GLGEN_RSTCTL) &
		I40E_GLGEN_RSTCTL_GRSTDEL_MASK) >> I40E_GLGEN_RSTCTL_GRSTDEL_SHIFT;
	timeout = grst_del * 20;
	do{
		msleep(100);
		reg = UFP_READ32(dev, I40E_GLGEN_RSTAT);
		if(!(reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK))
			break;
	}while(--timeout);
	if(!timeout)
		goto err_grst_in_progress;

	/* Now Wait for the FW to be ready */
	timeout = I40E_PF_RESET_WAIT_COUNT;
	do{
		usleep(10000);
		reg = UFP_READ32(dev, I40E_GLNVM_ULD);
		if((reg & I40E_GLNVM_ULD_CONF_CORE_DONE_MASK) &&
		(reg & I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK))
			break;
	}while(--timeout);
	if(!timeout)
		goto err_firmware_not_ready;

	reg = UFP_READ32(dev, I40E_PFGEN_CTRL);
	reg |= I40E_PFGEN_CTRL_PFSWR_MASK;
	UFP_WRITE32(dev, I40E_PFGEN_CTRL, reg);

	timeout = I40E_PF_RESET_WAIT_COUNT;
	do{
		usleep(1000);
		reg = UFP_READ32(dev, I40E_PFGEN_CTRL);
		if(!(reg & I40E_PFGEN_CTRL_PFSWR_MASK))
			break;
	}while(--timeout);
	if(!timeout)
		goto err_pf_reset;

	return 0;

err_pf_reset:
err_firmware_not_ready:
err_grst_in_progress:
	return -1;
}

static void i40e_clear_hw(struct ufp_dev *dev)
{
	uint32_t num_queues, base_queue;
	uint32_t num_pf_int;
	uint32_t num_vf_int;
	uint32_t num_vfs;
	uint32_t i, j;
	uint32_t val;
	uint32_t eol = 0x7ff;

	/* get number of interrupts, queues, and vfs */
	val = UFP_READ32(dev, I40E_GLPCI_CNF2);
	num_pf_int = (val & I40E_GLPCI_CNF2_MSI_X_PF_N_MASK) >>
			I40E_GLPCI_CNF2_MSI_X_PF_N_SHIFT;
	num_vf_int = (val & I40E_GLPCI_CNF2_MSI_X_VF_N_MASK) >>
			I40E_GLPCI_CNF2_MSI_X_VF_N_SHIFT;

	val = UFP_READ32(dev, I40E_PFLAN_QALLOC);
	base_queue = (val & I40E_PFLAN_QALLOC_FIRSTQ_MASK) >>
			I40E_PFLAN_QALLOC_FIRSTQ_SHIFT;
	j = (val & I40E_PFLAN_QALLOC_LASTQ_MASK) >>
			I40E_PFLAN_QALLOC_LASTQ_SHIFT;
	if (val & I40E_PFLAN_QALLOC_VALID_MASK)
		num_queues = (j - base_queue) + 1;
	else
		num_queues = 0;

	val = UFP_READ32(dev, I40E_PF_VT_PFALLOC);
	i = (val & I40E_PF_VT_PFALLOC_FIRSTVF_MASK) >>
			I40E_PF_VT_PFALLOC_FIRSTVF_SHIFT;
	j = (val & I40E_PF_VT_PFALLOC_LASTVF_MASK) >>
			I40E_PF_VT_PFALLOC_LASTVF_SHIFT;
	if (val & I40E_PF_VT_PFALLOC_VALID_MASK)
		num_vfs = (j - i) + 1;
	else
		num_vfs = 0;

	/* stop all the interrupts */
	UFP_WRITE32(dev, I40E_PFINT_ICR0_ENA, 0);
	val = 0x3 << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT;
	for (i = 0; i < num_pf_int - 2; i++)
		UFP_WRITE32(dev, I40E_PFINT_DYN_CTLN(i), val);

	/* Set the FIRSTQ_INDX field to 0x7FF in PFINT_LNKLSTx */
	val = eol << I40E_PFINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	UFP_WRITE32(dev, I40E_PFINT_LNKLST0, val);
	for (i = 0; i < num_pf_int - 2; i++)
		UFP_WRITE32(dev, I40E_PFINT_LNKLSTN(i), val);
	val = eol << I40E_VPINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	for (i = 0; i < num_vfs; i++)
		UFP_WRITE32(dev, I40E_VPINT_LNKLST0(i), val);
	for (i = 0; i < num_vf_int - 2; i++)
		UFP_WRITE32(dev, I40E_VPINT_LNKLSTN(i), val);

	/* warn the HW of the coming Tx disables */
	for (i = 0; i < num_queues; i++) {
		uint32_t abs_queue_idx = base_queue + i;
		uint32_t reg_block = 0;

		if (abs_queue_idx >= 128) {
			reg_block = abs_queue_idx / 128;
			abs_queue_idx %= 128;
		}

		val = UFP_READ32(dev, I40E_GLLAN_TXPRE_QDIS(reg_block));
		val &= ~I40E_GLLAN_TXPRE_QDIS_QINDX_MASK;
		val |= (abs_queue_idx << I40E_GLLAN_TXPRE_QDIS_QINDX_SHIFT);
		val |= I40E_GLLAN_TXPRE_QDIS_SET_QDIS_MASK;

		UFP_WRITE32(dev, I40E_GLLAN_TXPRE_QDIS(reg_block), val);
	}
	usleep(400);

	/* stop all the queues */
	for (i = 0; i < num_queues; i++) {
		UFP_WRITE32(dev, I40E_QINT_TQCTL(i), 0);
		UFP_WRITE32(dev, I40E_QTX_ENA(i), 0);
		UFP_WRITE32(dev, I40E_QINT_RQCTL(i), 0);
		UFP_WRITE32(dev, I40E_QRX_ENA(i), 0);
	}

	/* short wait for all queue disables to settle */
	usleep(50);

	return;
}

static int i40e_configure_pf(struct ufp_dev *dev)
{
	int err;

	err = i40e_firmware_version(dev);
	if(err < 0)
		goto err_firmware_version;

	err = i40e_driver_version(dev);
	if(err < 0)
		goto err_driver_version;

	err = i40e_clear_pxemode(dev);
	if(err < 0)
		goto err_clear_pxemode;

	/* Disable LLDP for NICs that have firmware versions lower than v4.3.
	 * Ignore error return codes because if it was already disabled via
	 * hardware settings this will fail
	 */
	err = i40e_stop_lldp(dev);
	if(err < 0)
		goto err_stop_lldp;

	err = i40e_macaddr_read(dev);
	if(err < 0)
		goto err_macaddr_read;

	/* The driver only wants link up/down and module qualification
	 * reports from firmware.  Note the negative logic.
	 */
	err = i40e_set_phyintmask(dev,
		~(I40E_AQ_EVENT_LINK_UPDOWN |
		I40E_AQ_EVENT_MEDIA_NA |
		I40E_AQ_EVENT_MODULE_QUAL_FAIL));
	if(err < 0)
		goto err_set_phyintmask;

	return 0;

err_set_phyintmask:
err_macaddr_read:
err_stop_lldp:
err_clear_pxemode:
err_driver_version:
err_firmware_version:
	return -1;
}

void i40e_setup_misc_irq(struct ufp_dev *dev)
{
	uint32_t val;

	/* clear things first */
	UFP_WRITE32(dev, I40E_PFINT_ICR0_ENA, 0);  /* disable all */
	UFP_READ32(dev, I40E_PFINT_ICR0);	 /* read to clear */

	val =	I40E_PFINT_ICR0_ENA_ECC_ERR_MASK |
		I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK |
		I40E_PFINT_ICR0_ENA_GRST_MASK |
		I40E_PFINT_ICR0_ENA_PCI_EXCEPTION_MASK |
		I40E_PFINT_ICR0_ENA_GPIO_MASK |
		I40E_PFINT_ICR0_ENA_HMC_ERR_MASK |
		I40E_PFINT_ICR0_ENA_VFLR_MASK |
		I40E_PFINT_ICR0_ENA_ADMINQ_MASK;
	UFP_WRITE32(dev, I40E_PFINT_ICR0_ENA, val);

	/* OTHER_ITR_IDX = 0 */
	UFP_WRITE32(dev, I40E_PFINT_STAT_CTL0, 0);

	/* associate no queues to the misc IRQ */
	UFP_WRITE32(dev, I40E_PFINT_LNKLST0, I40E_QUEUE_END_OF_LIST);
	UFP_WRITE32(dev, I40E_PFINT_ITR0(I40E_IDX_ITR0), I40E_ITR_8K);

	i40e_flush(dev);
	return;
}

void i40e_shutdown_misc_irq(struct ufp_dev *dev)
{
	/* Disable ICR 0 */
	UFP_WRITE32(dev, I40E_PFINT_ICR0_ENA, 0);

	i40e_flush(dev);
	return;
}

void i40e_start_misc_irq(struct ufp_dev *dev)
{
	uint32_t val;

	/* originally in i40e_irq_dynamic_enable_icr0() */
	val = I40E_PFINT_DYN_CTL0_INTENA_MASK |
		I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
		(I40E_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT);

	UFP_WRITE32(dev, I40E_PFINT_DYN_CTL0, val);

	i40e_flush(dev);
	return;
}

void i40e_stop_misc_irq(struct ufp_dev *dev)
{
	UFP_WRITE32(dev, I40E_PFINT_DYN_CTL0,
		I40E_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT);

	i40e_flush(dev);
	return;
}

static void i40e_set_pf_id(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	uint32_t pci_cap, pci_cap_ari, func_rid;

	pci_cap = UFP_READ32(dev, I40E_GLPCI_CAPSUP);
	pci_cap_ari = (pci_cap & I40E_GLPCI_CAPSUP_ARI_EN_MASK) >>
		I40E_GLPCI_CAPSUP_ARI_EN_SHIFT;
	func_rid = UFP_READ32(dev, I40E_PF_FUNC_RID);

	if (pci_cap_ari)
		i40e_dev->pf_id = (uint8_t)(func_rid & 0xff);
	else
		i40e_dev->pf_id = (uint8_t)(func_rid & 0x7);

	return;
}

static int i40e_configure_filter(struct ufp_dev *dev)
{
	int err;
	uint32_t val;

	/* Read the PF Queue Filter control register */
	err = i40e_rxctl_read(dev, I40E_PFQF_CTL_0, &val);
	if(err < 0)
		goto err_pfqfctl0_read;

	/* Program required PE hash buckets for the PF */
	val &= ~I40E_PFQF_CTL_0_PEHSIZE_MASK;
	val |= ((uint32_t)I40E_HASH_FILTER_SIZE_1K
		<< I40E_PFQF_CTL_0_PEHSIZE_SHIFT)
		& I40E_PFQF_CTL_0_PEHSIZE_MASK;

	/* Program required PE contexts for the PF */
	val &= ~I40E_PFQF_CTL_0_PEDSIZE_MASK;
	val |= ((uint32_t)I40E_DMA_CNTX_SIZE_512
		<< I40E_PFQF_CTL_0_PEDSIZE_SHIFT)
		& I40E_PFQF_CTL_0_PEDSIZE_MASK;

	/* Program required FCoE hash buckets for the PF */
	val &= ~I40E_PFQF_CTL_0_PFFCHSIZE_MASK;
	val |= ((uint32_t)I40E_HASH_FILTER_SIZE_1K
		<< I40E_PFQF_CTL_0_PFFCHSIZE_SHIFT)
		& I40E_PFQF_CTL_0_PFFCHSIZE_MASK;

	/* Program required FCoE DDP contexts for the PF */
	val &= ~I40E_PFQF_CTL_0_PFFCDSIZE_MASK;
	val |= ((uint32_t)I40E_DMA_CNTX_SIZE_512
		<< I40E_PFQF_CTL_0_PFFCDSIZE_SHIFT)
		& I40E_PFQF_CTL_0_PFFCDSIZE_MASK;

	/* Program Hash LUT size for the PF */
	val &= ~I40E_PFQF_CTL_0_HASHLUTSIZE_MASK;
	val |= ((uint32_t)I40E_HASH_LUT_SIZE_128
		<< I40E_PFQF_CTL_0_HASHLUTSIZE_SHIFT)
		& I40E_PFQF_CTL_0_HASHLUTSIZE_MASK;

	/* Enable FDIR, Ethertype and MACVLAN filters for PF and VFs */
	val |= I40E_PFQF_CTL_0_FD_ENA_MASK;
	val |= I40E_PFQF_CTL_0_ETYPE_ENA_MASK;
	val |= I40E_PFQF_CTL_0_MACVLAN_ENA_MASK;

	err = i40e_rxctl_write(dev, I40E_PFQF_CTL_0, val);
	if(err < 0)
		goto err_pfqfctl0_write;

	return 0;

err_pfqfctl0_write:
err_pfqfctl0_read:
	return -1;
}

static int i40e_configure_rss(struct ufp_dev *dev)
{
	uint32_t reg_val, hena_low, hena_high;
	uint64_t hena;
	int err;

	/* By default we enable TCP/UDP with IPv4/IPv6 ptypes */
	err = i40e_rxctl_read(dev, I40E_PFQF_HENA(0), &hena_low);
	if(err < 0)
		goto err_hena0_read;

	err = i40e_rxctl_read(dev, I40E_PFQF_HENA(1), &hena_high);
	if(err < 0)
		goto err_hena1_read;

	hena = ((uint64_t)hena_low)
		| (((uint64_t)hena_high) << 32);
	hena |=	BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_UDP) |
		BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_SCTP) |
		BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_TCP) |
		BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) |
		BIT_ULL(I40E_FILTER_PCTYPE_FRAG_IPV4) |
		BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_UDP) |
		BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_TCP) |
		BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_SCTP) |
		BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_OTHER) |
		BIT_ULL(I40E_FILTER_PCTYPE_FRAG_IPV6) |
		BIT_ULL(I40E_FILTER_PCTYPE_L2_PAYLOAD);

	hena_low = lower32(hena);
	err = i40e_rxctl_write(dev, I40E_PFQF_HENA(0), hena_low);
	if(err < 0)
		goto err_hena0_write;

	hena_high = upper32(hena);
	err = i40e_rxctl_write(dev, I40E_PFQF_HENA(1), hena_high);
	if(err < 0)
		goto err_hena1_write;

	/* Determine the RSS table size based on the hardware capabilities */
	err = i40e_rxctl_read(dev, I40E_PFQF_CTL_0, &reg_val);
	if(err < 0)
		goto err_ctl0_read;

	reg_val |= I40E_PFQF_CTL_0_HASHLUTSIZE_512;

	err = i40e_rxctl_write(dev, I40E_PFQF_CTL_0, reg_val);
	if(err < 0)
		goto err_ctl0_write;

	return 0;

err_ctl0_write:
err_ctl0_read:
err_hena1_write:
err_hena0_write:
err_hena1_read:
err_hena0_read:
	return -1;
}

static int i40e_setup_pf_switch(struct ufp_dev *dev)
{
	struct i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_iface *iface;
	struct i40e_iface *i40e_iface;
	struct i40e_elem *elem, *_elem;
	int err;

	/* Switch Global Configuration */
	if (i40e_dev->pf_id == 0) {
		/* See 7.4.9.5.4 - Set switch configuration command
		 * Packets are forwarded according to promiscuous filter
		 * even if matching an exact match filter.
		 */
		err = i40e_set_swconf(dev,
			0, I40E_AQ_SET_SWITCH_CFG_PROMISC);
		if(err < 0)
			goto err_wait_cmd;
	}

	/* Setup static PF queue filter control settings */
	err = i40e_configure_filter(dev);
	if(err < 0)
		goto err_configure_filter;

	/* enable RSS in the HW, even for only one queue, as the stack can use
	 * the hash
	 */
	err = i40e_configure_rss(dev);
	if(err < 0)
		goto err_configure_rss;

	err = i40e_get_swconf(dev);
	if(err < 0)
		goto err_get_swconf;

	elem = NULL;
	list_for_each(&i40e_dev->elem, _elem, list){
		if(_elem->type == I40E_AQ_SW_ELEM_TYPE_VSI){
			elem = _elem;
			break;
		}
	}
	if(!elem)
		goto err_first_vsi;

	/*
	 * Set up the iface associated with the default VSI
	 * that is already in the PF HW switch (See A.3.4.6.2)
	 */
	i40e_iface = malloc(sizeof(struct i40e_iface));
	if(!i40e_iface)
		goto err_alloc_iface;

	i40e_iface->seid	= elem->seid;
	i40e_iface->id		= elem->element_info;
	i40e_iface->base_qp	= 0;
	i40e_iface->type	= I40E_VSI_MAIN;

	iface = list_first_entry(&dev->iface, struct ufp_iface, list);
	iface->drv_data		= i40e_iface;

	memcpy(iface->mac_addr, i40e_dev->pf_lan_mac, ETH_ALEN);
	iface->num_rx_desc	= I40E_MAX_NUM_DESCRIPTORS;
	iface->size_rx_desc	=
		iface->num_rx_desc * sizeof(struct i40e_rx_desc);
	iface->num_tx_desc	= I40E_MAX_NUM_DESCRIPTORS;
	iface->size_tx_desc	=
		iface->num_tx_desc * sizeof(struct i40e_tx_desc);
	return 0;

err_alloc_iface:
err_first_vsi:
	i40e_clear_swconf(dev);
err_get_swconf:
err_configure_rss:
err_configure_filter:
err_wait_cmd:
	return -1;
}

int i40e_set_rsskey_reg(struct ufp_dev *dev,
	uint8_t *key, uint16_t key_size)
{
	uint32_t *key32 = (uint32_t *)key;
	int i, err;

	if(key_size != I40E_HKEY_ARRAY_SIZE)
		goto err_size_reg;

	for (i = 0; i <= I40E_PFQF_HKEY_MAX_INDEX; i++){
		err = i40e_rxctl_write(dev,
			I40E_PFQF_HKEY(i), key32[i]);
		if(err < 0)
			goto err_write_reg;
	}
	return 0;

err_write_reg:
err_size_reg:
	return -1;
}

int i40e_set_rsslut_reg(struct ufp_dev *dev,
	uint8_t *lut, uint16_t lut_size)
{
	uint32_t *lut32 = (uint32_t *)lut;
	int i;

	if(lut_size != I40E_HLUT_ARRAY_SIZE)
		goto err_size_reg;

	for (i = 0; i <= I40E_PFQF_HLUT_MAX_INDEX; i++)
		UFP_WRITE32(dev, I40E_PFQF_HLUT(i), lut32[i]);
	return 0;

err_size_reg:
	return -1;
}
