#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include <ufp.h>
#include "main.h"
#include "hash.h"

static void hash_entry_init(struct hash_entry *entry,
	void *key);

void hash_init(struct hash_table *table)
{
	int i;

	for(i = 0; i < HASH_SIZE; i++){
		hlist_init(&table->head[i]);
	}

	return;
}

static void hash_entry_init(struct hash_entry *entry,
	void *key)
{
	entry->key = key;
	return;
}

int hash_add(struct hash_table *table, void *key,
	struct hash_entry *entry_new)
{
	struct hlist_head *head;
	struct hash_entry *entry;
	unsigned int hash_key;

	hash_entry_init(entry_new, key);
	hash_key = table->hash_key_generate(key, HASH_BIT);
	head = &table->head[hash_key];

	hlist_for_each(head, entry, list){
		if(!table->hash_key_compare(key, entry->key)){
			goto err_entry_exist;
		}
	}

	hlist_add_first(head, &entry_new->list);

	return 0;

err_entry_exist:
	return -1;
}

int hash_delete(struct hash_table *table,
	void *key)
{
	struct hlist_head *head;
	struct hash_entry *entry, *target;
	unsigned int hash_key;

	target = NULL;
	hash_key = table->hash_key_generate(key, HASH_BIT);
	head = &table->head[hash_key];

	hlist_for_each(head, entry, list){
		if(!table->hash_key_compare(key, entry->key)){
			target = entry;
			break;
		}
	}

	if(!target)
		goto err_not_found;

	hlist_del(&target->list);
	table->hash_entry_delete(target);

	return 0;

err_not_found:
	return -1;
}

void hash_delete_all(struct hash_table *table)
{
	struct hlist_head *head;
	struct hash_entry *entry, *temp;
	unsigned int i;

	for(i = 0; i < HASH_SIZE; i++){
		head = &table->head[i];
		hlist_for_each_safe(head, entry, list, temp){
			hlist_del(&entry->list);
			table->hash_entry_delete(entry);
		}
	}

	return;
}

struct hash_entry *hash_lookup(struct hash_table *table,
	void *key)
{
	struct hlist_head *head;
	struct hash_entry *entry, *_entry;
	unsigned int hash_key;

	entry = NULL;
	hash_key = table->hash_key_generate(key, HASH_BIT);
	head = &table->head[hash_key];

	hlist_for_each(head, _entry, list){
		if(!table->hash_key_compare(key, _entry->key)){
			entry = _entry;
			break;
		}
	}

        return entry;
}

