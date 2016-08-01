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

	data->mbx_timeout	= IXGBE_VF_INIT_TIMEOUT;
	data->mbx_udelay	= IXGBE_VF_MBX_INIT_DELAY;
	data->mbx_size		= IXGBE_VFMAILBOX_SIZE;

	/* Configuration related functions*/
	ops->reset_hw		= i40e_reset_hw;
	ops->set_device_params	= ufp_i40e_set_device_params;
	ops->configure_irq	= ufp_i40e_configure_irq;
	ops->configure_tx	= ufp_i40e_configure_tx;
	ops->configure_rx	= ufp_i40e_configure_rx;
	ops->stop_adapter	= ufp_i40e_stop_adapter;

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

