#ifndef _LIBUFP_OPS_H
#define _LIBUFP_OPS_H

struct ufp_ops {
	/* For configuration */
	int		(*reset_hw)(struct ufp_handle *);
	int		(*set_device_params)(struct ufp_handle *,
				uint32, uint32, uint32);
	int		(*configure_irq)(struct ufp_handle *,
				uint32);
	int		(*configure_tx)(struct ufp_handle *);
	int		(*configure_rx)(struct ufp_handle *,
				uint32, uint32);
	int		(*stop_adapter)(struct ufp_handle *);

	/* For forwarding */
	void		(*set_rx_desc)(struct ufp_ring *, uint16_t,
				uint64_t);
	int		(*check_rx_desc)(struct ufp_ring *, uint16_t);
	unsigned int	(*get_rx_desc)(struct ufp_ring *, uint16_t);
	void		(*set_tx_desc)(struct ufp_ring *, uint16_t,
				uint64_t, uint32_t);
	int		(*check_tx_desc)(struct ufp_ring *, uint16_t);

	void		*data;
	uint16_t	device_id;
};

#endif /* _LIBUFP_OPS_H */
