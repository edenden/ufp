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
static int ufpd_thread_create(struct ufpd *ufpd,
	struct ufpd_thread *thread, unsigned int thread_id,
	unsigned int core_id);
static void ufpd_thread_kill(struct ufpd_thread *thread);
static int ufpd_set_signal(sigset_t *sigset);
static int ufpd_set_mempolicy(unsigned int node);
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
	int			ret, i, signal, opt;
	int			threads_assigned = 0,
				devices_assigned = 0,
				devices_up = 0,
				tun_assigned = 0,
				mpool_assigned = 0;
	sigset_t		sigset;
	char			strbuf[1024];

	/* set default values */
	ufpd.numa_node		= 0;
	ufpd.num_threads	= 0;
	ufpd.num_devices	= 0;
	ufpd.promisc		= 0;
	ufpd.mtu_frame		= 0; /* MTU=1522 is used by default. */
	ufpd.buf_count		= 8192; /* number of per port packet buffer */
	ufpd.buf_size		= 2048; /* must be larger than 2048 */

	while ((opt = getopt(argc, argv, "c:p:n:m:b:ah")) != -1) {
		switch(opt){
		case 'c':
			ret = ufpd_parse_range(optarg, strbuf, sizeof(strbuf));
			if(ret < 0){
				printf("Invalid argument\n");
				ret = -1;
				goto err_arg;
			}

			ufpd.num_threads = ufpd_parse_list(strbuf, ufpd.cores,
				sizeof(unsigned int), "%u", UFP_MAX_CORES);
			if(ufpd.num_threads < 0){
				printf("Invalid CPU cores to use\n");
				ret = -1;
				goto err_arg;
			}
			break;
		case 'p':
			ufpd.num_devices = ufpd_parse_list(optarg, ufpd.ifnames,
				IFNAMSIZ, "%s", UFP_MAX_IFS);
			if(ufpd.num_devices < 0){
				printf("Invalid Interfaces to use\n");
				ret = -1;
				goto err_arg;
			}
			break;
		case 'n':
			if(sscanf(optarg, "%u", &ufpd.numa_node) < 1){
				printf("Invalid NUMA node\n");
				ret = -1;
				goto err_arg;
			}
			break;
		case 'm':
			if(sscanf(optarg, "%u", &ufpd.mtu_frame) < 1){
				printf("Invalid MTU length\n");
				ret = -1;
				goto err_arg;
			}
			break;
		case 'b':
			if(sscanf(optarg, "%u", &ufpd.buf_count) < 1){
				printf("Invalid number of packet buffer\n");
				ret = -1;
				goto err_arg;
			}
			break;
		case 'a':
			ufpd.promisc = 1;
			break;
		case 'h':
			usage();
			ret = 0;
			goto err_arg;
			break;
		default:
			usage();
			ret = -1;
			goto err_arg;
		}
	}

	if(!ufpd.num_devices){
		printf("You must specify PCI Interfaces to use.\n");
		ret = -1;
		goto err_arg;
	}

	if(!ufpd.num_threads){
		printf("You must specify CPU cores to use.\n");
		ret = -1;
		goto err_arg;
	}

	openlog(PROCESS_NAME, LOG_CONS | LOG_PID, SYSLOG_FACILITY);

	ret = ufpd_set_mempolicy(ufpd.numa_node);
	if(ret < 0)
		goto err_set_mempolicy;

	ufpd.devs = malloc(sizeof(struct ufp_dev *) * ufpd.num_devices);
	if(!ufpd.devs){
		ret = -1;
		goto err_devs;
	}

	ufpd.tunhs = malloc(sizeof(struct tun_handle *) * ufpd.num_devices);
	if(!ufpd.tunhs){
		ret = -1;
		goto err_tunhs;
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

	for(i = 0; i < ufpd.num_threads; i++, mpool_assigned++){
		ufpd.mpools[i] = ufp_mpool_init();
		if(!ufpd.mpools[i]){
			ret = -1;
			goto err_mempool_init;
		}
	}

	for(i = 0; i < ufpd.num_devices; i++, devices_assigned++){
		ufpd.devs[i] = ufp_open(&ufpd.ifnames[i * IFNAMSIZ]);
		if(!ufpd.devs[i]){
			ufpd_log(LOG_ERR, "failed to ufp_open, idx = %d", i);
			ret = -1;
			goto err_open;
		}
	}

	for(i = 0; i < ufpd.num_devices; i++, tun_assigned++){
		ufpd.tunhs[i] = tun_open(&ufpd, i);
		if(!ufpd.tunhs[i]){
			ufpd_log(LOG_ERR, "failed to tun_open");
			ret = -1;
			goto err_tun_open;
		}
	}

	for(i = 0; i < ufpd.num_devices; i++, devices_up++){
		ret = ufp_up(ufpd.devs[i], ufpd.mpools,
			ufpd.num_threads, ufpd.mtu_frame, ufpd.promisc,
			UFP_RX_BUDGET, UFP_TX_BUDGET);
		if(ret < 0){
			ufpd_log(LOG_ERR, "failed to ufp_up, idx = %d", i);
			ret = -1;
			goto err_up;
		}
	}

	ret = ufpd_set_signal(&sigset);
	if(ret != 0){
		goto err_set_signal;
	}

	for(i = 0; i < ufpd.num_threads; i++, threads_assigned++){
		threads[i].mpool = ufpd.mpools[i];
		threads[i].buf = ufp_alloc_buf(ufpd.devs, ufpd.num_devices,
			ufpd.buf_size, ufpd.buf_count, threads[i].mpool);
		if(!threads[i].buf){
			ufpd_log(LOG_ERR,
				"failed to ufp_alloc_buf, idx = %d", i);
			goto err_buf_alloc;
		}

		threads[i].plane = ufp_plane_alloc(ufpd.devs, ufpd.num_devices,
			threads[i].buf, i, ufpd.cores[i]);
		if(!threads[i].plane){
			ufpd_log(LOG_ERR,
				"failed to ufp_plane_alloc, idx = %d", i);
			goto err_plane_alloc;
		}

		threads[i].tun_plane = tun_plane_alloc(&ufpd, i);
		if(!threads[i].tun_plane)
			goto err_tun_plane_alloc;

		ret = ufpd_thread_create(&ufpd, &threads[i], i, ufpd.cores[i]);
		if(ret < 0){
			goto err_thread_create;
		}

		continue;

err_thread_create:
		tun_plane_release(threads[i].tun_plane);
err_tun_plane_alloc:
		ufp_plane_release(threads[i].plane);
err_plane_alloc:
		ufp_release_buf(ufpd.devs, ufpd.num_devices, threads[i].buf);
err_buf_alloc:
		ret = -1;
		goto err_assign_threads;
	}

	while(1){
		if(sigwait(&sigset, &signal) == 0){
			break;
		}
	}
	ret = 0;

err_assign_threads:
	for(i = 0; i < threads_assigned; i++){
		ufpd_thread_kill(&threads[i]);
		tun_plane_release(threads[i].tun_plane);
		ufp_plane_release(threads[i].plane);
		ufp_release_buf(ufpd.devs, ufpd.num_devices, threads[i].buf);
	}
err_set_signal:
err_up:
	for(i = 0; i < devices_up; i++){
		ufp_down(ufpd.devs[i]);
	}
err_tun_open:
	for(i = 0; i < tun_assigned; i++){
		tun_close(&ufpd, i);
	}
err_open:
	for(i = 0; i < devices_assigned; i++){
		ufp_close(ufpd.devs[i]);
	}
err_mempool_init:
	for(i = 0; i < mpool_assigned; i++){
		ufp_mpool_destroy(ufpd.mpools[i]);
	}
	free(threads);
err_alloc_threads:
	free(ufpd.mpools);
err_mpools:
	free(ufpd.tunhs);
err_tunhs:
	free(ufpd.devs);
err_devs:
err_set_mempolicy:
	closelog();
err_arg:
	return ret;
}

static int ufpd_thread_create(struct ufpd *ufpd,
	struct ufpd_thread *thread, unsigned int thread_id,
	unsigned int core_id)
{
	cpu_set_t cpuset;
	int ret;

	thread->id		= thread_id;
	thread->num_ports	= ufpd->num_ports;
	thread->ptid		= pthread_self();

	ret = pthread_create(&thread->tid,
		NULL, thread_process_interrupt, thread);
	if(ret < 0){
		ufpd_log(LOG_ERR, "failed to create thread");
		goto err_pthread_create;
	}

	CPU_ZERO(&cpuset);
	CPU_SET(core_id, &cpuset);
	ret = pthread_setaffinity_np(thread->tid,
		sizeof(cpu_set_t), &cpuset);
	if(ret < 0){
		ufpd_log(LOG_ERR, "failed to set affinity");
		goto err_set_affinity;
	}

	return 0;

err_set_affinity:
	ufpd_thread_kill(thread);
err_pthread_create:
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
	int ret;

	ret = pthread_kill(thread->tid, SIGUSR1);
	if(ret != 0)
		ufpd_log(LOG_ERR, "failed to kill thread");

	ret = pthread_join(thread->tid, NULL);
	if(ret != 0)
		ufpd_log(LOG_ERR, "failed to join thread");

	return;
}

static int ufpd_set_signal(sigset_t *sigset)
{
	int ret;

	sigemptyset(sigset);
	ret = sigaddset(sigset, SIGUSR1);
	if(ret != 0)
		return -1;

	ret = sigaddset(sigset, SIGHUP);
	if(ret != 0)
		return -1;

	ret = sigaddset(sigset, SIGINT);
	if(ret != 0)
		return -1;

	ret = sigaddset(sigset, SIGTERM);
	if(ret != 0)
		return -1;

	ret = pthread_sigmask(SIG_BLOCK, sigset, NULL);
	if(ret != 0)
		return -1;

	return 0;
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
