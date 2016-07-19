unsigned int ufp_bufsize_get(struct ufp_handle *ih)
{
	return ih->buf_size;
}

uint8_t *ufp_macaddr_default(struct ufp_handle *ih)
{
	return ih->mac_addr;
}

unsigned int ufp_mtu_get(struct ufp_handle *ih)
{
	return ih->mtu_frame;
}

uint8_t *ufp_macaddr(struct ufp_plane *plane,
	unsigned int port_index)
{
	return plane->ports[port_index].mac_addr;
}

int ufp_irq_fd(struct ufp_plane *plane, unsigned int port_index,
	enum ufp_irq_type type)
{
	struct ufp_port *port;
	struct ufp_irq_handle *irqh;

	port = &plane->ports[port_index];

	switch(type){
	case IXMAP_IRQ_RX:
		irqh = port->rx_irq;
		break;
	case IXMAP_IRQ_TX:
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
	unsigned int port_index, enum ufp_irq_type type)
{
	struct ufp_port *port;
	struct ufp_irq_handle *irqh;

	port = &plane->ports[port_index];

	switch(type){
	case IXMAP_IRQ_RX:
		irqh = port->rx_irq;
		break;
	case IXMAP_IRQ_TX:
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
	unsigned int port_index)
{
	return plane->ports[port_index].count_rx_alloc_failed;
}

unsigned long ufp_count_rx_clean_total(struct ufp_plane *plane,
	unsigned int port_index)
{
	return plane->ports[port_index].count_rx_clean_total;
}

unsigned long ufp_count_tx_xmit_failed(struct ufp_plane *plane,
	unsigned int port_index)
{
	return plane->ports[port_index].count_tx_xmit_failed;
}

unsigned long ufp_count_tx_clean_total(struct ufp_plane *plane,
	unsigned int port_index)
{
	return plane->ports[port_index].count_tx_clean_total;
}
