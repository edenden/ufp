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
JUST MEMO
static const struct fm10k_mac_ops mac_ops_pf = {
	.get_bus_info	   = fm10k_get_bus_info_generic,
	.reset_hw	       = fm10k_reset_hw_pf,
	.init_hw		= fm10k_init_hw_pf,
	.start_hw	       = fm10k_start_hw_generic,
	.stop_hw		= fm10k_stop_hw_generic,
	.update_vlan	    = fm10k_update_vlan_pf,
	.read_mac_addr	  = fm10k_read_mac_addr_pf,
	.update_uc_addr	 = fm10k_update_uc_addr_pf,
	.update_mc_addr	 = fm10k_update_mc_addr_pf,
	.update_xcast_mode      = fm10k_update_xcast_mode_pf,
	.update_int_moderator   = fm10k_update_int_moderator_pf,
	.update_lport_state     = fm10k_update_lport_state_pf,
	.update_hw_stats	= fm10k_update_hw_stats_pf,
	.rebind_hw_stats	= fm10k_rebind_hw_stats_pf,
	.configure_dglort_map   = fm10k_configure_dglort_map_pf,
	.set_dma_mask	   = fm10k_set_dma_mask_pf,
	.get_fault	      = fm10k_get_fault_pf,
	.get_host_state	 = fm10k_get_host_state_pf,
	.request_lport_map      = fm10k_request_lport_map_pf,
};
*/

int ufp_fm10k_init(struct ufp_ops *ops)
{
	struct ufp_fm10k_data *data;

	data = malloc(sizeof(struct ufp_fm10k_data));
	if(!data)
		goto err_alloc_data;

	ops->data		= data;

	/* Configuration related functions*/
	ops->open		= ufp_fm10k_open;
	ops->up			= ufp_fm10k_up;
	ops->down		= ufp_fm10k_down;
	ops->close		= ufp_fm10k_close;

	/* RxTx related functions */
	ops->unmask_queues	= ufp_fm10k_unmask_queues;
	ops->set_rx_desc	= ufp_fm10k_rx_desc_set;
	ops->check_rx_desc	= ufp_fm10k_rx_desc_check;
	ops->get_rx_desc	= ufp_fm10k_rx_desc_get;
	ops->set_tx_desc	= ufp_fm10k_tx_desc_set;
	ops->check_tx_desc	= ufp_fm10k_tx_desc_check;

	return 0;

err_alloc_data:
	return -1;
}

void ufp_fm10k_destroy(struct ufp_ops *ops)
{
	free(ops->data);
	return;
}

int ufp_fm10k_open(struct ufp_handle *ih)
{
	int err;
	struct ufp_fm10k_data *data = ih->ops->data;

        /* pick up the PCIe bus settings for reporting later */
        if (hw->mac.ops.get_bus_info)
                hw->mac.ops.get_bus_info(hw);

        /* limit the usable DMA range */
        if (hw->mac.ops.set_dma_mask)
                hw->mac.ops.set_dma_mask(hw, dma_get_mask(&pdev->dev));

	err = ufp_fm10k_reset_hw(ih);
	if(err < 0)
		goto err_reset_hw;

	err = ufp_fm10k_init_hw(ih);
	if(err < 0)
		goto err_init_hw;

	err = ufp_fm10k_set_device_params(ih);
	if(err < 0)
		goto err_set_device_params;

	/* initialize hardware statistics */
	hw->mac.ops.update_hw_stats(hw, &interface->stats);

        /* Initialize MAC address from hardware */
        err = ufp_fm10k_read_mac_addr(ih);
        if(err < 0)
		goto err_read_mac_addr;

	return 0;

err_read_mac_addr:
err_set_device_params:
err_init_hw:
err_reset_hw:
	return -1;
}

int ufp_fm10k_up(struct ufp_handle *ih)
{
	/* configure msix on device side */
	ufp_fm10k_configure_irq(ih);

        /* configure Tx descriptor rings */
        ufp_fm10k_configure_tx(interface);

        /* configure Rx descriptor rings */
        ufp_fm10k_configure_rx(interface);

        /* configure interrupts moderator */
        ufp_fm10k_update_int_moderator(hw);

        /* re-establish Rx filters */
        fm10k_restore_rx_state(interface);

	return 0;
}

int ufp_fm10k_down(struct ufp_handle *ih)
{

	return 0;
}

int fm10k_close(struct net_device *netdev)
{
        struct fm10k_intfc *interface = netdev_priv(netdev);

        fm10k_down(interface);

        fm10k_qv_free_irq(interface);

        fm10k_free_udp_port_info(interface);

        fm10k_free_all_tx_resources(interface);
        fm10k_free_all_rx_resources(interface);

        return 0;
}

int ufp_fm10k_close(struct ufp_handle *ih)
{
	return 0;
}

