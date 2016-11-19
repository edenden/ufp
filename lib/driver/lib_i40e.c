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
	ops->fill_rx_desc	= ufp_i40e_rx_desc_fill;
	ops->fetch_rx_desc	= ufp_i40e_rx_desc_fetch;
	ops->fill_tx_desc	= ufp_i40e_tx_desc_fill;
	ops->fetch_tx_desc	= ufp_i40e_tx_desc_fetch;

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

	err = i40e_set_mac_type(dev);
	if(err < 0)
		goto err_mac_unknown;

	i40e_set_pf_id(dev);
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
	err = i40e_aqc_req_stop_lldp(dev);
	if(err < 0)
		goto err_stop_lldp;

	err = i40e_aqc_req_get_macaddr(dev);
	if(err < 0)
		goto err_get_macaddr;

	/* The driver only wants link up/down and module qualification
	 * reports from firmware.  Note the negative logic.
	 */
	err = i40e_aqc_req_set_phyintmask(dev,
		~(I40E_AQ_EVENT_LINK_UPDOWN |
		I40E_AQ_EVENT_MEDIA_NA |
		I40E_AQ_EVENT_MODULE_QUAL_FAIL));
	if(err < 0)
		goto err_aq_set_phy_int_mask;

	err = i40e_switchconf_fetch(dev);
	if(err < 0)
		goto err_switchconf_fetch;

	err = ufp_i40e_wait_cmd(dev);
	if(err < 0)
		goto err_wait_cmd;

	err = i40e_setup_pf_switch(dev);
	if (err)
		goto err_setup_switch;

	err = ufp_i40e_hmc_init(dev);
	if(err < 0)
		goto err_hmc_init;

	return 0;

err_clear_pxe:
	ufp_i40e_aq_destroy(dev);
err_init_adminq:
err_reset_hw:
err_mac_unknown:
	return -1;
}

int ufp_i40e_up(struct ufp_dev *dev)
{
	struct ufp_iface *iface;

	iface = dev->iface;
	while(iface){
		err = i40e_vsi_configure_filter(dev, iface);
		if(err < 0)
			goto err_configure_filter;

		err = i40e_vsi_configure_rx(iface);
		if(err < 0)
			goto err_configure_rx;

		err = i40e_vsi_configure_tx(iface);
		if(err < 0)
			goto err_configure_tx;

		err = i40e_vsi_start_rx(dev, iface);
		if(err < 0)
			goto err_start_rx;

		err = i40e_vsi_start_tx(dev, iface);
		if(err < 0)
			goto err_start_tx;

		err = i40e_vsi_start_irq(dev, iface);
		if(err < 0)
			goto err_start_irq;

		iface = iface->next;
		continue;

err_start_irq:
err_start_tx:
err_start_rx:
err_configure_tx:
err_configure_rx:
err_configure_filter:
		goto err_up;
	}

	dev->irqh = ufp_irq_open(dev, UFP_IRQ_MISC, 0, 0);
	if(!dev->irqh)
		goto err_open_irq;

	return 0;

err_open_irq:
err_up:
	return -1;
}

int ufp_i40e_down(struct ufp_dev *dev)
{
	struct ufp_iface *iface;

	ufp_irq_close(dev->irqh);

	iface = dev->iface;
	while(iface){
		err = i40e_vsi_stop_irq(dev, iface);
		if(err < 0)
			goto err_stop_irq;

		err = i40e_vsi_stop_tx(dev, iface);
		if(err < 0)
			goto err_stop_tx;

		err = i40e_vsi_stop_rx(dev, iface);
		if(err < 0)
			goto err_stop_rx;

		iface = iface->next;
		continue;

err_stop_rx:
err_stop_tx:
err_stop_irq:
		goto err_down;
	}

	return 0;

err_down:
	return -1;
}

int ufp_i40e_close(struct ufp_dev *dev)
{
	ufp_i40e_hmc_destroy(dev);
	ufp_i40e_aq_destroy(dev);

	return 0;
}
