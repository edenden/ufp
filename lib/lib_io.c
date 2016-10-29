#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <net/ethernet.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <pthread.h>

#include "lib_main.h"
#include "lib_rtx.h"

static inline uint16_t ufp_desc_unused(struct ufp_ring *ring,
	uint16_t num_desc);
static inline void ufp_write_tail(struct ufp_ring *ring, uint32_t value);
inline int ufp_slot_assign(struct ufp_buf *buf,
	struct ufp_plane *plane, unsigned int port_index);
static inline void ufp_slot_attach(struct ufp_ring *ring,
	uint16_t desc_index, int slot_index);
static inline int ufp_slot_detach(struct ufp_ring *ring,
	uint16_t desc_index);
inline void ufp_slot_release(struct ufp_buf *buf,
	int slot_index);
static inline unsigned long ufp_slot_addr_dma(struct ufp_buf *buf,
	int slot_index, int port_index);
inline void *ufp_slot_addr_virt(struct ufp_buf *buf,
	uint16_t slot_index);

static inline uint16_t ufp_desc_unused(struct ufp_ring *ring,
	uint16_t num_desc)
{
	uint16_t next_to_clean = ring->next_to_clean;
	uint16_t next_to_use = ring->next_to_use;

	return next_to_clean > next_to_use
		? next_to_clean - next_to_use - 1
		: (num_desc - next_to_use) + next_to_clean - 1;
}

static inline void ufp_write_tail(struct ufp_ring *ring, uint32_t value)
{
	ufp_writel(value, ring->tail);
	return;
}

void ufp_irq_unmask_queues(struct ufp_plane *plane,
	unsigned int port_index, struct ufp_irq_handle *irqh)
{
	struct ufp_port *port;

	port = &plane->ports[port_index];
	port->ops->unmask_queues(port->bar, irqh->qmask);

	return;
}

void ufp_rx_assign(struct ufp_plane *plane, unsigned int port_index,
	struct ufp_buf *buf)
{
	struct ufp_port *port;
	struct ufp_ring *rx_ring;
	unsigned int total_allocated;
	uint16_t max_allocation;

	port = &plane->ports[port_index];
	rx_ring = port->rx_ring;

	max_allocation = ufp_desc_unused(rx_ring, port->num_rx_desc);
	if (!max_allocation)
		return;

	total_allocated = 0;
	while(likely(total_allocated < max_allocation)){
		uint16_t next_to_use;
		uint64_t addr_dma;
		int slot_index;

		slot_index = ufp_slot_assign(buf, plane, port_index);
		if(unlikely(slot_index < 0)){
			port->count_rx_alloc_failed +=
				(max_allocation - total_allocated);
			break;
		}

		addr_dma = (uint64_t)ufp_slot_addr_dma(buf, slot_index, port_index);
		port->ops->set_rx_desc(rx_ring, rx_ring->next_to_use, addr_dma);
		ufp_slot_attach(rx_ring, rx_ring->next_to_use, slot_index);

		next_to_use = rx_ring->next_to_use + 1;
		rx_ring->next_to_use =
			(next_to_use < port->num_rx_desc) ? next_to_use : 0;

		total_allocated++;
	}

	if(likely(total_allocated)){
		/*
		 * Force memory writes to complete before letting h/w
		 * know there are new descriptors to fetch.  (Only
		 * applicable for weak-ordered memory model archs,
		 * such as IA-64).
		 */
		/* XXX: Do we need this write memory barrier ? */
		wmb();
		ufp_write_tail(rx_ring, rx_ring->next_to_use);
	}
}

void ufp_tx_assign(struct ufp_plane *plane, unsigned int port_index,
	struct ufp_buf *buf, struct ufp_packet *packet)
{
	struct ufp_port *port;
	struct ufp_ring *tx_ring;
	uint16_t unused_count;
	uint16_t next_to_use;
	uint64_t addr_dma;

	port = &plane->ports[port_index];
	tx_ring = port->tx_ring;

	unused_count = ufp_desc_unused(tx_ring, port->num_tx_desc);
	if(unlikely(!unused_count)){
		port->count_tx_xmit_failed++;
		ufp_slot_release(buf, packet->slot_index);
		return;
	}

	addr_dma = (uint64_t)ufp_slot_addr_dma(buf, packet->slot_index, port_index);
	port->ops->set_tx_desc(tx_ring, tx_ring->next_to_use,
		addr_dma, packet);
	ufp_slot_attach(tx_ring, tx_ring->next_to_use, packet->slot_index);
	ufp_print("Tx: packet sending DMAaddr = %p size = %d\n",
		(void *)addr_dma, packet->slot_size);

	next_to_use = tx_ring->next_to_use + 1;
	tx_ring->next_to_use =
		(next_to_use < port->num_tx_desc) ? next_to_use : 0;

	port->tx_suspended++;
	return;
}

void ufp_tx_xmit(struct ufp_plane *plane, unsigned int port_index)
{
	struct ufp_port *port;
	struct ufp_ring *tx_ring;

	port = &plane->ports[port_index];
	tx_ring = port->tx_ring;

	if(port->tx_suspended){
		/*
		 * Force memory writes to complete before letting h/w know there
		 * are new descriptors to fetch.  (Only applicable for weak-ordered
		 * memory model archs, such as IA-64).
		 *
		 * We also need this memory barrier to make certain all of the
		 * status bits have been updated before next_to_watch is written.
		 */
		wmb();
		ufp_write_tail(tx_ring, tx_ring->next_to_use);
		port->tx_suspended = 0;
	}

	return;
}

unsigned int ufp_rx_clean(struct ufp_plane *plane, unsigned int port_index,
	struct ufp_buf *buf, struct ufp_packet *packet)
{
	struct ufp_port *port;
	struct ufp_ring *rx_ring;
	unsigned int total_rx_packets;
	int err;

	port = &plane->ports[port_index];
	rx_ring = port->rx_ring;

	total_rx_packets = 0;
	while(likely(total_rx_packets < port->rx_budget)){
		uint16_t next_to_clean;
		int slot_index;

		if(unlikely(rx_ring->next_to_clean == rx_ring->next_to_use)){
			break;
		}

		err = port->ops->check_rx_desc(rx_ring, rx_ring->next_to_clean);
		if(unlikely(err < 0))
			break;

		/*
		 * This memory barrier is needed to keep us from reading
		 * any other fields out of the rx_desc until we know the
		 * RXD_STAT_DD bit is set
		 */
		rmb();

		port->ops->get_rx_desc(rx_ring, rx_ring->next_to_clean,
			&packet[total_rx_packets]);
		ufp_print("Rx: packet received size = %d\n", slot_size);

		/* retrieve a buffer address from the ring */
		slot_index = ufp_slot_detach(rx_ring, rx_ring->next_to_clean);
		packet[total_rx_packets].slot_index = slot_index;
		packet[total_rx_packets].slot_buf =
			ufp_slot_addr_virt(buf, slot_index);

		next_to_clean = rx_ring->next_to_clean + 1;
		rx_ring->next_to_clean = 
			(next_to_clean < port->num_rx_desc) ? next_to_clean : 0;

		total_rx_packets++;
	}

	port->count_rx_clean_total += total_rx_packets;
	return total_rx_packets;
}

void ufp_tx_clean(struct ufp_plane *plane, unsigned int port_index,
	struct ufp_buf *buf)
{
	struct ufp_port *port;
	struct ufp_ring *tx_ring;
	unsigned int total_tx_packets;
	int err;

	port = &plane->ports[port_index];
	tx_ring = port->tx_ring;

	total_tx_packets = 0;
	while(likely(total_tx_packets < port->tx_budget)){
		uint16_t next_to_clean;
		int slot_index;

		if(unlikely(tx_ring->next_to_clean == tx_ring->next_to_use)){
			break;
		}

		err = port->ops->check_tx_desc(tx_ring, tx_ring->next_to_clean);
		if(unlikely(err < 0))
			break;

		/* Release unused buffer */
		slot_index = ufp_slot_detach(tx_ring, tx_ring->next_to_clean);
		ufp_slot_release(buf, slot_index);

		next_to_clean = tx_ring->next_to_clean + 1;
		tx_ring->next_to_clean =
			(next_to_clean < port->num_tx_desc) ? next_to_clean : 0;

		total_tx_packets++;
	}

	port->count_tx_clean_total += total_tx_packets;
	return;
}

inline int ufp_slot_assign(struct ufp_buf *buf,
	struct ufp_plane *plane, unsigned int port_index)
{
	struct ufp_port *port;
	int slot_next, slot_index, i;

	port = &plane->ports[port_index];
	slot_next = port->rx_slot_next;

	for(i = 0; i < buf->count; i++){
		slot_index = port->rx_slot_offset + slot_next;
		if(!(buf->slots[slot_index] & UFP_SLOT_INFLIGHT)){
			goto out;
		}

		slot_next++;
		if(slot_next == buf->count)
			slot_next = 0;
	}

	return -1;

out:
	port->rx_slot_next = slot_next + 1;
	if(port->rx_slot_next == buf->count)
		port->rx_slot_next = 0;

	buf->slots[slot_index] |= UFP_SLOT_INFLIGHT;
	return slot_index;
}

static inline void ufp_slot_attach(struct ufp_ring *ring,
	uint16_t desc_index, int slot_index)
{
	ring->slot_index[desc_index] = slot_index;
	return;
}

static inline int ufp_slot_detach(struct ufp_ring *ring,
	uint16_t desc_index)
{
	return ring->slot_index[desc_index];
}

inline void ufp_slot_release(struct ufp_buf *buf,
	int slot_index)
{
	buf->slots[slot_index] = 0;
	return;
}

static inline unsigned long ufp_slot_addr_dma(struct ufp_buf *buf,
	int slot_index, int port_index)
{
	return buf->addr_dma[port_index] + (buf->buf_size * slot_index);
}

inline void *ufp_slot_addr_virt(struct ufp_buf *buf,
	uint16_t slot_index)
{
	return buf->addr_virt + (buf->buf_size * slot_index);
}

inline unsigned int ufp_slot_size(struct ufp_buf *buf)
{
	return buf->buf_size;
}
