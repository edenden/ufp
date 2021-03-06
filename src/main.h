#ifndef _UFPD_MAIN_H
#define _UFPD_MAIN_H

#include <sys/socket.h>
#include <linux/if.h>
#include <ufp.h>
#include <config.h>

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#define prefetch(x)	__builtin_prefetch(x, 0)
#define prefetchw(x)	__builtin_prefetch(x, 1)

#define PROCESS_NAME "ufp"
#define SYSLOG_FACILITY LOG_DAEMON
#define UFPD_RX_BUDGET 1024
#define UFPD_TX_BUDGET 4096
#define UFPD_MAX_CORES 16
#define UFPD_MAX_ARGS 128
#define UFPD_MAX_ARGLEN 1024
#define UFPD_MAX_IFS 64

struct ufpd {
	struct ufp_dev		**devs;
	struct ufp_mpool	**mpools;
	unsigned int		num_threads;
	unsigned int		cores[UFPD_MAX_CORES];
	unsigned int		num_devices;
	char			*ifnames[UFPD_MAX_IFS];
	unsigned int		promisc;
	unsigned int		mtu_frame;
	unsigned int		buf_size;
	unsigned int		buf_count;
	unsigned int		numa_node;
};

void ufpd_log(int level, char *fmt, ...);
extern char *optarg;

#endif /* _UFPD_MAIN_H */
