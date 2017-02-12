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

unsigned short ufp_portnum(struct ufp_plane *plane)
{
	return plane->num_ports;
}

int ufp_irq_fd(struct ufp_plane *plane, unsigned int port_idx,
	enum ufp_irq_type type)
{
	struct ufp_port *port;
	struct ufp_irq *irq;

	port = &plane->ports[port_idx];

	switch(type){
	case UFP_IRQ_RX:
		irq = port->rx_irq;
		break;
	case UFP_IRQ_TX:
		irq = port->tx_irq;
		break;
	default:
		goto err_undefined_type;
	}

	return irq->fd;

err_undefined_type:
	return -1;
}

struct ufp_irq *ufp_irq(struct ufp_plane *plane,
	unsigned int port_idx, enum ufp_irq_type type)
{
	struct ufp_port *port;
	struct ufp_irq *irq;

	port = &plane->ports[port_idx];

	switch(type){
	case UFP_IRQ_RX:
		irq = port->rx_irq;
		break;
	case UFP_IRQ_TX:
		irq = port->tx_irq;
		break;
	default:
		goto err_undefined_type;
	}

	return irq;

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
