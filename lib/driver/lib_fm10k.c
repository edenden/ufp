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
	struct ufp_fm10k_data *data = ih->ops->data;

	return 0;
}

int ufp_fm10k_up(struct ufp_handle *ih)
{

	return 0;
}

int ufp_i40e_down(struct ufp_handle *ih)
{

	return 0;
}

int ufp_i40e_close(struct ufp_handle *ih)
{
	return 0;
}
