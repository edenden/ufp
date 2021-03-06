#ifndef _LIBUFP_DEV_H
#define _LIBUFP_DEV_H

struct dylib {
	void			*handle;
	struct list_node	list;
};

struct pci_id {
	uint32_t		vendor_id;
	uint32_t		device_id;
};

struct pci_driver {
	const struct pci_id	*id_table;
	int			(*init)(struct ufp_dev *, struct ufp_ops *);
	void			(*destroy)(struct ufp_dev *, struct ufp_ops *);
};

struct pci_entry {
	uint32_t		vendor_id;
	uint32_t		device_id;
	int			(*init)(struct ufp_dev *, struct ufp_ops *);
	void			(*destroy)(struct ufp_dev *, struct ufp_ops *);
	struct list_node	list;
};

struct ufp_dev_buf {
	void			*addr_virt;
	unsigned long		addr_dma;
	uint64_t		size;
};


struct ufp_dev_buf *ufp_dev_dma_alloc(size_t size);
void ufp_dev_dma_free(struct ufp_dev_buf *buf);
int ufp_dev_init(struct ufp_dev *dev, struct ufp_ops *ops);
int ufp_dev_destroy(struct ufp_dev *dev, struct ufp_ops *ops);
int ufp_dev_register(struct pci_driver *driver);
void ufp_dev_unregister(struct pci_driver *driver);
int ufp_dev_load_lib(char *dir_path);
void ufp_dev_unload_lib();

#endif /* _LIBUFP_DEV_H */
