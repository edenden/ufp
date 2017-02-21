#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <endian.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <linux/mempolicy.h>
#include <stdarg.h>
#include <syslog.h>
#include <ufp.h>

#include "main.h"
#include "thread.h"

static void usage();
static int ufpd_device_init(struct ufpd *ufpd, int dev_idx);
static void ufpd_device_destroy(struct ufpd *ufpd, int dev_idx);
static int ufpd_thread_create(struct ufpd *ufpd,
	struct ufpd_thread *thread, unsigned int thread_id,
	unsigned int core_id);
static void ufpd_thread_kill(struct ufpd_thread *thread);
static int ufpd_set_signal(sigset_t *sigset);
static int ufpd_set_mempolicy(unsigned int node);
static int ufpd_parse_args(struct ufpd *ufpd, int argc, char **argv);
static int ufpd_parse_range(const char *str, char *result,
	int max_len);
static int ufpd_parse_list(const char *str, void *result,
	int size_elem, const char *format, int max_count);

char *optarg;

static void usage()
{
	printf("\n");
	printf("Usage:\n");
	printf("  -c [cpulist] : CPU cores to use\n");
	printf("  -p [ifnamelist] : Interfaces to use\n");
	printf("  -n [n] : NUMA node (default=0)\n");
	printf("  -m [n] : MTU length (default=1522)\n");
	printf("  -b [n] : Number of packet buffer per port(default=8192)\n");
	printf("  -a : Promiscuous mode (default=disabled)\n");
	printf("  -h : Show this help\n");
	printf("\n");
	return;
}

int main(int argc, char **argv)
{
	struct ufpd		ufpd;
	struct ufpd_thread	*threads;
	int			err, ret, i, signal;
	int			threads_done = 0,
				devices_done = 0,
				mpool_done = 0;
	sigset_t		sigset;

	/* set default values */
	ufpd.numa_node		= 0;
	ufpd.num_threads	= 0;
	ufpd.num_devices	= 0;
	ufpd.promisc		= 0;
	ufpd.mtu_frame		= 0; /* MTU=1522 is used by default. */
	ufpd.buf_count		= 8192; /* number of per port packet buffer */
	ufpd.buf_size		= 2048; /* must be larger than 2048 */

	err = ufpd_parse_args(&ufpd, argc, argv);
	if(err < 0){
		ret = -1;
		goto err_parse_args;
	}

	openlog(PROCESS_NAME, LOG_CONS | LOG_PID, SYSLOG_FACILITY);

	err = ufpd_set_mempolicy(ufpd.numa_node);
	if(err < 0){
		ret = -1;
		goto err_set_mempolicy;
	}

	ufpd.devs = malloc(sizeof(struct ufp_dev *) * ufpd.num_devices);
	if(!ufpd.devs){
		ret = -1;
		goto err_devs;
	}

	ufpd.mpools = malloc(sizeof(struct ufp_mpool *) * ufpd.num_threads);
	if(!ufpd.mpools){
		ret = -1;
		goto err_mpools;
	}

	threads = malloc(sizeof(struct ufpd_thread) * ufpd.num_threads);
	if(!threads){
		ret = -1;
		goto err_alloc_threads;
	}

	for(i = 0; i < ufpd.num_threads; i++, mpool_done++){
		ufpd.mpools[i] = ufp_mpool_init();
		if(!ufpd.mpools[i]){
			ret = -1;
			goto err_mempool_init;
		}
	}

	for(i = 0; i < ufpd.num_devices; i++, devices_done++){
		err = ufpd_device_init(&ufpd, i);
		if(err < 0){
			ret = -1;
			goto err_init_device;
		}
	}

	err = ufpd_set_signal(&sigset);
	if(err != 0){
		ret = -1;
		goto err_set_signal;
	}

	for(i = 0; i < ufpd.num_threads; i++, threads_done++){
		err = ufpd_thread_create(&ufpd, &threads[i], i, ufpd.cores[i]);
		if(err < 0){
			ret = -1;
			goto err_thread_create;
		}
	}

	while(1){
		if(sigwait(&sigset, &signal) == 0){
			break;
		}
	}
	ret = 0;

err_thread_create:
	for(i = 0; i < threads_done; i++){
		ufpd_thread_kill(&threads[i]);
	}
err_set_signal:
err_init_device:
	for(i = 0; i < devices_done; i++){
		ufpd_device_destroy(&ufpd, i);
	}
err_mempool_init:
	for(i = 0; i < mpool_done; i++){
		ufp_mpool_destroy(ufpd.mpools[i]);
	}
	free(threads);
err_alloc_threads:
	free(ufpd.mpools);
err_mpools:
	free(ufpd.devs);
err_devs:
err_set_mempolicy:
	closelog();
err_parse_args:
	return ret;
}

static int ufpd_device_init(struct ufpd *ufpd, int dev_idx)
{
	int err;

	ufpd->devs[dev_idx] = ufp_open(&ufpd->ifnames[dev_idx * IFNAMSIZ]);
	if(!ufpd->devs[dev_idx]){
		ufpd_log(LOG_ERR, "failed to ufp_open, idx = %d", dev_idx);
		goto err_open;
	}

	err = ufp_up(ufpd->devs[dev_idx], ufpd->mpools,
		ufpd->num_threads, ufpd->mtu_frame, ufpd->promisc,
		UFP_RX_BUDGET, UFP_TX_BUDGET);
	if(err < 0){
		ufpd_log(LOG_ERR, "failed to ufp_up, idx = %d", dev_idx);
		goto err_up;
	}

	return 0;

err_up:
	ufp_close(ufpd->devs[dev_idx]);
err_open:
	return -1;
}

static void ufpd_device_destroy(struct ufpd *ufpd, int dev_idx)
{
	ufp_down(ufpd->devs[dev_idx]);
	ufp_close(ufpd->devs[dev_idx]);
}

static int ufpd_thread_create(struct ufpd *ufpd,
	struct ufpd_thread *thread, unsigned int thread_id,
	unsigned int core_id)
{
	cpu_set_t cpuset;
	int err;

	thread->id		= thread_id;
	thread->num_ports	= ufp_portnum(thread->plane);
	thread->ptid		= pthread_self();
	thread->mpool		= ufpd->mpools[thread->id];

	thread->buf = ufp_alloc_buf(ufpd->devs, ufpd->num_devices,
			ufpd->buf_size, ufpd->buf_count, thread->mpool);
	if(!thread->buf){
		ufpd_log(LOG_ERR,
			"failed to ufp_alloc_buf, idx = %d", thread->id);
		goto err_buf_alloc;
	}

	thread->plane = ufp_plane_alloc(ufpd->devs, ufpd->num_devices,
		thread->buf, thread->id, ufpd->cores[thread->id]);
	if(!thread->plane){
		ufpd_log(LOG_ERR,
			"failed to ufp_plane_alloc, idx = %d", thread->id);
		goto err_plane_alloc;
	}

	err = pthread_create(&thread->tid,
		NULL, thread_process_interrupt, thread);
	if(err < 0){
		ufpd_log(LOG_ERR, "failed to create thread");
		goto err_pthread_create;
	}

	CPU_ZERO(&cpuset);
	CPU_SET(core_id, &cpuset);
	err = pthread_setaffinity_np(thread->tid,
		sizeof(cpu_set_t), &cpuset);
	if(err < 0){
		ufpd_log(LOG_ERR, "failed to set affinity");
		goto err_set_affinity;
	}

	return 0;

err_set_affinity:
	ufpd_thread_kill(thread);
err_pthread_create:
	ufp_plane_release(thread->plane);
err_plane_alloc:
	ufp_release_buf(thread->buf);
err_buf_alloc:
	return -1;
}

void ufpd_log(int level, char *fmt, ...){
	va_list args;
	va_start(args, fmt);

	vsyslog(level, fmt, args);

	va_end(args);
}

static void ufpd_thread_kill(struct ufpd_thread *thread)
{
	int err;

	err = pthread_kill(thread->tid, SIGUSR1);
	if(err != 0)
		ufpd_log(LOG_ERR, "failed to kill thread");

	err = pthread_join(thread->tid, NULL);
	if(err != 0)
		ufpd_log(LOG_ERR, "failed to join thread");

	ufp_plane_release(thread->plane);
	ufp_release_buf(thread->buf);
	return;
}

static int ufpd_set_signal(sigset_t *sigset)
{
	int err;

	sigemptyset(sigset);
	err = sigaddset(sigset, SIGUSR1);
	if(err != 0)
		goto err_sigaddset;

	err = sigaddset(sigset, SIGHUP);
	if(err != 0)
		goto err_sigaddset;

	err = sigaddset(sigset, SIGINT);
	if(err != 0)
		goto err_sigaddset;

	err = sigaddset(sigset, SIGTERM);
	if(err != 0)
		goto err_sigaddset;

	err = pthread_sigmask(SIG_BLOCK, sigset, NULL);
	if(err != 0)
		goto err_sigmask;

	return 0;

err_sigmask:
err_sigaddset:
	return -1;
}

static int ufpd_set_mempolicy(unsigned int node)
{
	int err;
	unsigned long node_mask;

	node_mask = 1UL << node;

	err = syscall(SYS_set_mempolicy, MPOL_BIND, &node_mask,
		sizeof(unsigned long) * 8);
	if(err < 0){
		return -1;
	}

	return 0;
}

static int ufpd_parse_args(struct ufpd *ufpd, int argc, char **argv)
{
	int err, opt;
	char strbuf[1024];

	while((opt = getopt(argc, argv, "c:p:n:m:b:ah")) != -1){
		switch(opt){
		case 'c':
			err = ufpd_parse_range(optarg,
				strbuf, sizeof(strbuf));
			if(err < 0){
				printf("Invalid argument\n");
				goto err_arg;
			}

			ufpd->num_threads = ufpd_parse_list(strbuf,
				ufpd->cores, sizeof(unsigned int),
				"%u", UFP_MAX_CORES);
			if(ufpd->num_threads < 0){
				printf("Invalid CPU cores to use\n");
				goto err_arg;
			}
			break;
		case 'p':
			ufpd->num_devices = ufpd_parse_list(optarg,
				ufpd->ifnames, IFNAMSIZ,
				"%s", UFP_MAX_IFS);
			if(ufpd->num_devices < 0){
				printf("Invalid Interfaces to use\n");
				goto err_arg;
			}
			break;
		case 'n':
			if(sscanf(optarg, "%u", &ufpd->numa_node) != 1){
				printf("Invalid NUMA node\n");
				goto err_arg;
			}
			break;
		case 'm':
			if(sscanf(optarg, "%u", &ufpd->mtu_frame) != 1){
				printf("Invalid MTU length\n");
				goto err_arg;
			}
			break;
		case 'b':
			if(sscanf(optarg, "%u", &ufpd->buf_count) != 1){
				printf("Invalid number of packet buffer\n");
				goto err_arg;
			}
			break;
		case 'a':
			ufpd->promisc = 1;
			break;
		case 'h':
			usage();
			goto err_arg;
			break;
		default:
			usage();
			goto err_arg;
		}
	}

	if(!ufpd->num_devices){
		printf("You must specify PCI Interfaces to use.\n");
		goto err_arg;
	}

	if(!ufpd->num_threads){
		printf("You must specify CPU cores to use.\n");
		goto err_arg;
	}

	return 0;

err_arg:
	return -1;
}

static int ufpd_parse_range(const char *str, char *result,
	int max_len)
{
	unsigned int range[2];
	int err, i, num, offset, ranged;
	char buf[128];

	result[0] = '\0';
	offset = 0;
	ranged = 0;
	for(i = 0; i < strlen(str) + 1; i++){
		switch(str[i]){
		case ',':
		case '\0':
			buf[offset] = '\0';

			if(sscanf(buf, "%u", &range[1]) < 1){
				goto err_parse;
			}

			if(!ranged)
				range[0] = range[1];

			for(num = range[0]; num <= range[1]; num++){
				err = snprintf(&(result)[strlen(result)],
					max_len,
					strlen(result) ? ",%d" : "%d", num);
				if(err < 0)
					goto err_print;
			}

			offset = 0;
			ranged = 0;

			break;
		case '-':
			buf[offset] = '\0';

			if(sscanf(buf, "%u", &range[0]) < 1){
				goto err_parse;
			}

			offset = 0;
			ranged = 1;

			break;
		default:
			buf[offset] = str[i];
			offset++;
		}
	}

	return 0;

err_print:
err_parse:
	return -1;
}

static int ufpd_parse_list(const char *str, void *result,
	int size_elem, const char *format, int max_count)
{
	int i, offset, count;
	char buf[128];

	offset = 0;
	count = 0;
	for(i = 0; i < strlen(str) + 1; i++){
		switch(str[i]){
		case ',':
		case '\0':
			buf[offset] = '\0';

			if(count >= max_count)
				goto err_max_count;

			if(sscanf(buf, format,
			result + (size_elem * count)) < 1){
				printf("parse error\n");
				goto err_parse;
			}

			count++;

			offset = 0;
			break;
		default:
			buf[offset] = str[i];
			offset++;
		}
	}

	return count;

err_parse:
err_max_count:
	return -1;
}
