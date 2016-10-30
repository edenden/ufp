#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <net/ethernet.h>

#include "lib_main.h"
#include "lib_mem.h"

static struct ufp_mnode *ufp_mnode_alloc(struct ufp_mnode *parent,
	void *ptr, unsigned int size, unsigned int index);
static void ufp_mnode_release(struct ufp_mnode *node);
static void _ufp_mem_destroy(struct ufp_mnode *node);
static struct ufp_mnode *_ufp_mem_alloc(struct ufp_mnode *node,
	unsigned int size);
static void _ufp_mem_free(struct ufp_mnode *node);

static struct ufp_mnode *ufp_mnode_alloc(struct ufp_mnode *parent,
	void *ptr, unsigned int size, unsigned int index)
{
	struct ufp_mnode *node;

	node = malloc(sizeof(struct ufp_mnode));
	if(!node)
		goto err_alloc_node;

	node->parent	= parent;
	node->child[0]	= NULL;
	node->child[1]	= NULL;
	node->allocated	= 0;
	node->index	= index;
	node->size	= size;
	node->ptr	= ptr;

	return node;

err_alloc_node:
	return NULL;
}

static void ufp_mnode_release(struct ufp_mnode *node)
{
	struct ufp_mnode *parent;

	parent = node->parent;
	if(parent)
		parent->child[node->index] = NULL;

	free(node);
	return;
}

struct ufp_mnode *ufp_mem_init(void *ptr, unsigned int size)
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

void *ufp_mem_alloc(struct ufp_mpool *mpool, unsigned int size)
{
	struct ufp_mnode *node;
	unsigned long *header;

	node = _ufp_mem_alloc(mpool->node,
		ALIGN(size, L1_CACHE_BYTES) +
		ALIGN(sizeof(unsigned long), L1_CACHE_BYTES));

	if(!node)
		goto err_alloc;

	header = node->ptr;
	*header = (unsigned long)node;
	return node->ptr + ALIGN(sizeof(unsigned long), L1_CACHE_BYTES);

err_alloc:
	return NULL;
}

static struct ufp_mnode *_ufp_mem_alloc(struct ufp_mnode *node,
	unsigned int size)
{
	void *ptr_new;
	struct ufp_mnode *ret;
	unsigned int size_new;
	int i, buddy_allocated;

	ret = NULL;

	if(!node)
		goto ign_node;

	if((node->size >> 1) < size){
		if(!node->allocated
		&& node->size >= size){
			ret = node;
		}
	}else{
		if(!node->allocated){
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
			if(ret)
				break;
		}
	}

	if(ret)
		node->allocated = 1;

ign_node:
	return ret;

err_alloc_child:
	for(i = 0; i < buddy_allocated; i++)
		ufp_mnode_release(node->child[i]);
	return NULL;
}

void ufp_mem_free(void *addr_free)
{
	struct ufp_mnode *node;
	unsigned long *header;

	header = addr_free - ALIGN(sizeof(unsigned long), L1_CACHE_BYTES);
	node = (struct ufp_mnode *)*header;
	_ufp_mem_free(node);
}

static void _ufp_mem_free(struct ufp_mnode *node)
{
	struct ufp_mnode *parent, *buddy;

	node->allocated = 0;
	
	parent = node->parent;
	if(!parent)
		goto out;

	buddy = parent->child[!node->index];
	if(buddy->allocated)
		goto out;

	ufp_mnode_release(buddy);
	ufp_mnode_release(node);
	_ufp_mem_free(parent);

	return;

out:
	return;
}
