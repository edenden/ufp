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

int vfio_container_open();
void vfio_container_close();
int vfio_dma_map(struct ufp_dev *dev,
	void *addr_virt, uint64_t *iova, size_t size);
int vfio_dma_unmap(struct ufp_dev *dev,
	uint64_t iova, size_t size);
int vfio_group_open(struct ufp_dev *dev);
int vfio_device_open(struct ufp_dev *dev, int group_fd);
int vfio_irq_set(struct ufp_dev *dev, unsigned int num_irqs);
int vfio_irq_unset(struct ufp_dev *dev);
int vfio_irq_vector_get(struct ufp_dev *dev);

#endif /* _UFP_VFIO_H */
