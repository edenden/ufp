#ifndef _IXMAPFWD_NETLINK_H
#define _IXMAPFWD_NETLINK_H

#include "main.h"

void netlink_process(struct ufpd_thread *thread,
	uint8_t *read_buf, int read_size);

#endif /* _IXMAPFWD_NETLINK_H */
