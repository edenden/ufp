#ifndef _UFP_DMA_H
#define _UFP_DMA_H

u8 __iomem *ufp_dma_map_iobase(struct ufp_device *device);
dma_addr_t ufp_dma_map(struct ufp_device *device,
	unsigned long addr_virtual, unsigned long size, u8 cache);
int ufp_dma_unmap(struct ufp_device *device, unsigned long addr_dma);
void ufp_dma_unmap_all(struct ufp_device *device);
struct ufp_dma_area *ufp_dma_area_lookup(struct ufp_device *device,
	unsigned long addr_dma);

enum {
	IXGBE_DMA_CACHE_DEFAULT = 0,
	IXGBE_DMA_CACHE_DISABLE,
	IXGBE_DMA_CACHE_WRITECOMBINE
};

struct ufp_dma_area {
	struct list_head	list;
	atomic_t		refcount;
	unsigned long		size;
	u8			cache;
	dma_addr_t		addr_dma;
	enum dma_data_direction direction;

	struct page		**pages;
	struct sg_table		*sgt;
	unsigned int		npages;
};

#endif /* _UFP_DMA_H */
