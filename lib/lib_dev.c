#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "lib_main.h"
#include "lib_list.h"
#include "lib_vfio.h"
#include "lib_dev.h"

static int ufp_dev_register_device(struct pci_driver *driver, int idx);
static void ufp_dev_unregister_device(struct pci_driver *driver, int idx);

LIST_HEAD(dylibs);
LIST_HEAD(pci_entries);

struct ufp_dev_buf *ufp_dev_dma_alloc(size_t size)
{
	struct ufp_dev_buf *buf;
	size_t size_align;
	int err;

	buf = malloc(sizeof(struct ufp_dev_buf));
	if(!buf)
		goto err_alloc_buf;

	buf->size = size;
	size_align = ALIGN(size, getpagesize());

	buf->addr_virt = mmap(NULL, size_align,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED, -1, 0);
	if(buf->addr_virt == MAP_FAILED){
		goto err_mmap;
	}

	err = ufp_vfio_dma_map(buf->addr_virt, &buf->addr_dma, size_align);
	if(err < 0){
		goto err_dma_map;
	}
	return buf;

err_dma_map:
	munmap(buf->addr_virt, size_align);
err_mmap:
	free(buf);
err_alloc_buf:
	return NULL;
}

void ufp_dev_dma_free(struct ufp_dev_buf *buf)
{
	size_t size_align;

	size_align = ALIGN(buf->size, getpagesize());
	ufp_vfio_dma_unmap(buf->addr_dma, size_align);
	munmap(buf->addr_virt, size_align);
	return;
}

int ufp_dev_init(struct ufp_dev *dev, struct ufp_ops *ops)
{
	struct pci_entry *entry;
	int err;

	list_for_each(&pci_entries, entry, list){
		if(entry->vendor_id == dev->vendor_id
		&& entry->device_id == dev->device_id){
			err = entry->init(dev, ops);
			if(err)
				goto err_probe_failed;
			return 0;
		}
	}

err_probe_failed:
	return -1;
}

int ufp_dev_destroy(struct ufp_dev *dev, struct ufp_ops *ops)
{
	struct pci_entry *entry;

	list_for_each(&pci_entries, entry, list){
		if(entry->vendor_id == dev->vendor_id
		&& entry->device_id == dev->device_id){
			entry->destroy(dev, ops);
			return 0;
		}
	}

	return -1;
}

static int ufp_dev_register_device(struct pci_driver *driver, int idx)
{
	struct pci_entry *entry, *new_entry;
	const struct pci_id *target;

	target = &driver->id_table[idx];

	list_for_each(&pci_entries, entry, list){
		if(entry->vendor_id == target->vendor_id
		&& entry->device_id == target->device_id)
			goto err_id_exist;
	}

	new_entry = malloc(sizeof(struct pci_entry));
	if(!new_entry)
		goto err_alloc_entry;

	new_entry->vendor_id = target->vendor_id;
	new_entry->device_id = target->device_id;
	new_entry->init = driver->init;
	new_entry->destroy = driver->destroy;

	list_add_last(&pci_entries, &new_entry->list);
	return 0;

err_alloc_entry:
err_id_exist:
	return -1;
}

static void ufp_dev_unregister_device(struct pci_driver *driver, int idx)
{
	struct pci_entry *entry, *temp;
	const struct pci_id *target;

	target = &driver->id_table[idx];

	list_for_each_safe(&pci_entries, entry, list, temp){
		if(entry->vendor_id == target->vendor_id
		&& entry->device_id == target->device_id){
			list_del(&entry->list);
			free(entry);
			break;
		}
	}

	return;
}

int ufp_dev_register(struct pci_driver *driver)
{
	int err, i;
	unsigned int registered = 0;

	for(i = 0; driver->id_table[i].device_id; i++, registered++){
		err = ufp_dev_register_device(driver, i);
		if(err)
			goto err_register_handler;
	}

	return 0;

err_register_handler:
	for(i = 0; i < registered; i++){
		ufp_dev_unregister_device(driver, i);
	}
	return -1;
}

void ufp_dev_unregister(struct pci_driver *driver)
{
	int i;

	/* TBD: Avoid to unregister another driver's entry */
	for(i = 0; driver->id_table[i].device_id; i++){
		ufp_dev_unregister_device(driver, i);
	}

	return;
}

int ufp_dev_load_lib(char *dir_path)
{
	DIR *dir;
	struct dirent *entry;
	char *ext, path[PATH_MAX];
	struct dylib *dylib;

	dir = opendir(dir_path);
	if(!dir)
		goto err_opendir;

	while((entry = readdir(dir))){
		ext = strrchr(entry->d_name, '.');
		if(ext && !strcmp(ext, ".so")){
			sprintf(path, "%s/%s", dir_path, entry->d_name);

			dylib = malloc(sizeof(struct dylib));
			if(!dylib)
				goto err_alloc_dylib;;

			dylib->handle = dlopen(path, RTLD_LAZY);
			if(!dylib->handle)
				goto err_dlopen;

			list_add_last(&dylibs, &dylib->list);
		}
		continue;
err_dlopen:
	free(dylib);
err_alloc_dylib:
	goto err_open;
	}

	closedir(dir);
	return 0;

err_open:
	ufp_dev_unload_lib();
	closedir(dir);
err_opendir:
	return -1;
}

void ufp_dev_unload_lib()
{
	struct dylib *dylib, *temp;

	list_for_each_safe(&dylibs, dylib, list, temp){
		dlclose(dylib->handle);
		list_del(&dylib->list);
		free(dylib);
	}

	return;
}

