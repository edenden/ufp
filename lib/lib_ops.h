#ifndef _LIBUFP_OPS_H
#define _LIBUFP_OPS_H

struct ufp_ops {
	int		(*reset_hw)(struct ufp_handle *);
	int		(*set_device_params)(struct ufp_handle *,
				uint32, uint32, uint32);
	int		(*configure_irq)(struct ufp_handle *,
				uint32);
	int		(*configure_tx)(struct ufp_handle *);
	int		(*configure_rx)(struct ufp_handle *,
				uint32, uint32);
	int		(*stop_adapter)(struct ufp_handle *);

	void		*data;
	uint16_t	device_id;
};

#endif /* _LIBUFP_OPS_H */
