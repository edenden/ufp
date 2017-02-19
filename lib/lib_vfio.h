#ifndef _UFP_VFIO_H
#define _UFP_VFIO_H

struct ufp_vfio {
	int container;
	int iommu_type;
};

struct ufp_vfio_irq_entry {
	struct list_node list;
	unsigned int vector;
};

int ufp_vfio_container_open();
void ufp_vfio_container_close();
int ufp_vfio_dma_map(void *addr_virt, uint64_t *iova, size_t size);
int ufp_vfio_dma_unmap(uint64_t iova, size_t size);
int ufp_vfio_group_open(struct ufp_dev *dev);
int ufp_vfio_device_open(struct ufp_dev *dev, int group_fd);
int ufp_vfio_irq_set(struct ufp_dev *dev, unsigned int num_irqs);
int ufp_vfio_irq_unset(struct ufp_dev *dev);
int ufp_vfio_irq_vector_get(struct ufp_dev *dev);

#endif /* _UFP_VFIO_H */
