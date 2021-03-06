#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/if_tun.h>
#include <net/if_arp.h>

#include "lib_main.h"
#include "lib_tap.h"

static int ufp_tun_iff(int fd, const char *if_name);
static int ufp_tun_persist(int fd, const char *if_name,
	int flag);
static int ufp_tun_macaddr_set(int sock, const char *if_name,
	void *src_mac);
static int ufp_tun_mtu_set(int sock, const char *if_name,
	unsigned int mtu_frame);
static int ufp_tun_flags_set(int sock, const char *if_name,
	short flags);
static short ufp_tun_flags_get(int sock, const char *if_name);
static int ufp_tun_index_get(int sock, const char *if_name);

int ufp_tun_open(struct ufp_iface *iface)
{
	int fd, err;

	fd = open("/dev/net/tun", O_RDWR);
	if(fd < 0)
		goto err_tun_open;

	err = ufp_tun_iff(fd, iface->name);
	if(err < 0)
		goto err_tun_iff;

	err = ufp_tun_persist(fd, iface->name, 1);
	if(err < 0)
		goto err_tun_persist;

	close(fd);
	return 0;

err_tun_persist:
err_tun_iff:
	close(fd);
err_tun_open:
	return -1;
}

void ufp_tun_close(struct ufp_iface *iface)
{

	int fd, err;

	fd = open("/dev/net/tun", O_RDWR);
	if(fd < 0)
		goto err_tun_open;

	err = ufp_tun_iff(fd, iface->name);
	if(err < 0)
		goto err_tun_iff;

	err = ufp_tun_persist(fd, iface->name, 0);
	if(err < 0)
		goto err_tun_persist;

err_tun_persist:
err_tun_iff:
	close(fd);
err_tun_open:
	return;
}

int ufp_tun_up(struct ufp_iface *iface)
{
	short flags;
	int sock, i, err;
	unsigned int open_done = 0;

	iface->tap_fds = malloc(sizeof(int) * iface->num_qps);
	if(!iface->tap_fds)
		goto err_fds_alloc;

	for(i = 0; i < iface->num_qps; i++, open_done++){
		iface->tap_fds[i] = open("/dev/net/tun", O_RDWR);
		if(iface->tap_fds < 0)
			goto err_tun_open;

		err = ufp_tun_iff(iface->tap_fds[i], iface->name);
		if(err < 0)
			goto err_tun_iff;

		continue;

err_tun_iff:
		close(iface->tap_fds[i]);
err_tun_open:
		goto err_queues;
	}

	/* Must open a normal socket (UDP in this case) for some ioctl */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0)
		goto err_sock_open;

	err = ufp_tun_macaddr_set(sock, iface->name,
		iface->mac_addr);
	if(err < 0)
		goto err_tun_mac;

	iface->tap_index = ufp_tun_index_get(sock, iface->name);
	if(iface->tap_index < 0)
		goto err_tun_ifindex;

	err = ufp_tun_mtu_set(sock, iface->name, iface->mtu_frame);
	if(err < 0)
		goto err_tun_mtu;

	flags = ufp_tun_flags_get(sock, iface->name);
	if(flags < 0)
		goto err_tun_flags_get;

	flags |= IFF_UP;
	err = ufp_tun_flags_set(sock, iface->name, flags);
	if(err < 0)
		goto err_tun_flags_set;

	close(sock);
	return 0;

err_tun_flags_set:
err_tun_flags_get:
err_tun_mtu:
err_tun_ifindex:
err_tun_mac:
	close(sock);
err_sock_open:
err_queues:
	for(i = 0; i < open_done; i++){
		close(iface->tap_fds[i]);
	}
	free(iface->tap_fds);
err_fds_alloc:
	return -1;
}

void ufp_tun_down(struct ufp_iface *iface)
{
	int sock, i, err;
	short flags;

	/* Must open a normal socket (UDP in this case) for some ioctl */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0)
		goto err_sock_open;

	flags = ufp_tun_flags_get(sock, iface->name);
	if(flags < 0)
		goto err_tun_flags_get;

	flags &= ~IFF_UP;
	err = ufp_tun_flags_set(sock, iface->name, flags);
	if(err < 0)
		goto err_tun_flags_set;

err_tun_flags_set:
err_tun_flags_get:
	close(sock);
err_sock_open:
	for(i = 0; i < iface->num_qps; i++){
		close(iface->tap_fds[i]);
	}
	free(iface->tap_fds);

	return;
}

static int ufp_tun_iff(int fd, const char *if_name)
{
	struct ifreq ifr;
	int err;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_flags = IFF_TAP | IFF_NO_PI | IFF_MULTI_QUEUE;

	err = ioctl(fd, TUNSETIFF, (void *)&ifr);
	if(err < 0)
		goto err_tun_ioctl;

	return 0;

err_tun_ioctl:
	return -1;
}

static int ufp_tun_persist(int fd, const char *if_name,
	int flag)
{
	int err;

	err = ioctl(fd, TUNSETPERSIST, flag);
	if(err < 0)
		goto err_tun_ioctl;

	return 0;

err_tun_ioctl:
	return -1;
}

static int ufp_tun_macaddr_set(int sock, const char *if_name,
	void *src_mac)
{
	struct ifreq ifr;
	int err;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	memcpy(ifr.ifr_hwaddr.sa_data, src_mac, ETH_ALEN);

	err = ioctl(sock, SIOCSIFHWADDR, (void *)&ifr);
	if(err < 0)
		goto err_tun_ioctl;

	return 0;

err_tun_ioctl:
	return -1;
}

static int ufp_tun_mtu_set(int sock, const char *if_name,
	unsigned int mtu_frame)
{
	struct ifreq ifr;
	int err;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	ifr.ifr_mtu = mtu_frame
		- (ETH_HLEN + ETH_FCS_LEN);

	err = ioctl(sock, SIOCSIFMTU, (void *)&ifr);
	if(err < 0)
		goto err_tun_ioctl;

	return 0;

err_tun_ioctl:
	return -1;
}

static int ufp_tun_flags_set(int sock, const char *if_name,
	short flags)
{
	struct ifreq ifr;
	int err;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_flags = flags;

	err = ioctl(sock, SIOCSIFFLAGS, (void *)&ifr);
	if(err < 0)
		goto err_tun_ioctl;

	return 0;

err_tun_ioctl:
	return -1;
}

static short ufp_tun_flags_get(int sock, const char *if_name)
{
	struct ifreq ifr;
	int err;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	err = ioctl(sock, SIOCGIFFLAGS, (void *)&ifr);
	if(err < 0)
		goto err_tun_ioctl;

	return ifr.ifr_flags;

err_tun_ioctl:
	return -1;
}

static int ufp_tun_index_get(int sock, const char *if_name)
{
	struct ifreq ifr;
	int err;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	err = ioctl(sock, SIOCGIFINDEX, (void *)&ifr);
	if(err < 0)
		goto err_tun_ioctl;

	return ifr.ifr_ifindex;

err_tun_ioctl:
	return -1;
}

