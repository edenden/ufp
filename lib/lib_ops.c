struct ufp_ops *ufp_ops_init(uint16_t device_id)
{
	struct ufp_ops *ops;

	ops = malloc(sizeof(struct ufp_ops));
	if(!ops)
		goto err_alloc_ops;

	switch(device_id){
	case IXGBE_DEV_ID_82599_VF:
	case IXGBE_DEV_ID_X540_VF:
	case IXGBE_DEV_ID_X550_VF:
	case IXGBE_DEV_ID_X550EM_X_VF:
		ufp_ixgbevf_init(ops);
		break;
	default:
		goto err_unknown_device;
	}

	return dev;

err_unknown_device:
	free(ops);
err_alloc_ops:
	return NULL;
}

void ufp_ops_destroy(struct ufp_ops *ops)
{
	free(ops);
	return;
}

int ufp_ops_reset_hw(struct ufp_handle *ih, )
{
	return ih->ops->reset_hw(ih);
}


