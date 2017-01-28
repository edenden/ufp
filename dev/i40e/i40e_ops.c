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
#include "i40e_aq.h"
#include "i40e_hmc.h"
#include "i40e_io.h"
#include "i40e_ops.h"

static int i40e_ops_init(struct ufp_dev *dev, struct ufp_ops *ops);
static void i40e_ops_destroy(struct ufp_dev *dev, struct ufp_ops *ops);
static int i40e_ops_open(struct ufp_dev *dev);
static int i40e_ops_close(struct ufp_dev *dev);
static int i40e_ops_up(struct ufp_dev *dev);
static int i40e_ops_down(struct ufp_dev *dev);

static const struct pci_id device_pci_tbl[] = {
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_SFP_XL710},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QEMU},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_B},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_C},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_A},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_B},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_C},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T4},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_20G_KR2},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_20G_KR2_A},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_X722},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_X722},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_SFP_X722},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_1G_BASE_T_X722},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T_X722},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_SFP_I_X722},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_I_X722},
	/* required last entry */
	{0, }
};

static struct pci_driver driver = {
	.id_table = device_pci_tbl,
	.init = i40e_ops_init,
	.destroy = i40e_ops_destroy
};

static int i40e_ops_init(struct ufp_dev *dev, struct ufp_ops *ops)
{
	struct i40e_dev *i40e_dev;

	i40e_dev = malloc(sizeof(struct i40e_dev));
	if(!i40e_dev)
		goto err_alloc_drv_data;

	list_init(&i40e_dev->elem);
	dev->drv_data = i40e_dev;

	/* Configuration related functions*/
	ops->open		= i40e_ops_open;
	ops->up			= i40e_ops_up;
	ops->down		= i40e_ops_down;
	ops->close		= i40e_ops_close;

	/* RxTx related functions */
	ops->unmask_queues	= i40e_update_enable_itr;
	ops->fill_rx_desc	= i40e_rx_desc_fill;
	ops->fetch_rx_desc	= i40e_rx_desc_fetch;
	ops->fill_tx_desc	= i40e_tx_desc_fill;
	ops->fetch_tx_desc	= i40e_tx_desc_fetch;

	return 0;

err_alloc_drv_data:
	return -1;
}

static void i40e_ops_destroy(struct ufp_dev *dev, struct ufp_ops *ops)
{
	free(dev->drv_data);
	return;
}

static int i40e_ops_open(struct ufp_dev *dev)
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
	err = i40e_aq_init(dev);
	if(err < 0)
		goto err_init_adminq;

	err = i40e_configure_pf(dev);
	if(err)
		goto err_configure_pf;

	err = i40e_setup_pf_switch(dev);
	if (err)
		goto err_setup_switch;

	err = i40e_hmc_init(dev);
	if(err < 0)
		goto err_hmc_init;

	return 0;

err_hmc_init:
err_setup_switch:
	i40e_aq_destroy(dev);
err_configure_pf:
	return -1;
}

static int i40e_ops_close(struct ufp_dev *dev)
{
	i40e_hmc_destroy(dev);
	i40e_switchconf_clear(dev);
	i40e_aq_destroy(dev);

	return 0;
}

static int i40e_ops_up(struct ufp_dev *dev)
{
	struct ufp_iface *iface;
	int err;

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

static int i40e_ops_down(struct ufp_dev *dev)
{
	struct ufp_iface *iface;
	int err;

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

__attribute__((constructor))
static void i40e_ops_load()
{
	int err;

	err = ufp_dev_register(&driver);
	if(err)
		goto err_register;

	return;

err_register:
	/* TBD: Handle when registeration failed */
	return;
}

__attribute__((destructor))
static void i40e_ops_unload()
{
	ufp_dev_unregister(&driver);
}
