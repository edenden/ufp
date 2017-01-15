#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "lib_main.h"
#include "lib_api.h"

void *ufp_macaddr_default(struct ufp_iface *iface)
{
	return iface->mac_addr;
}

unsigned int ufp_mtu_get(struct ufp_iface *iface)
{
	return iface->mtu_frame;
}

char *ufp_ifname_get(struct ufp_dev *dev)
{
	return dev->name;
}

void *ufp_macaddr(struct ufp_plane *plane,
	unsigned int port_idx)
{
	return plane->ports[port_idx].mac_addr;
}

int ufp_irq_fd(struct ufp_plane *plane, unsigned int port_idx,
	enum ufp_irq_type type)
{
	struct ufp_port *port;
	struct ufp_irq_handle *irqh;

	port = &plane->ports[port_idx];

	switch(type){
	case UFP_IRQ_RX:
		irqh = port->rx_irq;
		break;
	case UFP_IRQ_TX:
		irqh = port->tx_irq;
		break;
	default:
		goto err_undefined_type;
	}

	return irqh->fd;

err_undefined_type:
	return -1;
}

struct ufp_irq_handle *ufp_irq_handle(struct ufp_plane *plane,
	unsigned int port_idx, enum ufp_irq_type type)
{
	struct ufp_port *port;
	struct ufp_irq_handle *irqh;

	port = &plane->ports[port_idx];

	switch(type){
	case UFP_IRQ_RX:
		irqh = port->rx_irq;
		break;
	case UFP_IRQ_TX:
		irqh = port->tx_irq;
		break;
	default:
		goto err_undefined_type;
	}

	return irqh;

err_undefined_type:
	return NULL;
}

unsigned long ufp_count_rx_alloc_failed(struct ufp_plane *plane,
	unsigned int port_idx)
{
	return plane->ports[port_idx].count_rx_alloc_failed;
}

unsigned long ufp_count_rx_clean_total(struct ufp_plane *plane,
	unsigned int port_idx)
{
	return plane->ports[port_idx].count_rx_clean_total;
}

unsigned long ufp_count_tx_xmit_failed(struct ufp_plane *plane,
	unsigned int port_idx)
{
	return plane->ports[port_idx].count_tx_xmit_failed;
}

unsigned long ufp_count_tx_clean_total(struct ufp_plane *plane,
	unsigned int port_idx)
{
	return plane->ports[port_idx].count_tx_clean_total;
}
