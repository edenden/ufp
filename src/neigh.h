#ifndef _UFPD_NEIGH_H
#define _UFPD_NEIGH_H

#include <linux/if_ether.h>
#include <pthread.h>
#include <ufp.h>
#include "hash.h"

#define GOLDEN_RATIO_PRIME_32 0x9e370001UL
#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001UL

struct neigh_table {
	struct hash_table	table;
};

struct neigh_entry {
	struct hash_entry	hash;
	uint8_t			dst_mac[ETH_ALEN];
	uint32_t		dst_addr[4];
};

struct neigh_table *neigh_alloc(struct ufp_mpool *mpool, int family);
void neigh_release(struct neigh_table *neigh);
int neigh_add(struct neigh_table *neigh, int family,
	void *dst_addr, void *mac_addr, struct ufp_mpool *mpool);
int neigh_delete(struct neigh_table *neigh, int family,
	void *dst_addr);
struct neigh_entry *neigh_lookup(struct neigh_table *neigh,
	void *dst_addr);

#endif /* _UFPD_NEIGH_H */
