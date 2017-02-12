#ifndef _LIBUFP_MEM_H
#define _LIBUFP_MEM_H

struct ufp_mnode {
	struct ufp_mnode	*child[2];
	struct ufp_mnode	*parent;
	unsigned int		allocated;
	unsigned int		index;
	unsigned int		size;
	void			*ptr;
};

struct ufp_mnode *ufp_mem_init(void *ptr, unsigned int size);
void ufp_mem_destroy(struct ufp_mnode *node);
void *ufp_mem_alloc(struct ufp_mpool *mpool, unsigned int size);
void ufp_mem_free(void *addr_free);

#endif /* _LIBUFP_MEM_H */
