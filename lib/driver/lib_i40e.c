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

int ufp_i40e_init(struct ufp_dev *dev, struct ufp_ops *ops)
{
	struct ufp_i40e_dev *i40e_dev;

	i40e_dev = malloc(sizeof(struct ufp_i40e_dev));
	if(!i40e_dev)
		goto err_alloc_drv_data;

	dev->drv_data = i40e_dev;

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

err_alloc_drv_data:
	return -1;
}

void ufp_i40e_destroy(struct ufp_dev *dev, struct ufp_ops *ops)
{
	free(dev->drv_data);
	return;
}

int ufp_i40e_open(struct ufp_dev *dev)
{
	struct ufp_i40e_dev *i40e_dev = dev->drv_data;
	struct ufp_irq_handle *irqh;
	unsigned long read_buf;

	i40e_set_mac_type(dev);

	switch (i40e_dev->mac_type) {
	case I40E_MAC_XL710:
	case I40E_MAC_X722:
		break;
	default:
		goto err_not_supported;
	}

	i40e_clear_hw(dev);

	err = i40e_reset_hw(dev);
	if(err < 0)
		goto err_reset_hw;

	/* adminQ related initialization and depending process */
	err = ufp_i40e_aq_init(dev);
	if(err < 0)
		goto err_init_adminq;

	err = i40e_aq_clear_pxe_mode(dev);
	if(err < 0)
		goto err_clear_pxe;

	/* Disable LLDP for NICs that have firmware versions lower than v4.3.
	 * Ignore error return codes because if it was already disabled via
	 * hardware settings this will fail
	 */
	i40e_aq_stop_lldp(hw, true, NULL);

	i40e_get_mac_addr(hw, hw->mac.addr);

	irqh = data->aq_tx_irqh;
	while(!(data & AQ_MAC_ADDR) | !(data & AQ_CLEAR_PXE)){
		err = read(irqh->fd, &read_buf, sizeof(unsigned long));
		if(err < 0)
			goto err_read;

		i40e_aq_asq_clean(dev);
	}

	err = i40e_setup_pf_switch(dev);
	if (err)
		goto err_setup_switch;

	/* The driver only wants link up/down and module qualification
	 * reports from firmware.  Note the negative logic.
	 */
	err = i40e_aq_set_phy_int_mask(&pf->hw,
		~(I40E_AQ_EVENT_LINK_UPDOWN |
		I40E_AQ_EVENT_MEDIA_NA |
		I40E_AQ_EVENT_MODULE_QUAL_FAIL), NULL);
	if(err < 0)
		goto err_aq_set_phy_int_mask;

	err = ufp_i40e_hmc_init(dev);
	if(err < 0)
		goto err_hmc_init;

	return 0;

err_read:
err_clear_pxe:
err_init_adminq:
err_reset_hw;
err_not_supported:
	return -1;
}

int ufp_i40e_up(struct ufp_dev *dev)
{
	struct ufp_iface *iface;

	iface = dev->iface;
	while(iface){
		err = i40e_vsi_configure_tx(iface);
		if(err < 0)
			goto err_configure_tx;

		err = i40e_vsi_configure_rx(iface);
		if(err < 0)
			goto err_configure_rx;

		err = i40e_up_complete(iface);
		if (err)
			goto err_up_complete;

		iface = iface->next;
	}
	return 0;
}

int ufp_i40e_down(struct ufp_dev *dev)
{

}

int ufp_i40e_close(struct ufp_dev *dev)
{
	ufp_i40e_hmc_destroy(dev);
	ufp_i40e_aq_destroy(dev);
	return 0;
}
