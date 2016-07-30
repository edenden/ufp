#ifndef _UFPD_TUN_H
#define _UFPD_TUN_H

#include "main.h"

struct tun_handle {
	int		*queues;
        unsigned int	ifindex;
	unsigned int	mtu_frame;
};

struct tun_port {
	int		fd;
	unsigned int	ifindex;
	unsigned int	mtu_frame;
};

struct tun_plane {
	struct tun_port	*ports;
};

struct tun_handle *tun_open(struct ufpd *ufpd,
	unsigned int port_index);
void tun_close(struct ufpd *ufpd, unsigned int port_index);
struct tun_plane *tun_plane_alloc(struct ufpd *ufpd,
	unsigned int thread_id);
void tun_plane_release(struct tun_plane *plane, int num_ports);

#endif /* _UFPD_TUN_H */
