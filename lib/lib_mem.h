#ifndef _LIBUFP_MEM_H
#define _LIBUFP_MEM_H

struct ufp_mnode {
	struct ufp_mnode	*child[2];
	struct ufp_mnode	*parent;
	unsigned int		allocated;
	unsigned int		index;
	size_t			size;
	void			*ptr;
	struct list_node	list; /* for delayed release */
};

struct ufp_mnode *ufp_mem_init(void *ptr, size_t size);
void ufp_mem_destroy(struct ufp_mnode *node);
void *ufp_mem_alloc(struct ufp_mpool *mpool, size_t size);
void *ufp_mem_alloc_align(struct ufp_mpool *mpool, size_t size,
	size_t align);
void ufp_mem_free(void *addr_free);
void _ufp_mem_free(struct ufp_mnode *node);

#endif /* _LIBUFP_MEM_H */
