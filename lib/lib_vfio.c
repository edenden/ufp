#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/limits.h>
#include <linux/vfio.h>
#include <sys/eventfd.h>
#include <dirent.h>

#include "lib_main.h"
#include "lib_vfio.h"

static int ufp_vfio_iommu_type_set();
static int ufp_vfio_vendor_id(struct ufp_dev *dev);
static int ufp_vfio_device_id(struct ufp_dev *dev);
static int ufp_vfio_group_id(struct ufp_dev *dev);

static struct ufp_vfio vfio = { 0 };

int ufp_vfio_container_open()
{
	int err;

	/* Create a new container */
	vfio.container = open("/dev/vfio/vfio", O_RDWR);
	if(vfio.container < 0)
		goto err_open;

	err = ioctl(vfio.container, VFIO_GET_API_VERSION);
	if(err != VFIO_API_VERSION)
		goto err_api_version;

	err = ioctl(vfio.container,
		VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU);
	if(!err)
		goto err_check_ext;

	return 0;

err_check_ext:
err_api_version:
	close(vfio.container);
err_open:
	return -1;
}

void ufp_vfio_container_close()
{
	close(vfio.container);
	return;
}

static int ufp_vfio_iommu_type_set()
{
	struct vfio_iommu_type1_info iommu_info;
	int err;

	if(vfio.iommu_type)
		goto out;

	/* Enable the IOMMU model we want */
	err = ioctl(vfio.container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
	if(err < 0)
		goto err_set_iommu;

	/* Get addition IOMMU info */
	iommu_info.argsz = sizeof(struct vfio_iommu_type1_info);
	err = ioctl(vfio.container, VFIO_IOMMU_GET_INFO, &iommu_info);
	if(err < 0)
		goto err_get_info;

	vfio.iommu_type = VFIO_TYPE1_IOMMU;
out:
	return 0;

err_get_info:
err_set_iommu:
	return -1;
}

int ufp_vfio_dma_map(void *addr_virt, uint64_t *iova, size_t size)
{
	struct vfio_iommu_type1_dma_map dma_map;
	int err;

	/* setup a DMA mapping */
	dma_map.argsz = sizeof(struct vfio_iommu_type1_dma_map);
	dma_map.vaddr = (uint64_t)addr_virt;
	dma_map.size = size;
	/* 1:1 virtual addrress to IOVA mapping */
	dma_map.iova = (uint64_t)addr_virt;
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

	err = ioctl(vfio.container, VFIO_IOMMU_MAP_DMA, &dma_map);
	if(err < 0)
		goto err_dma_map;

	*iova = (uint64_t)dma_map.iova;
	return 0;

err_dma_map:
	return -1;
}

int ufp_vfio_dma_unmap(uint64_t iova, size_t size)
{
	struct vfio_iommu_type1_dma_unmap dma_unmap;
	int err;

	dma_unmap.argsz = sizeof(struct vfio_iommu_type1_dma_unmap);
	dma_unmap.iova = iova;
	dma_unmap.size = size;

	err = ioctl(vfio.container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
	if(err < 0)
		goto err_dma_unmap;

	return 0;

err_dma_unmap:
	return -1;
}

static int ufp_vfio_vendor_id(struct ufp_dev *dev)
{
	char path[PATH_MAX];
	FILE *file;
	unsigned int value;
	int err;

	snprintf(path, sizeof(path),
		"/sys/bus/pci/devices/%s/vendor", dev->name);

	file = fopen(path, "r");
	if(!file)
		goto err_fopen;

	err = fscanf(file, "%08x", &value);
	if(err != 1)
		goto err_fscanf;

	fclose(file);
	dev->vendor_id = value;

	return 0;

err_fscanf:
	fclose(file);
err_fopen:
	return -1;
}

static int ufp_vfio_device_id(struct ufp_dev *dev)
{
	char path[PATH_MAX];
	FILE *file;
	unsigned int value;
	int err;

	snprintf(path, sizeof(path),
		"/sys/bus/pci/devices/%s/device", dev->name);

	file = fopen(path, "r");
	if(!file)
		goto err_fopen;

	err = fscanf(file, "%08x", &value);
	if(err != 1)
		goto err_fscanf;

	fclose(file);
	dev->device_id = value;

	return 0;

err_fscanf:
	fclose(file);
err_fopen:
	return -1;
}

static int ufp_vfio_group_id(struct ufp_dev *dev)
{
	char path[PATH_MAX];
	char *linkpath;
	unsigned int group_id;
	int err;

	snprintf(path, sizeof(path),
		"/sys/bus/pci/devices/%s/iommu_group", dev->name);

	linkpath = realpath(path, NULL);
	if(!linkpath)
		goto err_readlink;

	err = sscanf(linkpath,
		"/sys/kernel/iommu_groups/%u", &group_id);
	if(err != 1)
		goto err_sscanf;

	free(linkpath);
	return group_id;

err_sscanf:
	free(linkpath);
err_readlink:
	return -1;
}

int ufp_vfio_group_open(struct ufp_dev *dev)
{
	int err, group_id, group_fd;
	char path[PATH_MAX];
	struct vfio_group_status group_status;

	group_id = ufp_vfio_group_id(dev);
	if(group_id < 0)
		goto err_group_id;

	/* Open the group */
	snprintf(path, sizeof(path), "/dev/vfio/%d", group_id);
	group_fd = open(path, O_RDWR);
	if(group_fd < 0)
		goto err_open;

	/* Test the group is viable and available */
	group_status.argsz = sizeof(struct vfio_group_status);
	err = ioctl(group_fd, VFIO_GROUP_GET_STATUS, &group_status);
	if(err < 0)
		goto err_get_status;

	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE))
		goto err_not_viable;

	if (!(group_status.flags & VFIO_GROUP_FLAGS_CONTAINER_SET)){
		/* Add the group to the container */
		err = ioctl(group_fd,
			VFIO_GROUP_SET_CONTAINER, &vfio.container);
		if(err < 0)
			goto err_set_container;
	}

	err = ufp_vfio_iommu_type_set();
	if(err < 0)
		goto err_iommu_type;

	return group_fd;

err_iommu_type:
err_set_container:
err_not_viable:
err_get_status:
	close(group_fd);
err_open:
err_group_id:
	return -1;
}

int ufp_vfio_device_open(struct ufp_dev *dev, int group_fd)
{
	int err;
	struct vfio_device_info device_info;
	struct vfio_region_info reg_info;

	/* Get a file descriptor for the device */
	dev->fd = ioctl(group_fd,
		VFIO_GROUP_GET_DEVICE_FD, dev->name);
	if(dev->fd < 0)
		goto err_open;

	/* Test and setup the device */
	device_info.argsz = sizeof(struct vfio_device_info);
	err = ioctl(dev->fd,
		VFIO_DEVICE_GET_INFO, &device_info);
	if(err < 0)
		goto err_get_info;

	if(!device_info.num_regions)
		goto err_device_info;

	reg_info.argsz = sizeof(struct vfio_region_info);
	reg_info.index = VFIO_PCI_BAR0_REGION_INDEX;
	err = ioctl(dev->fd,
		VFIO_DEVICE_GET_REGION_INFO, &reg_info);
	if(err < 0)
		goto err_reg_info;

	/* Setup mappings... read/write offsets, mmaps
	 * For PCI devices, config space is a region */
	if (!(reg_info.flags & VFIO_REGION_INFO_FLAG_MMAP))
		goto err_reg_flag;

	dev->bar_size = reg_info.size;
	dev->bar = mmap(NULL, reg_info.size, PROT_READ | PROT_WRITE,
		MAP_SHARED, dev->fd, reg_info.offset);
	if(dev->bar == MAP_FAILED)
		goto err_bar_map;

	err = ufp_vfio_vendor_id(dev);
	if(err < 0)
		goto err_vendor_id;

	err = ufp_vfio_device_id(dev);
	if(err < 0)
		goto err_device_id;

	/* Gratuitous device reset and go... */
	err = ioctl(dev->fd, VFIO_DEVICE_RESET);
	if(err < 0)
		goto err_reset;

	return 0;

err_reset:
err_device_id:
err_vendor_id:
	munmap(dev->bar, dev->bar_size);
err_bar_map:
err_reg_flag:
err_reg_info:
err_device_info:
err_get_info:
	close(dev->fd);
err_open:
	return -1;
}

int ufp_vfio_irq_set(struct ufp_dev *dev, unsigned int num_irqs)
{
	struct ufp_iface *iface;
	struct vfio_irq_info irq_info;
	struct vfio_irq_set *irq_set;
	size_t irq_set_len;
	void *irq_set_buf;
	unsigned int irq_idx = 0;
	int *irq_fd;
	int i, err;

	irq_info.argsz = sizeof(struct vfio_irq_info);
	irq_info.index = VFIO_PCI_MSIX_IRQ_INDEX;

	err = ioctl(dev->fd,
		VFIO_DEVICE_GET_IRQ_INFO, &irq_info);
	if(err < 0)
		goto err_irq_info;

	/* Setup IRQs... eventfds, VFIO_DEVICE_SET_IRQS */
	if(!(irq_info.flags & VFIO_IRQ_INFO_EVENTFD))
		goto err_irq_flag;

	irq_set_len = sizeof(struct vfio_irq_set) + (sizeof(int) * num_irqs);
	irq_set_buf = malloc(irq_set_len);
	if(!irq_set_buf)
		goto err_irq_set_buf;

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = irq_set_len;
	irq_set->count = num_irqs;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD
		| VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;
	irq_fd = (int *)&irq_set->data;

	for(i = 0; i < dev->num_misc_irqs; i++){
		irq_fd[irq_idx++] = dev->misc_irq[i]->fd;
	}
	list_for_each(&dev->iface, iface, list){
		for(i = 0; i < iface->num_qps; i++){
			irq_fd[irq_idx++] = iface->rx_irq[i]->fd;
		}
		for(i = 0; i < iface->num_qps; i++){
			irq_fd[irq_idx++] = iface->tx_irq[i]->fd;
		}
	}

	err = ioctl(dev->fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if(err < 0)
		goto err_set_irqs;

	free(irq_set_buf);
	return 0;

err_set_irqs:
	free(irq_set_buf);
err_irq_set_buf:
err_irq_flag:
err_irq_info:
	return -1;
}

int ufp_vfio_irq_unset(struct ufp_dev *dev)
{
	struct vfio_irq_set irq_set;
	int err;

	irq_set.argsz = sizeof(struct vfio_irq_set);
	irq_set.count = 0;
	irq_set.flags = VFIO_IRQ_SET_DATA_NONE
		| VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set.index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set.start = 0;

	err = ioctl(dev->fd, VFIO_DEVICE_SET_IRQS, &irq_set);
	if(err < 0)
		goto err_set_irqs;

	return 0;

err_set_irqs:
	return -1;
}

int ufp_vfio_irq_vector_get(struct ufp_dev *dev)
{
	struct ufp_iface *iface;
	struct list_head head;
	struct ufp_vfio_irq_entry *irq_entry, *_irq_entry, *temp;
	unsigned int vector;
	struct list_node *prior;
	DIR *dir;
	char path[PATH_MAX];
	struct dirent *entry;
	int i, err;

	sprintf(path, "/sys/bus/pci/devices/%s/msi_irqs",
		dev->name);
	dir = opendir(path);
	if(!dir)
		goto err_open;

	list_init(&head);
	while((entry = readdir(dir))){
		err = sscanf(entry->d_name, "%u", &vector);
		if(err != 1)
			continue;

		prior = NULL;
		list_for_each(&head, _irq_entry, list){
			if(vector > _irq_entry->vector)
				prior = &_irq_entry->list;
		}

		irq_entry = malloc(sizeof(struct ufp_vfio_irq_entry));
		if(!irq_entry)
			goto err_alloc_irq_entry;
		irq_entry->vector = vector;

		prior ? list_add_after(prior, &irq_entry->list)
			: list_add_first(&head, &irq_entry->list);
	}
	closedir(dir);

	for(i = 0; i < dev->num_misc_irqs; i++){
		irq_entry = list_first_entry(&head,
			struct ufp_vfio_irq_entry, list);
		if(!irq_entry)
			goto err_no_rest_irq;

		dev->misc_irq[i]->vector = irq_entry->vector;
		list_del(&irq_entry->list);
	}

	list_for_each(&dev->iface, iface, list){
		for(i = 0; i < iface->num_qps; i++){
			irq_entry = list_first_entry(&head,
				struct ufp_vfio_irq_entry, list);
			if(!irq_entry)
				goto err_no_rest_irq;

			iface->rx_irq[i]->vector = vector;
			list_del(&irq_entry->list);
		}
		for(i = 0; i < iface->num_qps; i++){
			irq_entry = list_first_entry(&head,
				struct ufp_vfio_irq_entry, list);
			if(!irq_entry)
				goto err_no_rest_irq;

			iface->tx_irq[i]->vector = vector;
			list_del(&irq_entry->list);
		}
	}

	list_for_each_safe(&head, irq_entry, list, temp){
		list_del(&irq_entry->list);
	}
	return 0;

err_no_rest_irq:
err_alloc_irq_entry:
	list_for_each_safe(&head, irq_entry, list, temp){
		list_del(&irq_entry->list);
	}
err_open:
	return -1;
}

