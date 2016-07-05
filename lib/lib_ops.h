#ifndef _LIBUFP_OPS_H
#define _LIBUFP_OPS_H

struct ufp_ops {
	int		(*reset_hw)(struct ufp_handle *);
	int		(*stop_adapter)(struct ufp_handle *);
	int		(*negotiate_api)(struct ufp_hw *, uint32_t);
	int		(*get_queues)(struct ufp_hw *, uint32_t *, uint32_t *);
	int		(*update_xcast_mode)(struct ufp_hw *, int);
	int		(*set_rlpml)(struct ufp_hw *, uint16_t);

	void		*data;
	uint16_t	device_id;
};

#endif /* _LIBUFP_OPS_H */
