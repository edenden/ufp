#ifndef _UFPD_THREAD_H
#define _UFPD_THREAD_H

#include <pthread.h>
#include <ufp.h>

#include "neigh.h"
#include "fib.h"

struct ufpd_thread {
	struct ufp_plane	*plane;
	struct ufp_mpool	*mpool;
	struct ufp_buf		*buf;
	struct neigh_table	**neigh_inet;
	struct neigh_table	**neigh_inet6;
	struct fib		*fib_inet;
	struct fib		*fib_inet6;
	unsigned int		id;
	pthread_t		tid;
	pthread_t		ptid;
	unsigned int		num_ports;
	uint8_t			*read_buf;
	size_t			read_size;
};

void *thread_process_interrupt(void *data);

#endif /* _UFPD_THREAD_H */
