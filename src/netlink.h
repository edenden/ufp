#ifndef _UFPD_NETLINK_H
#define _UFPD_NETLINK_H

#include "main.h"

void netlink_process(struct ufpd_thread *thread,
	uint8_t *read_buf, int read_size);

#endif /* _UFPD_NETLINK_H */
