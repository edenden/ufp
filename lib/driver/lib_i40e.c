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
	i40e_clear_hw(ih);

	err = i40e_reset_hw(ih);
	if(err < 0)
		goto err_reset_hw;

        err = i40e_init_shared_code(ih);
        if(err < 0)
                goto err_init_shared_code;

        err = err = i40e_init_adminq(hw);
        if(err < 0)
                goto err_init_adminq;

        i40e_verify_eeprom(pf);

        i40e_clear_pxe_mode(ih);

        err = i40e_get_capabilities(pf);
        if (err)
                goto err_adminq_setup;

        err = i40e_sw_init(pf);
        if (err) {
                dev_info(&pdev->dev, "sw_init failed: %d\n", err);
                goto err_sw_init;
        }

        return 0;

err_init_adminq:
err_init_shared_code:
err_reset_hw;
        return -1;
}

int ufp_i40e_up(struct ufp_handle *ih)
{

}

int ufp_i40e_down(struct ufp_handle *ih)
{

}

int ufp_i40e_close(struct ufp_handle *ih)
{
	return 0;
}
