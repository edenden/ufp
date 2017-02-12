#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/if_tun.h>
#include <linux/if_arp.h>

#include "main.h"
#include "iftap.h"

static int tun_assign(int fd, const char *if_name);
static int tun_mac(int fd, const char *if_name, void *src_mac);
static int tun_mtu(int fd, const char *if_name, unsigned int mtu_frame);
static int tun_up(int fd, const char *if_name);
static int tun_ifindex(int fd, const char *if_name);

struct tun_handle *tun_open(struct ufpd *ufpd,
	unsigned int port_index)
{
	struct tun_handle *tunh;
	int sock, ret, i;
	unsigned int queue_assigned = 0;
	void *src_mac;
	unsigned int mtu_frame;
	const char *if_name;

	if_name = ufp_ifname_get(ufpd->devs[port_index]);
	src_mac = ufp_macaddr_default(ufpd->devs[port_index]);
	mtu_frame = ufp_mtu_get(ufpd->devs[port_index]);

	tunh = malloc(sizeof(struct tun_handle));
	if(!tunh)
		goto err_tunh_alloc;

	tunh->queues = malloc(sizeof(int) * ufpd->num_threads);
	if(!tunh->queues)
		goto err_queues_alloc;

	/* Must open a normal socket (UDP in this case) for some ioctl */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0)
		goto err_sock_open;

	for(i = 0; i < ufpd->num_threads; i++, queue_assigned++){
		tunh->queues[i] = open("/dev/net/tun", O_RDWR);
		if(tunh->queues[i] < 0)
			goto err_tun_open;

		ret = tun_assign(tunh->queues[i], if_name);
		if(ret < 0)
			goto err_tun_assign;

		continue;

err_tun_assign:
		close(tunh->queues[i]);
err_tun_open:
		goto err_queues;
	}

	ret = tun_mac(sock, if_name, src_mac);
	if(ret < 0)
		goto err_tun_mac;

	tunh->ifindex = tun_ifindex(sock, if_name);
	if(tunh->ifindex < 0)
		goto err_tun_ifindex;

	ret = tun_mtu(sock, if_name, mtu_frame);
	if(ret < 0)
		goto err_tun_mtu;
	tunh->mtu_frame = mtu_frame;

	ret = tun_up(sock, if_name);
	if(ret < 0)
		goto err_tun_up;

	close(sock);
	return tunh;

err_tun_up:
err_tun_mtu:
err_tun_ifindex:
err_tun_mac:
err_queues:
	for(i = 0; i < queue_assigned; i++){
		close(tunh->queues[i]);
	}
	close(sock);
err_sock_open:
	free(tunh->queues);
err_queues_alloc:
	free(tunh);
err_tunh_alloc:
	return NULL;
}

void tun_close(struct ufpd *ufpd, unsigned int port_index)
{
	struct tun_handle *tunh;
	int i;

	tunh = ufpd->tunhs[port_index];

	for(i = 0; i < ufpd->num_threads; i++){
		close(tunh->queues[i]);
	}
	free(tunh->queues);
	free(tunh);
	return;
}

static int tun_assign(int fd, const char *if_name)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_flags = IFF_TAP | IFF_NO_PI | IFF_MULTI_QUEUE;

	ret = ioctl(fd, TUNSETIFF, (void *)&ifr);
	if(ret < 0){
		perror("tun assign");
		goto err_tun_ioctl;
	}

	return 0;

err_tun_ioctl:
	return -1;
}

static int tun_mac(int fd, const char *if_name, void *src_mac)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	memcpy(ifr.ifr_hwaddr.sa_data, src_mac, ETH_ALEN);

	ret = ioctl(fd, SIOCSIFHWADDR, (void *)&ifr);
	if(ret < 0) {
		perror("tun mac");
		goto err_tun_ioctl;
	}

	return 0;

err_tun_ioctl:
	return -1;
}

static int tun_mtu(int fd, const char *if_name, unsigned int mtu_frame)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_mtu = mtu_frame;

	ret = ioctl(fd, SIOCSIFMTU, (void *)&ifr);
	if(ret < 0) {
		perror("tun mtu");
		goto err_tun_ioctl;
	}

	return 0;

err_tun_ioctl:
	return -1;
}

static int tun_up(int fd, const char *if_name)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_flags = IFF_UP;

	ret = ioctl(fd, SIOCSIFFLAGS, (void *)&ifr);
	if(ret < 0) {
		perror("tun up");
		goto err_tun_ioctl;
	}

	return 0;

err_tun_ioctl:
	return -1;
}

static int tun_ifindex(int fd, const char *if_name)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ret = ioctl(fd, SIOCGIFINDEX, (void *)&ifr);
	if(ret < 0) {
		perror("tun ifindex");
		goto err_tun_ioctl;
	}

	return ifr.ifr_ifindex;

err_tun_ioctl:
	return -1;
}

struct tun_plane *tun_plane_alloc(struct ufpd *ufpd,
	unsigned int thread_id)
{
	struct tun_handle **tunhs;
	struct tun_plane *plane;
	int i;

	tunhs = ufpd->tunhs;

	plane = malloc(sizeof(struct tun_plane));
	if(!plane)
		goto err_alloc_plane;

	plane->ports = malloc(sizeof(struct tun_port) * ufpd->num_devices);
	if(!plane->ports)
		goto err_alloc_ports;

	for(i = 0; i < ufpd->num_devices; i++){
		plane->ports[i].fd = tunhs[i]->queues[thread_id];
		plane->ports[i].ifindex = tunhs[i]->ifindex;
		plane->ports[i].mtu_frame = tunhs[i]->mtu_frame;
	}

	return plane;

err_alloc_ports:
	free(plane);
err_alloc_plane:
	return NULL;
}

void tun_plane_release(struct tun_plane *plane)
{
	free(plane->ports);
	free(plane);

	return;
}
