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

int ufp_i40e_init(struct ufp_ops *ops)
{
	struct ufp_i40e_data *data;

	data = malloc(sizeof(struct ufp_i40e_data));
	if(!data)
		goto err_alloc_data;

	ops->data		= data;

	/* Configuration related functions*/
	ops->open		= ufp_i40e_open;
	ops->up			= ufp_i40e_up;
	ops->down		= ufp_i40e_down;
	ops->close		= ufp_i40e_close;

	/* RxTx related functions */
	ops->unmask_queues	= ufp_i40e_unmask_queues;
	ops->set_rx_desc	= ufp_i40e_rx_desc_set;
	ops->check_rx_desc	= ufp_i40e_rx_desc_check;
	ops->get_rx_desc	= ufp_i40e_rx_desc_get;
	ops->set_tx_desc	= ufp_i40e_tx_desc_set;
	ops->check_tx_desc	= ufp_i40e_tx_desc_check;

	return 0;

err_alloc_data:
	return -1;
}

void ufp_i40e_destroy(struct ufp_ops *ops)
{
	free(ops->data);
	return;
}

int ufp_i40e_open(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;
	struct ufp_irq_handle *irqh;
	unsigned long read_buf;

	i40e_set_mac_type(ih);
        
	switch (data->mac_type) {
	case I40E_MAC_XL710:
	case I40E_MAC_X722:
		break;
	default:
		goto err_not_supported;
	}

	if (data->mac_type == I40E_MAC_X722)
		hw->flags |= I40E_HW_FLAG_AQ_SRCTL_ACCESS_ENABLE;

	i40e_clear_hw(ih);

	err = i40e_reset_hw(ih);
	if(err < 0)
		goto err_reset_hw;

	/* NVM related initalization and depending process */
	err = i40e_init_nvm(hw);
	if(err < 0)
		goto err_init_nvm;

	err = i40e_diag_eeprom_test(hw);
	if(err < 0)
		goto err_eeprom_test;

	/* adminQ related initialization and depending process */
	err = i40e_init_adminq(hw);
	if(err < 0)
		goto err_init_adminq;

	err = i40e_aq_clear_pxe_mode(hw, NULL);
	if(err < 0)
		goto err_clear_pxe;

	err = i40e_get_capabilities(pf);
	if (err)
		goto err_adminq_setup;

	i40e_aq_send_driver_version(&pf->hw, &dv, NULL);

	irqh = data->aq_tx_irqh;
	while(!(data & AQ_XXX) | !(data & AQ_YYY)){
		err = read(irqh->fd, &read_buf, sizeof(unsigned long));
		if(err < 0)
			goto err_read;

		i40e_aq_asq_clean(ih);
	}

err_clear_pxe:
err_init_adminq:
err_eeprom_test:
err_init_nvm:
err_reset_hw;
err_not_supported:
	return -1;
}

int ufp_i40e_up(struct ufp_handle *ih)
{
	err = i40e_init_lan_hmc(hw, hw->func_caps.num_tx_qp,
				hw->func_caps.num_rx_qp,
				pf->fcoe_hmc_cntx_num, pf->fcoe_hmc_filt_num);
	if (err) {
		dev_info(&pdev->dev, "init_lan_hmc failed: %d\n", err);
		goto err_init_lan_hmc;
	}

	err = i40e_configure_lan_hmc(hw, I40E_HMC_MODEL_DIRECT_ONLY);
	if (err) {
		dev_info(&pdev->dev, "configure_lan_hmc failed: %d\n", err);
		err = -ENOENT;
		goto err_configure_lan_hmc;
	}

	/* Disable LLDP for NICs that have firmware versions lower than v4.3.
	 * Ignore error return codes because if it was already disabled via
	 * hardware settings this will fail
	 */
	i40e_aq_stop_lldp(hw, true, NULL);

	i40e_get_mac_addr(hw, hw->mac.addr);
	if (!is_valid_ether_addr(hw->mac.addr)) {
		dev_info(&pdev->dev, "invalid MAC address %pM\n", hw->mac.addr);
		err = -EIO;
		goto err_mac_addr;
	}

	err = i40e_init_interrupt_scheme(pf);
	if (err)
		goto err_switch_setup;

	err = i40e_setup_pf_switch(pf, false);
	if (err) {
		dev_info(&pdev->dev, "setup_pf_switch failed: %d\n", err);
		goto err_vsis;
	}

	/* The driver only wants link up/down and module qualification
	 * reports from firmware.  Note the negative logic.
	 */
	err = i40e_aq_set_phy_int_mask(&pf->hw,
				       ~(I40E_AQ_EVENT_LINK_UPDOWN |
					 I40E_AQ_EVENT_MEDIA_NA |
					 I40E_AQ_EVENT_MODULE_QUAL_FAIL), NULL);
	if (err)
		dev_info(&pf->pdev->dev, "set phy mask fail, err %s aq_err %s\n",
			 i40e_stat_str(&pf->hw, err),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));

	if (pf->flags & I40E_FLAG_RESTART_AUTONEG) {
		msleep(75);
		err = i40e_aq_set_link_restart_an(&pf->hw, true, NULL);
		if (err)
			dev_info(&pf->pdev->dev, "link restart failed, err %s aq_err %s\n",
				 i40e_stat_str(&pf->hw, err),
				 i40e_aq_str(&pf->hw,
					      pf->hw.aq.asq_last_status));
	}

	/* get the requested speeds from the fw */
	err = i40e_aq_get_phy_capabilities(hw, false, false, &abilities, NULL);
	if (err)
		dev_dbg(&pf->pdev->dev, "get requested speeds ret =  %s last_status =  %s\n",
			i40e_stat_str(&pf->hw, err),
			i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
	pf->hw.phy.link_info.requested_speeds = abilities.link_speed;

	/* get the supported phy types from the fw */
	err = i40e_aq_get_phy_capabilities(hw, false, true, &abilities, NULL);
	if (err)
		dev_dbg(&pf->pdev->dev, "get supported phy types ret =  %s last_status =  %s\n",
			i40e_stat_str(&pf->hw, err),
			i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
	pf->hw.phy.phy_types = LE32_TO_CPU(abilities.phy_type);

	if ((pf->hw.device_id == I40E_DEV_ID_10G_BASE_T) ||
	    (pf->hw.device_id == I40E_DEV_ID_10G_BASE_T4))
		pf->flags |= I40E_FLAG_HAVE_10GBASET_PHY;

	return 0;
}

int ufp_i40e_down(struct ufp_handle *ih)
{

}

int ufp_i40e_close(struct ufp_handle *ih)
{
	return 0;
}
