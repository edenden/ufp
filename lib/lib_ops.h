#ifndef _LIBUFP_OPS_H
#define _LIBUFP_OPS_H

struct ufp_ops {
	int		(*reset_hw)(struct ufp_handle *);
	int		(*get_queues)(struct ufp_handle *);
	int		(*get_bufsize)(struct ufp_handle *);
	int		(*irq_configure)(struct ufp_handle *);
	int		(*configure_tx)(struct ufp_handle *);
	int		(*configure_rx)(struct ufp_handle *);

	void		*data;
	uint16_t	device_id;
};

#endif /* _LIBUFP_OPS_H */
