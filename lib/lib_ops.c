struct ufp_ops *ufp_ops_init(uint16_t device_id)
{
	struct ufp_ops *ops;
	int err;

	ops = malloc(sizeof(struct ufp_ops));
	if(!ops)
		goto err_alloc_ops;

	memset(ops, 0, sizeof(struct ufp_ops));

	switch(device_id){
	case IXGBE_DEV_ID_82599_VF:
	case IXGBE_DEV_ID_X540_VF:
	case IXGBE_DEV_ID_X550_VF:
	case IXGBE_DEV_ID_X550EM_X_VF:
		err = ufp_ixgbevf_init(ops);
		if(err)
			goto err_init_device;
		break;
	default:
		goto err_init_device;
	}

	ops->device_id = device_id;
	return ops;

err_init_device:
	free(ops);
err_alloc_ops:
	return NULL;
}

void ufp_ops_destroy(struct ufp_ops *ops)
{
	switch(ops->device_id){
	case IXGBE_DEV_ID_82599_VF:
	case IXGBE_DEV_ID_X540_VF:
	case IXGBE_DEV_ID_X550_VF:
	case IXGBE_DEV_ID_X550EM_X_VF:
		ufp_ixgbevf_destroy(ops);
                break;
	default:
		break;
	}

	free(ops);
	return;
}

int ufp_ops_reset_hw(struct ufp_handle *ih)
{
	return ih->ops->reset_hw(ih);
}

int ufp_ops_get_queues(struct ufp_handle *ih)
{
	return ih->ops->get_queues(ih);
}

int ufp_ops_get_intr_rate(struct ufp_handle *ih)
{
	return ih->ops->get_intr_rate(ih);
}

int ufp_ops_intr_configure(struct ufp_handle *ih)
{

}

int ufp_ops_rx_configure(struct ufp_handle *ih)
{

}

int ufp_ops_tx_configure(struct ufp_handle *ih)
{

}
