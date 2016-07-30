#ifndef _UFPD_EPOLL_H
#define _UFPD_EPOLL_H

#include <linux/netlink.h>
#include <signal.h>
#include <ufp.h>
#include "linux/list.h"
#include "iftap.h"

#define EPOLL_MAXEVENTS 16

enum {
	EPOLL_IRQ_RX = 0,
	EPOLL_IRQ_TX,
	EPOLL_TUN,
	EPOLL_SIGNAL,
	EPOLL_NETLINK
};

struct epoll_desc {
	int			fd;
	int			type;
	unsigned int		port_index;
	void			*data;
	struct list_head	list;
};

int epoll_add(int fd_ep, void *ptr, int fd);
int epoll_del(int fd_ep, int fd);
struct epoll_desc *epoll_desc_alloc_irq(struct ufp_plane *plane,
	unsigned int port_index, enum ufp_irq_type type);
void epoll_desc_release_irq(struct epoll_desc *ep_desc);
struct epoll_desc *epoll_desc_alloc_signalfd(sigset_t *sigset);
void epoll_desc_release_signalfd(struct epoll_desc *ep_desc);
struct epoll_desc *epoll_desc_alloc_tun(struct tun_plane *tun_plane,
	unsigned int port_index);
void epoll_desc_release_tun(struct epoll_desc *ep_desc);
struct epoll_desc *epoll_desc_alloc_netlink(struct sockaddr_nl *addr);
void epoll_desc_release_netlink(struct epoll_desc *ep_desc);

#endif /* _UFPD_EPOLL_H */
