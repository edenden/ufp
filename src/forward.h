#ifndef _UFPD_FORWARD_H
#define _UFPD_FORWARD_H

#include "thread.h"

void forward_process(struct ufpd_thread *thread, unsigned int port_index,
	struct ufp_packet *packet, int num_packet);
void forward_process_tun(struct ufpd_thread *thread, unsigned int port_index,
	uint8_t *read_buf, unsigned int read_size);

#endif /* _UFPD_FORWARD_H */
