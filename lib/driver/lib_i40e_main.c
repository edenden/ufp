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

struct ufp_i40e_page *ufp_i40e_page_alloc(struct ufp_dev *dev)
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

	err = ufp_dma_map(dev, addr_virt, &addr_dma, size);
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

void ufp_i40e_page_release(struct ufp_dev *dev, struct ufp_i40e_page *page)
{
	ufp_dma_unmap(dev, page->addr_dma);
	munmap(page->addr_virt, sysconf(_SC_PAGESIZE));
	free(page);
	return;
}

int ufp_i40e_wait_cmd(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct timespec ts;
	unsigned int timeout;

	timeout = I40E_ASQ_CMD_TIMEOUT;
	do{
		usleep(&ts, 1000);
		i40e_aq_asq_clean(dev);
		if(!i40e_dev->aq.flag)
			break;
	}while(--timeout);
	if(!timeout)
		goto err_timeout;

	return 0;

err_timeout:
	return -1;
}

int i40e_reset_hw(struct ufp_dev *dev)
{
	struct timespec ts;
	uint32_t grst_del, reg;
	unsigned int timeout;

	/* Poll for Global Reset steady state in case of recent GRST.
	 * The grst delay value is in 100ms units, and we'll wait a
	 * couple counts longer to be sure we don't just miss the end.
	 */
	grst_del = (ufp_read_reg(dev, I40E_GLGEN_RSTCTL) &
		I40E_GLGEN_RSTCTL_GRSTDEL_MASK) >> I40E_GLGEN_RSTCTL_GRSTDEL_SHIFT;
	timeout = grst_del * 20;
	do{
		msleep(&ts, 100);
		reg = ufp_read_reg(dev, I40E_GLGEN_RSTAT);
		if(!(reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK))
			break;
	}while(--timeout);
	if(!timeout)
		goto err_grst_in_progress;

	/* Now Wait for the FW to be ready */
	timeout = I40E_PF_RESET_WAIT_COUNT;
	do{
		usleep(&ts, 10000);
		reg = ufp_read_reg(dev, I40E_GLNVM_ULD);
		if((reg & I40E_GLNVM_ULD_CONF_CORE_DONE_MASK) &&
		(reg & I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK))
			break;
	}while(--timeout);
	if(!timeout)
		goto err_firmware_not_ready;

	reg = ufp_read_reg(dev, I40E_PFGEN_CTRL);
	reg |= I40E_PFGEN_CTRL_PFSWR_MASK;
	ufp_write_reg(dev, I40E_PFGEN_CTRL, reg);

	timeout = I40E_PF_RESET_WAIT_COUNT;
	do{
		usleep(&ts, 1000);
		reg = ufp_read_reg(dev, I40E_PFGEN_CTRL);
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

void i40e_clear_hw(struct ufp_dev *dev)
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
	val = ufp_read_reg(dev, I40E_GLPCI_CNF2);
	num_pf_int = (val & I40E_GLPCI_CNF2_MSI_X_PF_N_MASK) >>
			I40E_GLPCI_CNF2_MSI_X_PF_N_SHIFT;
	num_vf_int = (val & I40E_GLPCI_CNF2_MSI_X_VF_N_MASK) >>
			I40E_GLPCI_CNF2_MSI_X_VF_N_SHIFT;

	val = ufp_read_reg(dev, I40E_PFLAN_QALLOC);
	base_queue = (val & I40E_PFLAN_QALLOC_FIRSTQ_MASK) >>
			I40E_PFLAN_QALLOC_FIRSTQ_SHIFT;
	j = (val & I40E_PFLAN_QALLOC_LASTQ_MASK) >>
			I40E_PFLAN_QALLOC_LASTQ_SHIFT;
	if (val & I40E_PFLAN_QALLOC_VALID_MASK)
		num_queues = (j - base_queue) + 1;
	else
		num_queues = 0;

	val = ufp_read_reg(dev, I40E_PF_VT_PFALLOC);
	i = (val & I40E_PF_VT_PFALLOC_FIRSTVF_MASK) >>
			I40E_PF_VT_PFALLOC_FIRSTVF_SHIFT;
	j = (val & I40E_PF_VT_PFALLOC_LASTVF_MASK) >>
			I40E_PF_VT_PFALLOC_LASTVF_SHIFT;
	if (val & I40E_PF_VT_PFALLOC_VALID_MASK)
		num_vfs = (j - i) + 1;
	else
		num_vfs = 0;

	/* stop all the interrupts */
	ufp_write_reg(dev, I40E_PFINT_ICR0_ENA, 0);
	val = 0x3 << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT;
	for (i = 0; i < num_pf_int - 2; i++)
		ufp_write_reg(dev, I40E_PFINT_DYN_CTLN(i), val);

	/* Set the FIRSTQ_INDX field to 0x7FF in PFINT_LNKLSTx */
	val = eol << I40E_PFINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	ufp_write_reg(dev, I40E_PFINT_LNKLST0, val);
	for (i = 0; i < num_pf_int - 2; i++)
		ufp_write_reg(dev, I40E_PFINT_LNKLSTN(i), val);
	val = eol << I40E_VPINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	for (i = 0; i < num_vfs; i++)
		ufp_write_reg(dev, I40E_VPINT_LNKLST0(i), val);
	for (i = 0; i < num_vf_int - 2; i++)
		ufp_write_reg(dev, I40E_VPINT_LNKLSTN(i), val);

	/* warn the HW of the coming Tx disables */
	for (i = 0; i < num_queues; i++) {
		uint32_t abs_queue_idx = base_queue + i;
		uint32_t reg_block = 0;

		if (abs_queue_idx >= 128) {
			reg_block = abs_queue_idx / 128;
			abs_queue_idx %= 128;
		}

		val = ufp_read_reg(dev, I40E_GLLAN_TXPRE_QDIS(reg_block));
		val &= ~I40E_GLLAN_TXPRE_QDIS_QINDX_MASK;
		val |= (abs_queue_idx << I40E_GLLAN_TXPRE_QDIS_QINDX_SHIFT);
		val |= I40E_GLLAN_TXPRE_QDIS_SET_QDIS_MASK;

		ufp_write_reg(dev, I40E_GLLAN_TXPRE_QDIS(reg_block), val);
	}
	usleep(&ts, 400);

	/* stop all the queues */
	for (i = 0; i < num_queues; i++) {
		ufp_write_reg(dev, I40E_QINT_TQCTL(i), 0);
		ufp_write_reg(dev, I40E_QTX_ENA(i), 0);
		ufp_write_reg(dev, I40E_QINT_RQCTL(i), 0);
		ufp_write_reg(dev, I40E_QRX_ENA(i), 0);
	}

	/* short wait for all queue disables to settle */
	usleep(&ts, 50);

	return;
}

void i40e_set_mac_type(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;

	switch(dev->ops->device_id){
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
		i40e_dev->mac_type = I40E_MAC_XL710;
		break;
	case I40E_DEV_ID_KX_X722:
	case I40E_DEV_ID_QSFP_X722:
	case I40E_DEV_ID_SFP_X722:
	case I40E_DEV_ID_1G_BASE_T_X722:
	case I40E_DEV_ID_10G_BASE_T_X722:
	case I40E_DEV_ID_SFP_I_X722:
	case I40E_DEV_ID_QSFP_I_X722:
		i40e_dev->mac_type = I40E_MAC_X722;
		break;
	default:
		i40e_dev->mac_type = I40E_MAC_GENERIC;
		break;
	}

	if(i40e_dev->mac_type == I40E_MAC_GENERIC)
		goto err_mac_unknown;

	return 0;

err_mac_unknown:
	return -1;
}

void i40e_set_pf_id(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	uint32_t pci_cap, pci_cap_ari, func_rid;

	pci_cap = rd32(hw, I40E_GLPCI_CAPSUP);
	pci_cap_ari = (pci_cap & I40E_GLPCI_CAPSUP_ARI_EN_MASK) >>
		I40E_GLPCI_CAPSUP_ARI_EN_SHIFT;
	func_rid = rd32(hw, I40E_PF_FUNC_RID);

	if (pci_cap_ari)
		i40e_dev->pf_id = (u8)(func_rid & 0xff);
	else
		i40e_dev->pf_id = (u8)(func_rid & 0x7);

	return;
}

int i40e_switchconf_fetch(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	int err;

	i40e_dev->aq_seid_offset = 0;
	clear_list(i40e_dev->switch_elem);

	do{
		err = i40e_aq_cmd_xmit_getconf(dev);
		if(err < 0)
			goto err_cmd_xmit_getconf;

		while(!(data->flag & AQ_GETCONF)){
			i40e_aq_asq_clean(dev);
		}
	}while(i40e_dev->aq_seid_offset);

	return 0;

err_cmd_xmit_getconf:
	return -1;
}

int i40e_configure_filter(struct ufp_dev *dev)
{
	uint32_t val;

	/* Read the PF Queue Filter control register */
	val = i40e_read_rx_ctl(hw, I40E_PFQF_CTL_0);

	/* Program required PE hash buckets for the PF */
	val &= ~I40E_PFQF_CTL_0_PEHSIZE_MASK;
	val |= ((u32)I40E_HASH_FILTER_SIZE_1K << I40E_PFQF_CTL_0_PEHSIZE_SHIFT) &
		I40E_PFQF_CTL_0_PEHSIZE_MASK;
	/* Program required PE contexts for the PF */
	val &= ~I40E_PFQF_CTL_0_PEDSIZE_MASK;
	val |= ((u32)I40E_DMA_CNTX_SIZE_512 << I40E_PFQF_CTL_0_PEDSIZE_SHIFT) &
		I40E_PFQF_CTL_0_PEDSIZE_MASK;

	/* Program required FCoE hash buckets for the PF */
	val &= ~I40E_PFQF_CTL_0_PFFCHSIZE_MASK;
	val |= ((u32)I40E_HASH_FILTER_SIZE_1K <<
			I40E_PFQF_CTL_0_PFFCHSIZE_SHIFT) &
		I40E_PFQF_CTL_0_PFFCHSIZE_MASK;
	/* Program required FCoE DDP contexts for the PF */
	val &= ~I40E_PFQF_CTL_0_PFFCDSIZE_MASK;
	val |= ((u32)I40E_DMA_CNTX_SIZE_512 <<
			I40E_PFQF_CTL_0_PFFCDSIZE_SHIFT) &
		I40E_PFQF_CTL_0_PFFCDSIZE_MASK;

	/* Program Hash LUT size for the PF */
	val &= ~I40E_PFQF_CTL_0_HASHLUTSIZE_MASK;
	val |= ((u32)I40E_HASH_LUT_SIZE_128 << I40E_PFQF_CTL_0_HASHLUTSIZE_SHIFT) &
		I40E_PFQF_CTL_0_HASHLUTSIZE_MASK;

	/* Enable FDIR, Ethertype and MACVLAN filters for PF and VFs */
	val |= I40E_PFQF_CTL_0_FD_ENA_MASK;
	val |= I40E_PFQF_CTL_0_ETYPE_ENA_MASK;
	val |= I40E_PFQF_CTL_0_MACVLAN_ENA_MASK;

	i40e_write_rx_ctl(hw, I40E_PFQF_CTL_0, val);

	return 0;
}

static int i40e_configure_rss(struct ufp_dev *dev)
{
	u32 reg_val;
	u64 hena;

	/* By default we enable TCP/UDP with IPv4/IPv6 ptypes */
	hena = (u64)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(0)) |
		((u64)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(1)) << 32);
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

	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(0), (u32)hena);
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(1), (u32)(hena >> 32));

	/* Determine the RSS table size based on the hardware capabilities */
	reg_val = i40e_read_rx_ctl(hw, I40E_PFQF_CTL_0);
	reg_val |= I40E_PFQF_CTL_0_HASHLUTSIZE_512;
	i40e_write_rx_ctl(hw, I40E_PFQF_CTL_0, reg_val);

	return 0;
}

static int i40e_setup_pf_switch(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_iface *iface;
	struct ufp_i40e_iface *i40e_iface;
	struct switch_elem *elem;
	int err;

	/* Switch Global Configuration */
	if (i40e_dev->pf_id == 0) {
		/* See 7.4.9.5.4 - Set switch configuration command
		 * Packets are forwarded according to promiscuous filter
		 * even if matching an exact match filter.
		 */
		err = i40e_aq_cmd_xmit_setconf(dev,
			0, I40E_AQ_SET_SWITCH_CFG_PROMISC);
		if(err < 0)
			goto err_switch_setconf;
	}

	/* Setup static PF queue filter control settings */
	err = i40e_configure_filter(dev);
	if(err < 0)
		goto err_set_filter_ctrl;

	/* enable RSS in the HW, even for only one queue, as the stack can use
	 * the hash
	 */
	err = i40e_configure_rss(dev);
	if(err < 0)
		goto err_configure_rss;

	foreach(data->switch_elem){
		if(elem->type == I40E_SWITCH_ELEMENT_TYPE_VSI){
			elem = current;
			break;
		}
	}
	if(!elem)
		goto err_first_vsi;

	/* Set up the PF VSI associated with the PF's main VSI
	 * that is already in the HW switch
	 */
	i40e_iface = malloc(sizeof(struct ufp_i40e_iface));
	if(!i40e_iface)
		goto err_alloc_iface;

	i40e_iface->seid = elem->seid;
	i40e_iface->id = elem->element_info;
	i40e_iface->type = VSI_TYPE_MAIN;
	iface = dev->iface;
	iface->drv_data = i40e_iface;

	memcpy(iface->mac_addr, i40e_dev->pf_lan_mac, ETH_ALEN);
	iface->num_rx_desc = I40E_MAX_NUM_DESCRIPTORS;
	iface->size_rx_desc = 
		iface->num_rx_desc * sizeof(union i40e_16byte_rx_desc);
	iface->num_tx_desc = I40E_MAX_NUM_DESCRIPTORS;
	iface->size_tx_desc =
		iface->num_tx_desc * sizeof(union i40e_tx_desc);
	i40e_vlan_stripping_disable(dev, iface);
	i40e_vsi_rss_config(dev, iface);

	return 0;

err_set_filter_ctrl:
err_switch_setconf:
	free(i40e_iface);
err_alloc_iface:
err_first_vsi:
err_switchconf_fetch:
	return -1;
}

