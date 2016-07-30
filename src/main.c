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

#include "linux/list.h"
#include "main.h"
#include "thread.h"

static void usage();
static int ufpd_thread_create(struct ufpd *ufpd,
	struct ufpd_thread *thread, int thread_index);
static void ufpd_thread_kill(struct ufpd_thread *thread);
static int ufpd_set_signal(sigset_t *sigset);
static int ufpd_set_mempolicy(unsigned int node);

char *optarg;

static void usage()
{
	printf("\n");
	printf("Usage:\n");
	printf("  -c [n] : Number of cores\n");
	printf("  -p [n] : Number of ports\n");
	printf("  -n [n] : NUMA node (default=0)\n");
	printf("  -m [n] : MTU length (default=1522)\n");
	printf("  -b [n] : Number of packet buffer per port\n");
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
	int			cores_assigned = 0,
				ports_assigned = 0,
				ports_up = 0,
				tun_assigned = 0,
				pages_assigned = 0;
	sigset_t		sigset;

	/* set default values */
	ufpd.numa_node	= 0;
	ufpd.num_cores	= 1;
	ufpd.buf_size	= 0;
	ufpd.num_ports	= 0;
	ufpd.promisc	= 0;
	ufpd.mtu_frame	= 0; /* MTU=1522 is used by default. */
	ufpd.intr_rate	= IXGBE_20K_ITR;
	ufpd.buf_count	= 8192; /* number of per port packet buffer */

	while ((opt = getopt(argc, argv, "c:p:n:m:b:ah")) != -1) {
		switch(opt){
		case 'c':
			if(sscanf(optarg, "%u", &ufpd.num_cores) < 1){
				printf("Invalid number of cores\n");
				ret = -1;
				goto err_arg;
			}
			break;
		case 'p':
			if(sscanf(optarg, "%u", &ufpd.num_ports) < 1){
				printf("Invalid number of ports\n");
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

	if(!ufpd.num_ports){
		printf("You must specify number of ports.\n");
		printf("Try -h to show help.\n");
		ret = -1;
		goto err_arg;
	}

	openlog(PROCESS_NAME, LOG_CONS | LOG_PID, SYSLOG_FACILITY);

	ret = ufpd_set_mempolicy(ufpd.numa_node);
	if(ret < 0)
		goto err_set_mempolicy;

	ufpd.ih_array = malloc(sizeof(struct ufp_handle *) * ufpd.num_ports);
	if(!ufpd.ih_array){
		ret = -1;
		goto err_ih_array;
	}

	ufpd.tunh_array = malloc(sizeof(struct tun_handle *) * ufpd.num_ports);
	if(!ufpd.tunh_array){
		ret = -1;
		goto err_tunh_array;
	}

	threads = malloc(sizeof(struct ufpd_thread) * ufpd.num_cores);
	if(!threads){
		ret = -1;
		goto err_alloc_threads;
	}

	for(i = 0; i < ufpd.num_ports; i++, ports_assigned++){
		ufpd.ih_array[i] = ufp_open(i, ufpd.num_cores,
			IXGBE_MAX_RXD, IXGBE_MAX_TXD);
		if(!ufpd.ih_array[i]){
			ufpd_log(LOG_ERR, "failed to ufp_open, idx = %d", i);
			ret = -1;
			goto err_open;
		}

		/* calclulate maximum buf_size we should prepare */
		if(ufp_bufsize_get(ufpd.ih_array[i]) > ufpd.buf_size)
			ufpd.buf_size = ufp_bufsize_get(ufpd.ih_array[i]);
	}

	for(i = 0; i < ufpd.num_cores; i++, pages_assigned++){
		threads[i].desc = ufp_desc_alloc(ufpd.ih_array,
			ufpd.num_ports, i);
		if(!threads[i].desc){
			ufpd_log(LOG_ERR, "failed to ufp_alloc_descring, idx = %d", i);
			ufpd_log(LOG_ERR, "please decrease descripter or enable iommu");
			goto err_desc_alloc;
		}

		threads[i].buf = ufp_buf_alloc(ufpd.ih_array,
			ufpd.num_ports, ufpd.buf_count, ufpd.buf_size, i);
		if(!threads[i].buf){
			ufpd_log(LOG_ERR, "failed to ufp_alloc_buf, idx = %d", i);
			ufpd_log(LOG_ERR, "please decrease buffer or enable iommu");
			goto err_buf_alloc;
		}

		continue;

err_buf_alloc:
		ufp_desc_release(ufpd.ih_array,
			ufpd.num_ports, i, threads[i].desc);
err_desc_alloc:
		ret = -1;
		goto err_assign_pages;
	}

	for(i = 0; i < ufpd.num_ports; i++, ports_up++){
		ret = ufp_up(ufpd.ih_array[i], ufpd.intr_rate,
			ufpd.mtu_frame, ufpd.promisc,
			IXMAP_RX_BUDGET, IXMAP_TX_BUDGET);
		if(ret < 0){
			ufpd_log(LOG_ERR, "failed to ufp_up, idx = %d", i);
			ret = -1;
			goto err_up;
		}
	}

	for(i = 0; i < ufpd.num_ports; i++, tun_assigned++){
		ufpd.tunh_array[i] = tun_open(&ufpd, i);
		if(!ufpd.tunh_array[i]){
			ufpd_log(LOG_ERR, "failed to tun_open");
			ret = -1;
			goto err_tun_open;
		}
	}

	ret = ufpd_set_signal(&sigset);
	if(ret != 0){
		goto err_set_signal;
	}

	for(i = 0; i < ufpd.num_cores; i++, cores_assigned++){
		threads[i].plane = ufp_plane_alloc(ufpd.ih_array,
			threads[i].buf, ufpd.num_ports, i);
		if(!threads[i].plane){
			ufpd_log(LOG_ERR, "failed to ufp_plane_alloc, idx = %d", i);
			goto err_plane_alloc;
		}

		threads[i].tun_plane = tun_plane_alloc(&ufpd, i);
		if(!threads[i].tun_plane)
			goto err_tun_plane_alloc;

		ret = ufpd_thread_create(&ufpd, &threads[i], i);
		if(ret < 0){
			goto err_thread_create;
		}

		continue;

err_thread_create:
		tun_plane_release(threads[i].tun_plane,
			ufpd.num_ports);
err_tun_plane_alloc:
		ufp_plane_release(threads[i].plane,
			ufpd.num_ports);
err_plane_alloc:
		ret = -1;
		goto err_assign_cores;
	}

	while(1){
		if(sigwait(&sigset, &signal) == 0){
			break;
		}
	}
	ret = 0;

err_assign_cores:
	for(i = 0; i < cores_assigned; i++){
		ufpd_thread_kill(&threads[i]);
		tun_plane_release(threads[i].tun_plane,
			ufpd.num_ports);
		ufp_plane_release(threads[i].plane,
			ufpd.num_ports);
	}
err_set_signal:
err_tun_open:
	for(i = 0; i < tun_assigned; i++){
		tun_close(&ufpd, i);
	}
err_up:
	for(i = 0; i < ports_up; i++){
		ufp_down(ufpd.ih_array[i]);
	}
err_assign_pages:
	for(i = 0; i < pages_assigned; i++){
		ufp_buf_release(threads[i].buf,
			ufpd.ih_array, ufpd.num_ports);
		ufp_desc_release(ufpd.ih_array,
			ufpd.num_ports, i, threads[i].desc);
	}
err_open:
	for(i = 0; i < ports_assigned; i++){
		ufp_close(ufpd.ih_array[i]);
	}
	free(threads);
err_alloc_threads:
	free(ufpd.tunh_array);
err_tunh_array:
	free(ufpd.ih_array);
err_ih_array:
err_set_mempolicy:
	closelog();
err_arg:
	return ret;
}

static int ufpd_thread_create(struct ufpd *ufpd,
	struct ufpd_thread *thread, int thread_index)
{
	cpu_set_t cpuset;
	int ret;

	thread->index		= thread_index;
	thread->num_ports	= ufpd->num_ports;
	thread->ptid		= pthread_self();

	ret = pthread_create(&thread->tid, NULL, thread_process_interrupt, thread);
	if(ret < 0){
		ufpd_log(LOG_ERR, "failed to create thread");
		goto err_pthread_create;
	}

	CPU_ZERO(&cpuset);
	CPU_SET(thread->index, &cpuset);
	ret = pthread_setaffinity_np(thread->tid, sizeof(cpu_set_t), &cpuset);
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
