#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/aer.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/eventfd.h>
#include <linux/numa.h>

#include "ufp_main.h"
#include "ufp_dma.h"
#include "ufp_fops.h"

static struct ufp_device *ufp_device_alloc(struct pci_dev *pdev);
static void ufp_device_free(struct ufp_device *device);
static irqreturn_t ufp_interrupt(int irqnum, void *data);
static struct ufp_irq *ufp_irq_alloc(struct msix_entry *entry);
static void ufp_irq_free(struct ufp_irq *irq);
static void ufp_free_msix(struct ufp_device *device);
static int ufp_configure_msix(struct ufp_device *device);
static void ufp_interrupt_disable(struct ufp_device *device);
static int ufp_probe(struct pci_dev *pdev,
	const struct pci_device_id *ent);
static void ufp_remove(struct pci_dev *pdev);
static pci_ers_result_t ufp_io_error_detected(struct pci_dev *pdev,
	pci_channel_state_t state);
static pci_ers_result_t ufp_io_slot_reset(struct pci_dev *pdev);
static void ufp_io_resume(struct pci_dev *pdev);
static int __init ufp_module_init(void);
static void __exit ufp_module_exit(void);

const char ufp_driver_name[]	= "ufp";
const char ufp_driver_desc[]	= "Direct access to Intel VF device from UserSpace";
const char ufp_driver_ver[]	= "1.0";
const char *ufp_copyright[]	= {
	"Copyright (c) 1999-2014 Intel Corporation.",
	"Copyright (c) 2009 Qualcomm Inc.",
	"Copyright (c) 2014 by Yukito Ueno <eden@sfc.wide.ad.jp>.",
};

static struct pci_device_id ufp_pci_tbl[] = {
	{ PCI_VDEVICE(INTEL, I40E_DEV_ID_SFP_XL710) },
	{ PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_VF) },
	{ PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X540_VF) },
	{ PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550_VF) },
	{ PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_X_VF) },
	/* required last entry */
	{ .device = 0 }
};

MODULE_DEVICE_TABLE(pci, ufp_pci_tbl);
MODULE_AUTHOR("Yukito Ueno <eden@sfc.wide.ad.jp>");
MODULE_DESCRIPTION("Direct access to Intel VF device from UserSpace");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

static struct pci_error_handlers ufp_err_handler = {
	.error_detected	= ufp_io_error_detected,
	.slot_reset	= ufp_io_slot_reset,
	.resume		= ufp_io_resume,
};

static struct pci_driver ufp_driver = {
	.name		= ufp_driver_name,
	.id_table	= ufp_pci_tbl,
	.probe		= ufp_probe,
	.remove		= ufp_remove,
	.err_handler	= &ufp_err_handler
};

int ufp_device_inuse(struct ufp_device *device)
{
	unsigned ref = atomic_read(&device->refcount);
	if (!ref)
		return 0;
	return 1;
}

void ufp_device_get(struct ufp_device *device)
{
	atomic_inc(&device->refcount);
	return;
}

void ufp_device_put(struct ufp_device *device)
{
	atomic_dec(&device->refcount);
	return;
}

static struct ufp_device *ufp_device_alloc(struct pci_dev *pdev)
{
	struct ufp_device *device;

	device = kzalloc(sizeof(struct ufp_device), GFP_KERNEL);
	if (!device){
		return NULL;
	}

	pci_set_drvdata(pdev, device);
	device->pdev = pdev;
	atomic_set(&device->refcount, 0);
	sema_init(&device->sem, 1);

	device->iobase = pci_resource_start(pdev, 0);
	device->iolen  = pci_resource_len(pdev, 0);

	return device;
}

static void ufp_device_free(struct ufp_device *device)
{
	kfree(device);
	return;
}

static irqreturn_t ufp_interrupt(int irqnum, void *data)
{
	struct ufp_irq *irq = data;

	/* 
	 * We setup EIAM such that interrupts are auto-masked (disabled).
	 * User-space will re-enable them.
	 */

	if(irq->efd_ctx)
		eventfd_signal(irq->efd_ctx, EVENTFD_INCREMENT);

	return IRQ_HANDLED;
}

static struct ufp_irq *ufp_irq_alloc(struct msix_entry *entry)
{
	struct ufp_irq *irq;

	/* XXX: To support NUMA-aware allocation, use kzalloc_node() */
	irq = kzalloc(sizeof(struct ufp_irq), GFP_KERNEL);
	if(!irq){
		return NULL;
	}

	irq->msix_entry = entry;
	irq->efd_ctx = NULL;

	return irq;
}

static void ufp_irq_free(struct ufp_irq *irq)
{
	if(irq->efd_ctx){
		eventfd_ctx_put(irq->efd_ctx);
	}

	kfree(irq);
	return;
}

int ufp_irq_bind(struct ufp_device *device, u32 entry_idx,
	int event_fd, u32 *vector)
{
	struct ufp_irq *irq;
	struct eventfd_ctx *efd_ctx;

	if(entry_idx >= device->num_irqs)
		goto err_invalid_arg;

	irq = device->irqs[entry_idx];

	efd_ctx = eventfd_ctx_fdget(event_fd);
	if(IS_ERR(efd_ctx)){
		goto err_get_ctx;
	}

	if(irq->efd_ctx)
		eventfd_ctx_put(irq->efd_ctx);

	irq->efd_ctx = efd_ctx;
	*vector = irq->msix_entry->vector;

	return 0;

err_get_ctx:
err_invalid_arg:
	return -EINVAL;
}

static void ufp_free_msix(struct ufp_device *device)
{
	int i;

        for(i = 0; i < device->num_irqs; i++){
                free_irq(device->irqs[i]->msix_entry->vector,
                        device->irqs[i]);
                ufp_irq_free(device->irqs[i]);
        }

	kfree(device->irqs);
	pci_disable_msix(device->pdev);
	kfree(device->msix_entries);
	device->msix_entries = NULL;

	return;
}

static int ufp_configure_msix(struct ufp_device *device)
{
	int entry_idx = 0, err, i;
	int num_irq_requested = 0;
	struct msix_entry *entry;

	pr_info("required vector num = %d\n", device->num_irqs);

	device->msix_entries = kcalloc(device->num_irqs,
		sizeof(struct msix_entry), GFP_KERNEL);
	if (!device->msix_entries) {
		goto err_allocate_msix_entries;
	}

	for (entry_idx = 0; entry_idx < device->num_irqs; entry_idx++){
		device->msix_entries[entry_idx].entry = entry_idx;
	}

	err = pci_enable_msix(device->pdev,
		device->msix_entries, device->num_irqs);
	if(err){
		/* failed to allocate enough msix vector */
		goto err_pci_enable_msix;
	}

	device->irqs = kcalloc(device->num_irqs,
		sizeof(struct ufp_irq *), GFP_KERNEL);
	if(!device->irqs)
		goto err_alloc_irq_array;

	for(entry_idx = 0; entry_idx < device->num_irqs; entry_idx++,
	num_irq_requested++){
		entry = &device->msix_entries[entry_idx];

		device->irqs[entry_idx] = ufp_irq_alloc(entry);
		if(!device->irqs[entry_idx]){
			goto err_alloc_irq;
		}

		err = request_irq(entry->vector, &ufp_interrupt, 0,
			pci_name(device->pdev), device->irqs[entry_idx]);
		if(err){
			goto err_request_irq;
		}

		pr_info("IRQ registered: entry_idx = %d, vector = %d\n",
			entry_idx, entry->vector);
		continue;

err_request_irq:
		kfree(device->irqs[entry_idx]);
		goto err_alloc_irq;
	}

	return 0;

err_alloc_irq:
	for(i = 0; i < num_irq_requested; i++){
		free_irq(device->irqs[i]->msix_entry->vector,
			device->irqs[i]);
		kfree(device->irqs[i]);
	}
	kfree(device->irqs);
err_alloc_irq_array:
	pci_disable_msix(device->pdev);
err_pci_enable_msix:
	kfree(device->msix_entries);
	device->msix_entries = NULL;
err_allocate_msix_entries:
	return -1;
}

static void ufp_interrupt_disable(struct ufp_device *device)
{
	int vector;

	for(vector = 0; vector < device->num_irqs; vector++)
		synchronize_irq(device->msix_entries[vector].vector);

	return;
}

int ufp_start(struct ufp_device *device, u32 num_irqs)
{
	int err;

	if(device->started){
		goto err_already;
	}

	if(!num_irqs){
		goto err_num_queues;
	}

	device->num_irqs = num_irqs;

	err = ufp_configure_msix(device);
	if(err < 0){
		pr_err("failed to allocate MSI-X\n");
		goto err_configure_msix;
	}

	device->started = 1;
	return 0;

err_configure_msix:
err_num_queues:
err_already:
	return -1;
}

void ufp_stop(struct ufp_device *device)
{
	if(!device->started){
		goto err_already;
	}

	ufp_interrupt_disable(device);

	if (!pci_channel_offline(device->pdev)){
		pr_err("pci channel state is abnormal");
	}

	/* free irqs */
	ufp_free_msix(device);
	device->started = 0;

err_already:
	return;
}

static int ufp_probe(struct pci_dev *pdev,
	const struct pci_device_id *ent)
{
	struct ufp_device *device;
	int err;

	pr_info("probing device %s\n", pci_name(pdev));

	err = pci_enable_device(pdev);
	if (err)
		goto err_enable_device;

	err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (err)
		err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (err) {
		pr_err("DMA configuration failed: 0x%x\n", err);
		goto err_dma;
	}

	err = pci_request_regions(pdev, ufp_driver_name);
	if (err) {
		pr_err("pci_request_regions failed 0x%x\n", err);
		goto err_pci_reg;
	}

	pci_enable_pcie_error_reporting(pdev);
	pci_set_master(pdev);

	device = ufp_device_alloc(pdev);
	if (!device) {
		err = -ENOMEM;
		goto err_alloc;
	}

	/*
	 * call save state here in standalone driver because it relies on
	 * device struct to exist.
	 */
	pci_save_state(pdev);

	/* setup for userland pci register access */
	INIT_LIST_HEAD(&device->areas);
	device->hw_addr = ufp_dma_map_iobase(device);
	if (!device->hw_addr) {
		err = -EIO;
		goto err_ioremap;
	}

	pr_info("device %s initialized\n", pci_name(pdev));

	err = ufp_miscdev_register(device);
	if(err < 0)
		goto err_miscdev_register;

	return 0;

err_miscdev_register:
	ufp_dma_unmap_all(device);
err_ioremap:
	ufp_device_free(device);
err_alloc:
	pci_disable_pcie_error_reporting(pdev);
	pci_release_regions(pdev);
err_pci_reg:
err_dma:
	pci_disable_device(pdev);
err_enable_device:
	return err;
}

static void ufp_remove(struct pci_dev *pdev)
{
	struct ufp_device *device = pci_get_drvdata(pdev);

	ufp_stop(device);

	ufp_miscdev_deregister(device);
	ufp_dma_unmap_all(device);

	pr_info("device %s removed\n", pci_name(pdev));
	ufp_device_free(device);

	pci_disable_pcie_error_reporting(pdev);
	pci_release_regions(pdev);
	pci_disable_device(pdev);

	return;
}

static pci_ers_result_t ufp_io_error_detected(struct pci_dev *pdev,
	pci_channel_state_t state)
{
	// FIXME: Do something
	pr_info("ufp_io_error_detected: PCI error is detected");
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t ufp_io_slot_reset(struct pci_dev *pdev)
{
	// FIXME: Do something
	pr_info("ufp_io_slot_reset: the pci bus has been reset");
	return PCI_ERS_RESULT_RECOVERED;
}

static void ufp_io_resume(struct pci_dev *pdev)
{
	// FIXME: Do something
	pr_info("ufp_io_resume: traffic can start flowing again");
	return;
}

static int __init ufp_module_init(void)
{
	int err;

	pr_info("%s - version %s\n",
		ufp_driver_desc, ufp_driver_ver);
	pr_info("%s\n", ufp_copyright[0]);
	pr_info("%s\n", ufp_copyright[1]);
	pr_info("%s\n", ufp_copyright[2]);

	err = pci_register_driver(&ufp_driver);
	return err;
}

static void __exit ufp_module_exit(void)
{
	pci_unregister_driver(&ufp_driver);
	return;
}

module_init(ufp_module_init);
module_exit(ufp_module_exit);

