#ifndef _IXMAPFWD_THREAD_H
#define _IXMAPFWD_THREAD_H

#include <pthread.h>
#include <ufp.h>

#include "iftap.h"
#include "neigh.h"
#include "fib.h"

struct ufpd_thread {
	struct ufp_plane	*plane;
	struct ufp_buf		*buf;
	struct ufp_desc		*desc;
	struct neigh_table	**neigh_inet;
	struct neigh_table	**neigh_inet6;
	struct fib		*fib_inet;
	struct fib		*fib_inet6;
	struct tun_plane	*tun_plane;
	unsigned int		id;
	pthread_t		tid;
	pthread_t		ptid;
	unsigned int		num_ports;
};

void *thread_process_interrupt(void *data);

#endif /* _IXMAPFWD_THREAD_H */
