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

/*
static u16 fm10k_get_pcie_msix_count_generic(struct fm10k_hw *hw)
{
	u16 msix_count;

	/* read in value from MSI-X capability register */
	msix_count = fm10k_read_pci_cfg_word(hw, FM10K_PCI_MSIX_MSG_CTRL);
	msix_count &= FM10K_PCI_MSIX_MSG_CTRL_TBL_SZ_MASK;

	/* MSI-X count is zero-based in HW */
	msix_count++;

	if (msix_count > FM10K_MAX_MSIX_VECTORS)
		msix_count = FM10K_MAX_MSIX_VECTORS;

	return msix_count;
}

s32 fm10k_get_constant_value(struct fm10k_hw *hw)
{
	struct fm10k_mac_info *mac = &hw->mac;

	/* initialize GLORT state to avoid any false hits */
	mac->dglort_map = FM10K_DGLORTMAP_NONE;

	/* record maximum number of MSI-X vectors */
	mac->max_msix_vectors = fm10k_get_pcie_msix_count_generic(hw);

	return 0;
}
*/

int ufp_fm10k_reset_hw(struct ufp_handle *ih)
{
	int err, i;
	uint32_t reg;

	/* Disable interrupts */
	ufp_write_reg(ih, FM10K_EIMR, FM10K_EIMR_DISABLE(ALL));

	/* Lock ITR2 reg 0 into itself and disable interrupt moderation */
	ufp_write_reg(ih, FM10K_ITR2(0), 0);
	ufp_write_reg(ih, FM10K_INT_CTRL, 0);

	/* We assume here Tx and Rx queue 0 are owned by the PF */

	/* Shut off VF access to their queues forcing them to queue 0 */
	for (i = 0; i < FM10K_TQMAP_TABLE_SIZE; i++) {
		ufp_write_reg(ih, FM10K_TQMAP(i), 0);
		ufp_write_reg(ih, FM10K_RQMAP(i), 0);
	}

	/* shut down all rings */
	err = ufp_fm10k_disable_queues(ih, FM10K_MAX_QUEUES);
	if(err < 0)
		goto err_disable_queues;

	/* Verify that DMA is no longer active */
	reg = ufp_read_reg(ih, FM10K_DMA_CTRL);
	if (reg & (FM10K_DMA_CTRL_TX_ACTIVE | FM10K_DMA_CTRL_RX_ACTIVE))
		return FM10K_ERR_DMA_PENDING;

	/* Inititate data path reset */
	reg = FM10K_DMA_CTRL_DATAPATH_RESET;
	ufp_write_reg(ih, FM10K_DMA_CTRL, reg);

	/* Flush write and allow 100us for reset to complete */
	fm10k_write_flush(ih);
	udelay(FM10K_RESET_TIMEOUT);

	/* Reset mailbox global interrupts */
	reg = FM10K_MBX_GLOBAL_REQ_INTERRUPT | FM10K_MBX_GLOBAL_ACK_INTERRUPT;
	ufp_write_reg(ih, FM10K_GMBX, reg);

	/* Verify we made it out of reset */
	reg = ufp_read_reg(ih, FM10K_IP);
	if (!(reg & FM10K_IP_NOTINRESET))
		return FM10K_ERR_RESET_FAILED;

	return 0;

err_disable_queues:
	return -1;
}

static int ufp_fm10k_disable_queues(struct ufp_handle *ih, uint16_t q_cnt)
{
	int i, time;
	uint32_t reg;

	/* clear the enable bit for all rings */
	for (i = 0; i < q_cnt; i++) {
		reg = fm10k_read_reg(hw, FM10K_TXDCTL(i));
		fm10k_write_reg(hw, FM10K_TXDCTL(i),
				reg & ~FM10K_TXDCTL_ENABLE);
		reg = fm10k_read_reg(hw, FM10K_RXQCTL(i));
		fm10k_write_reg(hw, FM10K_RXQCTL(i),
				reg & ~FM10K_RXQCTL_ENABLE);
	}

	fm10k_write_flush(hw);
	udelay(1);

	/* loop through all queues to verify that they are all disabled */
	for (i = 0, time = FM10K_QUEUE_DISABLE_TIMEOUT; time;) {
		/* if we are at end of rings all rings are disabled */
		if (i == q_cnt)
			return 0;

		/* if queue enables cleared, then move to next ring pair */
		reg = fm10k_read_reg(hw, FM10K_TXDCTL(i));
		if (!~reg || !(reg & FM10K_TXDCTL_ENABLE)) {
			reg = fm10k_read_reg(hw, FM10K_RXQCTL(i));
			if (!~reg || !(reg & FM10K_RXQCTL_ENABLE)) {
				i++;
				continue;
			}
		}

		/* decrement time and wait 1 usec */
		time--;
		if (time)
			udelay(1);
	}

	return -1;
}

int lib_fm10k_init_hw(struct ufp_handle *ih,
	uint32_t num_qps_req, uint32_t num_rx_desc, uint32_t num_tx_desc)
{
	uint32_t dma_ctrl, txqctl;
	int i;

	/* Establish default VSI as valid */
	fm10k_write_reg(hw, FM10K_DGLORTDEC(fm10k_dglort_default), 0);
	fm10k_write_reg(hw, FM10K_DGLORTMAP(fm10k_dglort_default),
			FM10K_DGLORTMAP_ANY);

	/* Invalidate all other GLORT entries */
	for (i = 1; i < FM10K_DGLORT_COUNT; i++)
		fm10k_write_reg(hw, FM10K_DGLORTMAP(i), FM10K_DGLORTMAP_NONE);

	/* reset ITR2(0) to point to itself */
	fm10k_write_reg(hw, FM10K_ITR2(0), 0);

	/* reset VF ITR2(0) to point to 0 avoid PF registers */
	fm10k_write_reg(hw, FM10K_ITR2(FM10K_ITR_REG_COUNT_PF), 0);

	/* loop through all PF ITR2 registers pointing them to the previous */
	for (i = 1; i < FM10K_ITR_REG_COUNT_PF; i++)
		fm10k_write_reg(hw, FM10K_ITR2(i), i - 1);

	/* Enable interrupt moderator if not already enabled */
	fm10k_write_reg(hw, FM10K_INT_CTRL, FM10K_INT_CTRL_ENABLEMODERATOR);

	/* compute the default txqctl configuration */
	txqctl = FM10K_TXQCTL_PF | FM10K_TXQCTL_UNLIMITED_BW;

	for (i = 0; i < FM10K_MAX_QUEUES; i++) {
		/* configure rings for 256 Queue / 32 Descriptor cache mode */
		fm10k_write_reg(hw, FM10K_TQDLOC(i),
				(i * FM10K_TQDLOC_BASE_32_DESC) |
				FM10K_TQDLOC_SIZE_32_DESC);
		fm10k_write_reg(hw, FM10K_TXQCTL(i), txqctl);

		/* configure rings to provide TPH processing hints */
		fm10k_write_reg(hw, FM10K_TPH_TXCTRL(i),
				FM10K_TPH_TXCTRL_DESC_TPHEN |
				FM10K_TPH_TXCTRL_DESC_RROEN |
				FM10K_TPH_TXCTRL_DESC_WROEN |
				FM10K_TPH_TXCTRL_DATA_RROEN);
		fm10k_write_reg(hw, FM10K_TPH_RXCTRL(i),
				FM10K_TPH_RXCTRL_DESC_TPHEN |
				FM10K_TPH_RXCTRL_DESC_RROEN |
				FM10K_TPH_RXCTRL_DATA_WROEN |
				FM10K_TPH_RXCTRL_HDR_WROEN);
	}

	/* set max hold interval to align with 1.024 usec in all modes and
	 * store ITR scale
	 */
	switch (hw->bus.speed) {
	case fm10k_bus_speed_2500:
		dma_ctrl = FM10K_DMA_CTRL_MAX_HOLD_1US_GEN1;
		break;
	case fm10k_bus_speed_5000:
		dma_ctrl = FM10K_DMA_CTRL_MAX_HOLD_1US_GEN2;
		break;
	case fm10k_bus_speed_8000:
		dma_ctrl = FM10K_DMA_CTRL_MAX_HOLD_1US_GEN3;
		break;
	default:
		dma_ctrl = 0;
		/* just in case, assume Gen3 ITR scale */
		break;
	}

	/* Configure TSO flags */
	fm10k_write_reg(hw, FM10K_DTXTCPFLGL, FM10K_TSO_FLAGS_LOW);
	fm10k_write_reg(hw, FM10K_DTXTCPFLGH, FM10K_TSO_FLAGS_HI);

	/* Enable DMA engine
	 * Set Rx Descriptor size to 32
	 * Set Minimum MSS to 64
	 * Set Maximum number of Rx queues to 256 / 32 Descriptor
	 */
	dma_ctrl |= FM10K_DMA_CTRL_TX_ENABLE | FM10K_DMA_CTRL_RX_ENABLE |
		    FM10K_DMA_CTRL_RX_DESC_SIZE | FM10K_DMA_CTRL_MINMSS_64 |
		    FM10K_DMA_CTRL_32_DESC;

	fm10k_write_reg(hw, FM10K_DMA_CTRL, dma_ctrl);

	return 0;
}

int ufp_fm10k_set_device_params(struct ufp_handle *ih,
	uint32_t num_qps_req, uint32_t num_rx_desc, uint32_t num_tx_desc)
{
	ih->num_qps = min(num_qps_req, FM10K_MAX_RSS_INDICES);

	if(num_rx_desc > FM10K_MAX_TXD)
		ih->num_rx_desc = FM10K_MAX_TXD;
	else if(num_rx_desc < FM10K_MIN_TXD)
		ih->num_rx_desc = FM10K_MIN_TXD;
	else
		ih->num_rx_desc = num_rx_desc;

	if(num_tx_desc > FM10K_MAX_TXD)
		ih->num_tx_desc = FM10K_MAX_TXD;
	else if(num_tx_desc < FM10K_MAX_TXD)
		ih->num_tx_desc = FM10K_MIN_TXD;
	else
		ih->num_tx_desc = num_tx_desc;

	ih->size_rx_desc = sizeof(union ufp_ixgbevf_rx_desc) * ih->num_rx_desc;
	ih->size_tx_desc = sizeof(union ufp_ixgbevf_tx_desc) * ih->num_tx_desc;

	ih->buf_size = FM10K_RX_BUFSZ;

	return 0;
}

void fm10k_set_dma_mask(struct fm10k_hw *hw, uint64_t dma_mask)
{
	/* we need to write the upper 32 bits of DMA mask to PhyAddrSpace */
	uint32_t phyaddr = (uint32_t)(dma_mask >> 32);

	fm10k_write_reg(hw, FM10K_PHYADDR, phyaddr);
}

int fm10k_get_bus_info(struct ufp_handle *ih)
{
	u16 link_cap, link_status, device_cap, device_control;

	/* Get the maximum link width and speed from PCIe config space */
	link_cap = fm10k_read_pci_cfg_word(hw, FM10K_PCIE_LINK_CAP);

	switch (link_cap & FM10K_PCIE_LINK_WIDTH) {
	case FM10K_PCIE_LINK_WIDTH_1:
		hw->bus_caps.width = fm10k_bus_width_pcie_x1;
		break;
	case FM10K_PCIE_LINK_WIDTH_2:
		hw->bus_caps.width = fm10k_bus_width_pcie_x2;
		break;
	case FM10K_PCIE_LINK_WIDTH_4:
		hw->bus_caps.width = fm10k_bus_width_pcie_x4;
		break;
	case FM10K_PCIE_LINK_WIDTH_8:
		hw->bus_caps.width = fm10k_bus_width_pcie_x8;
		break;
	default:
		hw->bus_caps.width = fm10k_bus_width_unknown;
		break;
	}

	switch (link_cap & FM10K_PCIE_LINK_SPEED) {
	case FM10K_PCIE_LINK_SPEED_2500:
		hw->bus_caps.speed = fm10k_bus_speed_2500;
		break;
	case FM10K_PCIE_LINK_SPEED_5000:
		hw->bus_caps.speed = fm10k_bus_speed_5000;
		break;
	case FM10K_PCIE_LINK_SPEED_8000:
		hw->bus_caps.speed = fm10k_bus_speed_8000;
		break;
	default:
		hw->bus_caps.speed = fm10k_bus_speed_unknown;
		break;
	}

	/* Get the PCIe maximum payload size for the PCIe function */
	device_cap = fm10k_read_pci_cfg_word(hw, FM10K_PCIE_DEV_CAP);

	switch (device_cap & FM10K_PCIE_DEV_CAP_PAYLOAD) {
	case FM10K_PCIE_DEV_CAP_PAYLOAD_128:
		hw->bus_caps.payload = fm10k_bus_payload_128;
		break;
	case FM10K_PCIE_DEV_CAP_PAYLOAD_256:
		hw->bus_caps.payload = fm10k_bus_payload_256;
		break;
	case FM10K_PCIE_DEV_CAP_PAYLOAD_512:
		hw->bus_caps.payload = fm10k_bus_payload_512;
		break;
	default:
		hw->bus_caps.payload = fm10k_bus_payload_unknown;
		break;
	}

	/* Get the negotiated link width and speed from PCIe config space */
	link_status = fm10k_read_pci_cfg_word(hw, FM10K_PCIE_LINK_STATUS);

	switch (link_status & FM10K_PCIE_LINK_WIDTH) {
	case FM10K_PCIE_LINK_WIDTH_1:
		hw->bus.width = fm10k_bus_width_pcie_x1;
		break;
	case FM10K_PCIE_LINK_WIDTH_2:
		hw->bus.width = fm10k_bus_width_pcie_x2;
		break;
	case FM10K_PCIE_LINK_WIDTH_4:
		hw->bus.width = fm10k_bus_width_pcie_x4;
		break;
	case FM10K_PCIE_LINK_WIDTH_8:
		hw->bus.width = fm10k_bus_width_pcie_x8;
		break;
	default:
		hw->bus.width = fm10k_bus_width_unknown;
		break;
	}

	switch (link_status & FM10K_PCIE_LINK_SPEED) {
	case FM10K_PCIE_LINK_SPEED_2500:
		hw->bus.speed = fm10k_bus_speed_2500;
		break;
	case FM10K_PCIE_LINK_SPEED_5000:
		hw->bus.speed = fm10k_bus_speed_5000;
		break;
	case FM10K_PCIE_LINK_SPEED_8000:
		hw->bus.speed = fm10k_bus_speed_8000;
		break;
	default:
		hw->bus.speed = fm10k_bus_speed_unknown;
		break;
	}

	/* Get the negotiated PCIe maximum payload size for the PCIe function */
	device_control = fm10k_read_pci_cfg_word(hw, FM10K_PCIE_DEV_CTRL);

	switch (device_control & FM10K_PCIE_DEV_CTRL_PAYLOAD) {
	case FM10K_PCIE_DEV_CTRL_PAYLOAD_128:
		hw->bus.payload = fm10k_bus_payload_128;
		break;
	case FM10K_PCIE_DEV_CTRL_PAYLOAD_256:
		hw->bus.payload = fm10k_bus_payload_256;
		break;
	case FM10K_PCIE_DEV_CTRL_PAYLOAD_512:
		hw->bus.payload = fm10k_bus_payload_512;
		break;
	default:
		hw->bus.payload = fm10k_bus_payload_unknown;
		break;
	}

	return 0;
}

void fm10k_update_hw_stats(struct fm10k_hw *hw,
				     struct fm10k_hw_stats *stats)
{
	u32 timeout, ur, ca, um, xec, vlan_drop, loopback_drop, nodesc_drop;
	u32 id, id_prev;

	/* Use Tx queue 0 as a canary to detect a reset */
	id = fm10k_read_reg(hw, FM10K_TXQCTL(0));

	/* Read Global Statistics */
	do {
		timeout = fm10k_read_hw_stats_32b(hw, FM10K_STATS_TIMEOUT,
						  &stats->timeout);
		ur = fm10k_read_hw_stats_32b(hw, FM10K_STATS_UR, &stats->ur);
		ca = fm10k_read_hw_stats_32b(hw, FM10K_STATS_CA, &stats->ca);
		um = fm10k_read_hw_stats_32b(hw, FM10K_STATS_UM, &stats->um);
		xec = fm10k_read_hw_stats_32b(hw, FM10K_STATS_XEC, &stats->xec);
		vlan_drop = fm10k_read_hw_stats_32b(hw, FM10K_STATS_VLAN_DROP,
						    &stats->vlan_drop);
		loopback_drop =
			fm10k_read_hw_stats_32b(hw,
						FM10K_STATS_LOOPBACK_DROP,
						&stats->loopback_drop);
		nodesc_drop = fm10k_read_hw_stats_32b(hw,
						      FM10K_STATS_NODESC_DROP,
						      &stats->nodesc_drop);

		/* if value has not changed then we have consistent data */
		id_prev = id;
		id = fm10k_read_reg(hw, FM10K_TXQCTL(0));
	} while ((id ^ id_prev) & FM10K_TXQCTL_ID_MASK);

	/* drop non-ID bits and set VALID ID bit */
	id &= FM10K_TXQCTL_ID_MASK;
	id |= FM10K_STAT_VALID;

	/* Update Global Statistics */
	if (stats->stats_idx == id) {
		stats->timeout.count += timeout;
		stats->ur.count += ur;
		stats->ca.count += ca;
		stats->um.count += um;
		stats->xec.count += xec;
		stats->vlan_drop.count += vlan_drop;
		stats->loopback_drop.count += loopback_drop;
		stats->nodesc_drop.count += nodesc_drop;
	}       
	
	/* Update bases and record current PF id */
	fm10k_update_hw_base_32b(&stats->timeout, timeout);
	fm10k_update_hw_base_32b(&stats->ur, ur); 
	fm10k_update_hw_base_32b(&stats->ca, ca);
	fm10k_update_hw_base_32b(&stats->um, um);
	fm10k_update_hw_base_32b(&stats->xec, xec);
	fm10k_update_hw_base_32b(&stats->vlan_drop, vlan_drop);
	fm10k_update_hw_base_32b(&stats->loopback_drop, loopback_drop);
	fm10k_update_hw_base_32b(&stats->nodesc_drop, nodesc_drop);
	stats->stats_idx = id;
	
	/* Update Queue Statistics */
	fm10k_update_hw_stats_q(hw, stats->q, 0, hw->mac.max_queues);
}       

int fm10k_read_mac_addr(struct ufp_handle *ih)
{
	u8 perm_addr[ETH_ALEN];
	u32 serial_num;

	serial_num = fm10k_read_reg(hw, FM10K_SM_AREA(1));

	/* last byte should be all 1's */
	if ((~serial_num) << 24)
		return  FM10K_ERR_INVALID_MAC_ADDR;

	ih->mac_addr[0] = (u8)(serial_num >> 24);
	ih->mac_addr[1] = (u8)(serial_num >> 16);
	ih->mac_addr[2] = (u8)(serial_num >> 8);

	serial_num = fm10k_read_reg(hw, FM10K_SM_AREA(0));

	/* first byte should be all 1's */
	if ((~serial_num) >> 24)
		return  FM10K_ERR_INVALID_MAC_ADDR;

	ih->mac_addr[3] = (u8)(serial_num >> 16);
	ih->mac_addr[4] = (u8)(serial_num >> 8);
	ih->mac_addr[5] = (u8)(serial_num);

	return 0;
}

