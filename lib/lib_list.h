#include <stddef.h>

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type,member));	\
	})

struct list_node {
	struct list_node	*next;
	struct list_node	*prev;
};

struct list_head {
	struct list_node	node;
};

/* '**prev' points '*first' or '*next' of previous node */
struct hlist_node {
	struct hlist_node	*next;
	struct hlist_node	**prev;
};

struct hlist_head {
	struct hlist_node	*first;
};

static inline void list_add_before(struct list_node *node,
	struct list_node *new)
{
	new->next = node;
	new->prev = node->prev;
	node->prev->next = new;
	node->prev = new;
	return;
}

static inline void list_add_after(struct list_node *node,
	struct list_node *new)
{
	new->next = node->next;
	new->prev = node;
	node->next->prev = new;
	node->next = new;
	return;
}

static inline void list_add_first(struct list_head *head,
	struct list_node *new)
{
	list_add_after(&head->node, new);
}

static inline void list_add_last(struct list_head *head,
	struct list_node *new)
{
	list_add_before(&head->node, new);
}

static inline void list_del(struct list_node *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
	return;
}

static inline void list_del_first(struct list_head *head)
{
	list_del(head->node.next);
}

static inline void list_del_last(struct list_head *head)
{
	list_del(head->node.prev);
}

static inline int list_empty(struct list_head *head)
{
	return head->node.next == &head->node;
}

static inline void list_init(struct list_head *head)
{
	head->node.next = &head->node;
	head->node.prev = &head->node;
	return;
}

#define LIST_HEAD_INIT(name) { { &(name).node, &(name).node } }
#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define list_entry(ptr, type, member)					\
	container_of(ptr, type, member)

#define list_for_each(head, data, member)				\
	list_for_each_dir(head, data, member, next)

#define list_for_each_rev(head, data, member)				\
	list_for_each_dir(head, data, member, prev)

#define list_for_each_dir(head, data, member, dir)			\
	for(	(data) = list_entry((head)->node.dir,			\
			typeof(*(data)),member);			\
		&((data)->member) != &((head)->node);			\
		(data) = list_entry((data)->member.dir,			\
			typeof(*(data)), member))

#define list_for_each_safe(head, data, member, temp)			\
	list_for_each_safe_dir(head, data, member, temp, next)

#define list_for_each_safe_rev(head, data, member, temp)		\
	list_for_each_safe_dir(head, data, member, temp, prev)

#define list_for_each_safe_dir(head, data, member, temp, dir)		\
	for(	(data) = list_entry((head)->node.dir,			\
			typeof(*(data)), member),			\
		(temp) = list_entry((data)->member.dir,			\
			typeof(*(data)),member);			\
		&((data)->member) != &((head)->node);			\
		(data) = (temp),					\
		(temp) = list_entry((data)->member.dir,			\
			typeof(*(data)), member))

static inline void hlist_add_before(struct hlist_node *node,
	struct hlist_node *new)
{
	new->next = node;
	new->prev = node->prev;
	node->prev = &new->next;
	*(new->prev) = new;
	return;
}

static inline void hlist_add_after(struct hlist_node *node,
	struct hlist_node *new)
{
	new->next = node->next;
	new->prev = &node->next;
	if(new->next)
		new->next->prev = &new->next;
	node->next = new;
	return;
}

static inline void hlist_add_first(struct hlist_head *head,
	struct hlist_node *new)
{
	new->next = head->first;
	new->prev = &head->first;
	if(head->first)
		head->first->prev = &new->next;
	head->first = new;
	return;
}

static inline void hlist_del(struct hlist_node *node)
{
	if(node->next)
		node->next->prev = node->prev;
	*(node->prev) = node->next;
	return;
}

static inline void hlist_del_first(struct hlist_head *head)
{
	hlist_del(head->first);
	return;
}

static inline int hlist_empty(struct hlist_head *head)
{
	return !head->first;
}

static inline void hlist_init(struct hlist_head *head)
{
	head->first = NULL;
	return;
}

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) \
	struct hlist_head name = HLIST_HEAD_INIT(name)

#define hlist_entry(ptr, type, member)					\
	container_of(ptr, type, member)

#define hlist_entry_safe(ptr, type, member)				\
	(ptr) ? hlist_entry(ptr, type, member) : NULL

#define hlist_for_each(head, data, member)				\
	for(	(data) = hlist_entry_safe((head)->first,		\
			typeof(*(data)), member);			\
		(data);							\
		(data) = hlist_entry_safe((data)->member.next,		\
			typeof(*(data)), member))

#define hlist_for_each_safe(head, data, member, temp)			\
	for(	(data) = hlist_entry_safe((head)->first,		\
			typeof(*(data)), member),			\
		(data)							\
		&& ((temp) = hlist_entry_safe((data)->member.next,	\
			typeof(*(data)), member));			\
		(data);							\
		(data) = (temp),					\
		(data)							\
		&& ((temp) = hlist_entry_safe((data)->member.next,	\
			typeof(*(data)), member)))

