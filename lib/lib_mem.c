#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <net/ethernet.h>

#include "lib_main.h"
#include "lib_mem.h"
#include "lib_list.h"

static struct ufp_mnode *ufp_mnode_alloc(struct ufp_mnode *parent,
	void *ptr, size_t size, unsigned int index);
static void ufp_mnode_release(struct ufp_mnode *node);
static void _ufp_mem_destroy(struct ufp_mnode *node);
static struct ufp_mnode *_ufp_mem_alloc(struct ufp_mnode *node,
	size_t size);

static struct ufp_mnode *ufp_mnode_alloc(struct ufp_mnode *parent,
	void *ptr, size_t size, unsigned int index)
{
	struct ufp_mnode *node;

	node = malloc(sizeof(struct ufp_mnode));
	if(!node)
		goto err_alloc_node;

	node->parent	= parent;
	node->child[0]	= NULL;
	node->child[1]	= NULL;
	node->flag	= 0;
	node->index	= index;
	node->size	= size;
	node->ptr	= ptr;

	return node;

err_alloc_node:
	return NULL;
}

static void ufp_mnode_release(struct ufp_mnode *node)
{
	free(node);
	return;
}

struct ufp_mnode *ufp_mem_init(void *ptr, size_t size)
{
	struct ufp_mnode *root;

	root = ufp_mnode_alloc(NULL, ptr, size, 0);
	if(!root)
		goto err_alloc_root;

	return root;

err_alloc_root:
	return NULL;
}

void ufp_mem_destroy(struct ufp_mnode *node)
{
	_ufp_mem_destroy(node);
}

static void _ufp_mem_destroy(struct ufp_mnode *node)
{
	struct ufp_mnode *child;
	int i;

	for(i = 0; i < 2; i++){
		child = node->child[i];
		if(child)
			_ufp_mem_destroy(child);
	}

	ufp_mnode_release(node);
	return;
}

void *ufp_mem_alloc(struct ufp_mpool *mpool, size_t size)
{
	struct ufp_mnode *node;
	void **header;
	size_t size_header;

	size_header = sizeof(void *);

	node = _ufp_mem_alloc(mpool->node, size_header + size);
	if(!node)
		goto err_alloc;

	header = node->ptr;
	*header = node;
	return node->ptr + size_header;

err_alloc:
	return NULL;
}

void *ufp_mem_alloc_align(struct ufp_mpool *mpool, size_t size,
	size_t align)
{
	struct ufp_mnode *node;
	void **header;
	size_t size_header, offset;

	size_header = sizeof(void *);

	node = _ufp_mem_alloc(mpool->node, align + size_header + size);
	if(!node)
		goto err_alloc;

	offset = align - (((size_t)node->ptr + size_header) % align);
	header = node->ptr + offset;
	*header = node;
	return node->ptr + offset + size_header;

err_alloc:
	return NULL;
}

static struct ufp_mnode *_ufp_mem_alloc(struct ufp_mnode *node,
	size_t size)
{
	void *ptr_new;
	struct ufp_mnode *ret;
	size_t size_new;
	int i, buddy_allocated;

	ret = NULL;
	if(node->flag & UFP_MNODE_FULL)
		goto out;

	if((node->size >> 1) < size){
		if(!(node->flag & UFP_MNODE_ALLOCATED)
		&& node->size >= size){
			node->flag |= UFP_MNODE_FULL;
			ret = node;
		}
	}else{
		if(!(node->flag & UFP_MNODE_ALLOCATED)){
			size_new = node->size >> 1;
			for(i = 0, buddy_allocated = 0; i < 2; i++, buddy_allocated++){
				ptr_new = node->ptr + (size_new * i);

				node->child[i] = ufp_mnode_alloc(node, ptr_new, size_new, i);
				if(!node->child[i])
					goto err_alloc_child;
			}
		}

		for(i = 0; i < 2; i++){
			ret = _ufp_mem_alloc(node->child[i], size);
			if(ret){
				if(node->child[0]->flag & UFP_MNODE_FULL
				&& node->child[1]->flag & UFP_MNODE_FULL)
					node->flag |= UFP_MNODE_FULL;
				break;
			}
		}
	}

	if(ret)
		node->flag |= UFP_MNODE_ALLOCATED;

out:
	return ret;

err_alloc_child:
	for(i = 0; i < buddy_allocated; i++){
		ufp_mnode_release(node->child[i]);
		node->child[i] = NULL;
	}
	return NULL;
}

void ufp_mem_free(void *addr_free)
{
	struct ufp_mnode *node;
	void **header;
	size_t size_header;

	size_header = sizeof(void *);
	header = addr_free - size_header;
	node = (struct ufp_mnode *)*header;
	_ufp_mem_free(node);
	return;
}

void _ufp_mem_free(struct ufp_mnode *node)
{
	struct ufp_mnode *parent, *buddy;

	node->flag = 0;

	parent = node->parent;
	if(!parent)
		goto out;
	parent->flag &= ~UFP_MNODE_FULL;

	buddy = parent->child[!node->index];
	if(buddy->flag & UFP_MNODE_ALLOCATED)
		goto out;

	ufp_mnode_release(buddy);
	ufp_mnode_release(node);
	parent->child[0] = NULL;
	parent->child[1] = NULL;
	_ufp_mem_free(parent);

out:
	return;
}

void ufp_mem_free_delay(struct ufp_mpool *mpool, void *addr_free)
{
	struct ufp_mnode *node;
	void **header;
	size_t size_header;

	size_header = sizeof(void *);
	header = addr_free - size_header;
	node = (struct ufp_mnode *)*header;
	list_add_last(&mpool->head, &node->list);
	return;
}
