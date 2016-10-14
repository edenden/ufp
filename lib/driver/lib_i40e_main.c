#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

#include "lib_main.h"
#include "lib_rtx.h"

#include "lib_i40e.h"
#include "lib_i40e_main.h"

struct ufp_i40e_page *ufp_i40e_page_alloc(struct ufp_handle *ih)
{
	struct ufp_i40e_page *page;
	unsigned long addr_dma, size;
	void *addr_virt;
	int err;

	page = malloc(sizeof(struct ufp_i40e_page));
	if(!page)
		goto err_alloc_page;

	size = sysconf(_SC_PAGESIZE);
	addr_virt = mmap(NULL, size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if(addr_virt == MAP_FAILED){
		goto err_mmap;
	}

	page->addr_virt = addr_virt;

	err = ufp_dma_map(ih, addr_virt, &addr_dma, size);
	if(err < 0){
		goto err_dma_map;
	}

	page->addr_dma = addr_dma;

	return page;

err_dma_map:
	munmap(page->addr_virt, size);
err_mmap:
	free(page);
err_alloc_page:
	return NULL;
}

void ufp_i40e_page_release(struct ufp_handle *ih, struct ufp_i40e_page *page)
{
	ufp_dma_unmap(ih, page->addr_dma);
	munmap(page->addr_virt, sysconf(_SC_PAGESIZE));
	free(page);
	return;
}

int i40e_reset_hw(struct ufp_handle *ih)
{
	struct timespec ts;
	int err;
	uint32_t cnt = 0;
	uint32_t cnt1 = 0;
	uint32_t reg = 0;
	uint32_t grst_del;

	/* Poll for Global Reset steady state in case of recent GRST.
	 * The grst delay value is in 100ms units, and we'll wait a
	 * couple counts longer to be sure we don't just miss the end.
	 */
	grst_del = (ufp_read_reg(ih, I40E_GLGEN_RSTCTL) &
			I40E_GLGEN_RSTCTL_GRSTDEL_MASK) >>
			I40E_GLGEN_RSTCTL_GRSTDEL_SHIFT;

	grst_del = grst_del * 20;

	for (cnt = 0; cnt < grst_del; cnt++) {
		reg = ufp_read_reg(ih, I40E_GLGEN_RSTAT);
		if (!(reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK))
			break;
		msleep(&ts, 100);
	}
	if (reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK) {
		return I40E_ERR_RESET_FAILED;
	}

	/* Now Wait for the FW to be ready */
	for (cnt1 = 0; cnt1 < I40E_PF_RESET_WAIT_COUNT; cnt1++) {
		reg = ufp_read_reg(ih, I40E_GLNVM_ULD);
		reg &= (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
			I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK);
		if (reg == (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
			    I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK)) {
			break;
		}
		usleep(&ts, 10000);
	}
	if (!(reg & (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
		     I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK))) {
		return I40E_ERR_RESET_FAILED;
	}

	/* If there was a Global Reset in progress when we got here,
	 * we don't need to do the PF Reset
	 */
	if (!cnt) {
		reg = ufp_read_reg(ih, I40E_PFGEN_CTRL);
		ufp_write_reg(ih, I40E_PFGEN_CTRL,
		     (reg | I40E_PFGEN_CTRL_PFSWR_MASK));
		for (cnt = 0; cnt < I40E_PF_RESET_WAIT_COUNT; cnt++) {
			reg = ufp_read_reg(ih, I40E_PFGEN_CTRL);
			if (!(reg & I40E_PFGEN_CTRL_PFSWR_MASK))
				break;
			usleep_range(1000, 2000);
		}
		if (reg & I40E_PFGEN_CTRL_PFSWR_MASK) {
			return I40E_ERR_RESET_FAILED;
		}
	}

	return 0;
}

void i40e_clear_hw(struct ufp_handle *ih)
{
	struct timespec ts;
	uint32_t num_queues, base_queue;
	uint32_t num_pf_int;
	uint32_t num_vf_int;
	uint32_t num_vfs;
	uint32_t i, j;
	uint32_t val;
	uint32_t eol = 0x7ff;

	/* get number of interrupts, queues, and vfs */
	val = ufp_read_reg(ih, I40E_GLPCI_CNF2);
	num_pf_int = (val & I40E_GLPCI_CNF2_MSI_X_PF_N_MASK) >>
			I40E_GLPCI_CNF2_MSI_X_PF_N_SHIFT;
	num_vf_int = (val & I40E_GLPCI_CNF2_MSI_X_VF_N_MASK) >>
			I40E_GLPCI_CNF2_MSI_X_VF_N_SHIFT;

	val = ufp_read_reg(ih, I40E_PFLAN_QALLOC);
	base_queue = (val & I40E_PFLAN_QALLOC_FIRSTQ_MASK) >>
			I40E_PFLAN_QALLOC_FIRSTQ_SHIFT;
	j = (val & I40E_PFLAN_QALLOC_LASTQ_MASK) >>
			I40E_PFLAN_QALLOC_LASTQ_SHIFT;
	if (val & I40E_PFLAN_QALLOC_VALID_MASK)
		num_queues = (j - base_queue) + 1;
	else
		num_queues = 0;

	val = ufp_read_reg(ih, I40E_PF_VT_PFALLOC);
	i = (val & I40E_PF_VT_PFALLOC_FIRSTVF_MASK) >>
			I40E_PF_VT_PFALLOC_FIRSTVF_SHIFT;
	j = (val & I40E_PF_VT_PFALLOC_LASTVF_MASK) >>
			I40E_PF_VT_PFALLOC_LASTVF_SHIFT;
	if (val & I40E_PF_VT_PFALLOC_VALID_MASK)
		num_vfs = (j - i) + 1;
	else
		num_vfs = 0;

	/* stop all the interrupts */
	ufp_write_reg(ih, I40E_PFINT_ICR0_ENA, 0);
	val = 0x3 << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT;
	for (i = 0; i < num_pf_int - 2; i++)
		ufp_write_reg(ih, I40E_PFINT_DYN_CTLN(i), val);

	/* Set the FIRSTQ_INDX field to 0x7FF in PFINT_LNKLSTx */
	val = eol << I40E_PFINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	ufp_write_reg(ih, I40E_PFINT_LNKLST0, val);
	for (i = 0; i < num_pf_int - 2; i++)
		ufp_write_reg(ih, I40E_PFINT_LNKLSTN(i), val);
	val = eol << I40E_VPINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	for (i = 0; i < num_vfs; i++)
		ufp_write_reg(ih, I40E_VPINT_LNKLST0(i), val);
	for (i = 0; i < num_vf_int - 2; i++)
		ufp_write_reg(ih, I40E_VPINT_LNKLSTN(i), val);

	/* warn the HW of the coming Tx disables */
	for (i = 0; i < num_queues; i++) {
		uint32_t abs_queue_idx = base_queue + i;
		uint32_t reg_block = 0;

		if (abs_queue_idx >= 128) {
			reg_block = abs_queue_idx / 128;
			abs_queue_idx %= 128;
		}

		val = ufp_read_reg(ih, I40E_GLLAN_TXPRE_QDIS(reg_block));
		val &= ~I40E_GLLAN_TXPRE_QDIS_QINDX_MASK;
		val |= (abs_queue_idx << I40E_GLLAN_TXPRE_QDIS_QINDX_SHIFT);
		val |= I40E_GLLAN_TXPRE_QDIS_SET_QDIS_MASK;

		ufp_write_reg(ih, I40E_GLLAN_TXPRE_QDIS(reg_block), val);
	}
	usleep(&ts, 400);

	/* stop all the queues */
	for (i = 0; i < num_queues; i++) {
		ufp_write_reg(ih, I40E_QINT_TQCTL(i), 0);
		ufp_write_reg(ih, I40E_QTX_ENA(i), 0);
		ufp_write_reg(ih, I40E_QINT_RQCTL(i), 0);
		ufp_write_reg(ih, I40E_QRX_ENA(i), 0);
	}

	/* short wait for all queue disables to settle */
	usleep(&ts, 50);

	return;
}

void i40e_set_mac_type(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;

	switch(ih->ops->device_id){
	case I40E_DEV_ID_SFP_XL710:
	case I40E_DEV_ID_QEMU:
	case I40E_DEV_ID_KX_B:
	case I40E_DEV_ID_KX_C:
	case I40E_DEV_ID_QSFP_A:
	case I40E_DEV_ID_QSFP_B:
	case I40E_DEV_ID_QSFP_C:
	case I40E_DEV_ID_10G_BASE_T:
	case I40E_DEV_ID_10G_BASE_T4:
	case I40E_DEV_ID_20G_KR2:
	case I40E_DEV_ID_20G_KR2_A:
	case I40E_DEV_ID_25G_B:
	case I40E_DEV_ID_25G_SFP28:
		data->mac_type = I40E_MAC_XL710;
		break;
	case I40E_DEV_ID_KX_X722:
	case I40E_DEV_ID_QSFP_X722:
	case I40E_DEV_ID_SFP_X722:
	case I40E_DEV_ID_1G_BASE_T_X722:
	case I40E_DEV_ID_10G_BASE_T_X722:
	case I40E_DEV_ID_SFP_I_X722:
	case I40E_DEV_ID_QSFP_I_X722:
		data->mac_type = I40E_MAC_X722;
		break;
	default:
		data->mac_type = I40E_MAC_GENERIC;
		break;
	}

	return;
}

static int i40e_setup_pf_switch(struct i40e_pf *pf, bool reinit)
{
	u16 flags = 0;
	struct i40e_vsi *vsi = NULL;
	u16 uplink_seid;
	int ret;

	/* find out what's out there already */
	ret = i40e_fetch_switch_configuration(pf, false);

	if (pf->hw.pf_id == 0) {
		u16 valid_flags;

		valid_flags = I40E_AQ_SET_SWITCH_CFG_PROMISC;
		ret = i40e_aq_set_switch_config(&pf->hw, flags, valid_flags,
						NULL);
	}

	/* Set up the PF VSI associated with the PF's main VSI
	 * that is already in the HW switch
	 */
	uplink_seid = pf->mac_seid;

	vsi = i40e_vsi_setup(pf, I40E_VSI_MAIN, uplink_seid, 0);
	if (!vsi) {
		dev_info(&pf->pdev->dev, "setup of MAIN VSI failed\n");
		i40e_fdir_teardown(pf);
		return -EAGAIN;
	}

	i40e_vlan_stripping_disable(pf->vsi[pf->lan_vsi]);

	i40e_fdir_sb_setup(pf);

	/* Setup static PF queue filter control settings */
	ret = i40e_setup_pf_filter_control(pf);
	if (ret) {
		dev_info(&pf->pdev->dev, "setup_pf_filter_control failed: %d\n",
			 ret);
		/* Failure here should not stop continuing other steps */
	}

	/* enable RSS in the HW, even for only one queue, as the stack can use
	 * the hash
	 */
	if ((pf->flags & I40E_FLAG_RSS_ENABLED))
		i40e_pf_config_rss(pf);

	/* fill in link information and enable LSE reporting */
	i40e_update_link_info(&pf->hw);
	i40e_link_event(pf);

	/* Initialize user-specific link properties */
	pf->fc_autoneg_status = ((pf->hw.phy.link_info.an_info &
				  I40E_AQ_AN_COMPLETED) ? true : false);

	return ret;
}

struct i40e_vsi_context {
	u16 seid;
	u16 uplink_seid;
	u16 vsi_number;
	u16 vsis_allocated;
	u16 vsis_unallocated;
	u16 flags;
	u8 pf_num;
	u8 vf_num;
	u8 connection_type;
	struct i40e_aqc_vsi_properties_data info;
};

static int i40e_add_vsi(struct i40e_vsi *vsi)
{
	int ret = -ENODEV;
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_vsi_context ctxt;

	u8 enabled_tc = 0x1; /* TC0 enabled */
	u32 val;

	memset(&ctxt, 0, sizeof(ctxt));
	switch (vsi->type) {
	case I40E_VSI_MAIN:
		/* The PF's main VSI is already setup as part of the
		 * device initialization, so we'll not bother with
		 * the add_vsi call, but we will retrieve the current
		 * VSI context.
		 */
		ctxt.seid = pf->main_vsi_seid;
		ctxt.pf_num = pf->hw.pf_id;
		ctxt.vf_num = 0;
		ret = i40e_aq_get_vsi_params(&pf->hw, &ctxt, NULL);
		ctxt.flags = I40E_AQ_VSI_TYPE_PF;
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "couldn't get PF vsi config, err %s aq_err %s\n",
				 i40e_stat_str(&pf->hw, ret),
				 i40e_aq_str(&pf->hw,
					      pf->hw.aq.asq_last_status));
			return -ENOENT;
		}
		vsi->info = ctxt.info;
		vsi->info.valid_sections = 0;

		vsi->seid = ctxt.seid;
		vsi->id = ctxt.vsi_number;

		enabled_tc = i40e_pf_get_tc_map(pf);

		/* Default/Main VSI is only enabled for TC0
		 * reconfigure it to enable all TCs that are
		 * available on the port in SFP mode.
		 * For MFP case the iSCSI PF would use this
		 * flow to enable LAN+iSCSI TC.
		 */
		ret = i40e_vsi_config_tc(vsi, enabled_tc);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "failed to configure TCs for main VSI tc_map 0x%08x, err %s aq_err %s\n",
				 enabled_tc,
				 i40e_stat_str(&pf->hw, ret),
				 i40e_aq_str(&pf->hw,
					    pf->hw.aq.asq_last_status));
			ret = -ENOENT;
		}

		break;
	case I40E_VSI_FDIR:
		ctxt.pf_num = hw->pf_id;
		ctxt.vf_num = 0;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.connection_type = I40E_AQ_VSI_CONN_TYPE_NORMAL;
		ctxt.flags = I40E_AQ_VSI_TYPE_PF;
		if ((pf->flags & I40E_FLAG_VEB_MODE_ENABLED) &&
		    (i40e_is_vsi_uplink_mode_veb(vsi))) {
			ctxt.info.valid_sections |=
			     cpu_to_le16(I40E_AQ_VSI_PROP_SWITCH_VALID);
			ctxt.info.switch_id =
			   cpu_to_le16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);
		}
		i40e_vsi_setup_queue_map(vsi, &ctxt, enabled_tc, true);
		break;
	default:
		return -ENODEV;
	}

	if (vsi->type != I40E_VSI_MAIN) {
		ret = i40e_aq_add_vsi(hw, &ctxt, NULL);
		if (ret) {
			dev_info(&vsi->back->pdev->dev,
				 "add vsi failed, err %s aq_err %s\n",
				 i40e_stat_str(&pf->hw, ret),
				 i40e_aq_str(&pf->hw,
					      pf->hw.aq.asq_last_status));
			ret = -ENOENT;
			goto err;
		}
		vsi->info = ctxt.info;
		vsi->info.valid_sections = 0;
		vsi->seid = ctxt.seid;
		vsi->id = ctxt.vsi_number;
		val = rd32(&pf->hw, 0x208800 + (4*(vsi->id)));
		if (!(val & 0x1)) /* MACVSIPRUNEENABLE = 1*/
			dev_warn(&vsi->back->pdev->dev,
				 "Note: VSI source pruning is not being set correctly by FW\n");
	}

	/* Update VSI BW information */
	ret = i40e_vsi_get_bw_info(vsi);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "couldn't get vsi bw info, err %s aq_err %s\n",
			 i40e_stat_str(&pf->hw, ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		/* VSI is already added so not tearing that up */
		ret = 0;
	}

err:
	return ret;
}

