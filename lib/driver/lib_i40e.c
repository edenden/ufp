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
	ops->unmask_queues	= i40e_update_enable_itr;
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
	int err;

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

	err = i40e_configure_pf(dev);
	if(err)
		goto err_configure_pf;

	err = i40e_setup_pf_switch(dev);
	if (err)
		goto err_setup_switch;

	err = ufp_i40e_hmc_init(dev);
	if(err < 0)
		goto err_hmc_init;

	return 0;

err_hmc_init:
err_setup_switch:
	ufp_i40e_aq_destroy(dev);
err_configure_pf:
	return -1;
}

int ufp_i40e_close(struct ufp_dev *dev)
{
	ufp_i40e_hmc_destroy(dev);
	ufp_i40e_aq_destroy(dev);

	return 0;
}

int ufp_i40e_up(struct ufp_dev *dev)
{
	struct ufp_iface *iface;

	iface = dev->iface;
	while(iface){
		err = i40e_vsi_promisc_mode(dev, iface);
		if(err < 0)
			goto err_configure_filter;

		err = i40e_vsi_configure_rx(iface);
		if(err < 0)
			goto err_configure_rx;

		err = i40e_vsi_configure_tx(iface);
		if(err < 0)
			goto err_configure_tx;

		err = i40e_vsi_configure_irq(iface);
		if(err < 0)
			goto err_configure_irq;

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
err_configure_irq:
err_configure_tx:
err_configure_rx:
err_configure_filter:
		goto err_up;
	}

	err = i40e_setup_misc_irq(dev);
	if(err < 0)
		goto err_setup_misc_irq;
	i40e_start_misc_irq(dev);
	return 0;

err_setup_misc_irq:
err_up:
	return -1;
}

int ufp_i40e_down(struct ufp_dev *dev)
{
	struct ufp_iface *iface;

	i40e_stop_misc_irq(dev);
	i40e_shutdown_misc_irq(dev);

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

		err = i40e_vsi_shutdown_irq(dev, iface);
		if(err < 0)
			goto err_shutdown_irq;

		iface = iface->next;
		continue;

err_shutdown_irq:
err_stop_rx:
err_stop_tx:
err_stop_irq:
		goto err_down;
	}

	return 0;

err_down:
	return -1;
}

