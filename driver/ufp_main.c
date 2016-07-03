#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/aer.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/eventfd.h>

#include "ufp_main.h"
#include "ufp_dma.h"
#include "ufp_fops.h"

static int ufp_alloc_enid(void);
static struct ufp_port *ufp_port_alloc(void);
static void ufp_port_free(struct ufp_port *port);
static irqreturn_t ufp_interrupt(int irqnum, void *data);
static struct ufp_irq *ufp_irq_alloc(struct msix_entry *entry);
static void ufp_irq_free(struct ufp_irq *irq);
static void ufp_free_msix(struct ufp_port *port);
static int ufp_configure_msix(struct ufp_port *port);
static inline void ufp_interrupt_disable(struct ufp_port *port);
static int ufp_hw_init(struct ufp_port *port);
static void ufp_hw_free(struct ufp_port *port);
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

static LIST_HEAD(dev_list);
static DEFINE_SEMAPHORE(dev_sem);

int ufp_port_inuse(struct ufp_port *port)
{
	unsigned ref = atomic_read(&port->refcount);
	if (!ref)
		return 0;
	return 1;
}

void ufp_port_get(struct ufp_port *port)
{
	atomic_inc(&port->refcount);
	return;
}

void ufp_port_put(struct ufp_port *port)
{
	atomic_dec(&port->refcount);
	return;
}

static int ufp_alloc_enid(void)
{
	struct ufp_port *port;
	unsigned int id = 0;

	list_for_each_entry(port, &dev_list, list) {
		id++;
	}

	return id;
}

static struct ufp_port *ufp_port_alloc(void)
{
	struct ufp_port *port;

	port = kzalloc(sizeof(struct ufp_port), GFP_KERNEL);
	if (!port){
		return NULL;
	}

	port->hw = kzalloc(sizeof(struct ufp_hw), GFP_KERNEL);
	if(!port->hw){
		return NULL;
	}

	atomic_set(&port->refcount, 0);
	sema_init(&port->sem, 1);

	/* Add to global list */
	down(&dev_sem);
	port->id = ufp_alloc_enid();
	list_add(&port->list, &dev_list);
	up(&dev_sem);

	return port;
}

static void ufp_port_free(struct ufp_port *port)
{
	down(&dev_sem);
	list_del(&port->list);
	up(&dev_sem);

	kfree(port->hw);
	kfree(port);
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

int ufp_irq_assign(struct ufp_port *port, enum ufp_irq_type type,
	uint32_t queue_idx, int event_fd, uint32_t *vector, uint16_t *entry)
{
	struct ufp_irq *irq;
	struct eventfd_ctx *efd_ctx;

	switch(type){
	case IXMAP_IRQ_RX:
		if(queue_idx > port->num_rx_queues)
			goto err_invalid_arg;

		irq = port->rx_irq[queue_idx];
		break;
	case IXMAP_IRQ_TX:
		if(queue_idx > port->num_tx_queues)
			goto err_invalid_arg;

		irq = port->tx_irq[queue_idx];
		break;
	default:
		goto err_invalid_arg;
	}

	efd_ctx = eventfd_ctx_fdget(event_fd);
	if(IS_ERR(efd_ctx)){
		goto err_get_ctx;
	}

	if(irq->efd_ctx)
		eventfd_ctx_put(irq->efd_ctx);

	irq->efd_ctx = efd_ctx;
	*vector = irq->msix_entry->vector;
	*entry = irq->msix_entry->entry;

	return 0;

err_get_ctx:
err_invalid_arg:
	return -EINVAL;
}

static void ufp_free_msix(struct ufp_port *port)
{
	int i;

        for(i = 0; i < port->num_rx_queues; i++){
                free_irq(port->rx_irq[i]->msix_entry->vector,
                        port->rx_irq[i]);
                ufp_irq_free(port->rx_irq[i]);
        }

        for(i = 0; i < port->num_tx_queues; i++){
                free_irq(port->tx_irq[i]->msix_entry->vector,
                        port->tx_irq[i]);
                ufp_irq_free(port->tx_irq[i]);
        }

        kfree(port->tx_irq);
        kfree(port->rx_irq);
        pci_disable_msix(port->pdev);
        kfree(port->msix_entries);
        port->msix_entries = NULL;
        port->num_q_vectors = 0;

        return;
}

static int ufp_configure_msix(struct ufp_port *port)
{
	int vector = 0, vector_num, queue_idx, err, i;
	int num_rx_requested = 0, num_tx_requested = 0;
	struct msix_entry *entry;

	vector_num = port->num_rx_queues + port->num_tx_queues;
	pr_info("required vector num = %d\n", vector_num);

	port->msix_entries = kcalloc(vector_num,
		sizeof(struct msix_entry), GFP_KERNEL);
	if (!port->msix_entries) {
		goto err_allocate_msix_entries;
	}

	for (vector = 0; vector < vector_num; vector++){
		port->msix_entries[vector].entry = vector;
	}

	err = vector_num;
	while (err){
		/* err == number of vectors we should try again with */ 
	      	err = pci_enable_msix(port->pdev, port->msix_entries, err);

		if(err < 0){
		       	/* failed to allocate enough msix vector */
			goto err_pci_enable_msix;
	       	}
	}
	port->num_q_vectors = vector_num;

	port->rx_irq = kcalloc(port->num_rx_queues,
		sizeof(struct ufp_irq *), GFP_KERNEL);
	if(!port->rx_irq)
		goto err_alloc_irq_array_rx;

	port->tx_irq = kcalloc(port->num_tx_queues,
		sizeof(struct ufp_irq *), GFP_KERNEL);
	if(!port->tx_irq)
		goto err_alloc_irq_array_tx;

	for(queue_idx = 0, vector = 0; queue_idx < port->num_rx_queues;
	queue_idx++, vector++, num_rx_requested++){
		entry = &port->msix_entries[vector];

		port->rx_irq[queue_idx] = ufp_irq_alloc(entry);
		if(!port->rx_irq[queue_idx]){
			goto err_alloc_irq_rx;
		}

		err = request_irq(entry->vector, &ufp_interrupt, 0,
				pci_name(port->pdev), port->rx_irq[queue_idx]);
		if(err){
			goto err_request_irq_rx;
		}

		pr_info("RX irq registered: index = %d, vector = %d\n",
			queue_idx, entry->vector);

		continue;

err_request_irq_rx:
		kfree(port->rx_irq[queue_idx]);
		goto err_alloc_irq_rx;
	}

	for(queue_idx = 0; queue_idx < port->num_tx_queues;
	queue_idx++, vector++, num_tx_requested++){
		entry = &port->msix_entries[vector];

		port->tx_irq[queue_idx] = ufp_irq_alloc(entry);
		if(!port->tx_irq[queue_idx]){
			goto err_alloc_irq_tx;
		}

		err = request_irq(entry->vector, &ufp_interrupt, 0,
			pci_name(port->pdev), port->tx_irq[queue_idx]);
		if(err){
			goto err_request_irq_tx;
		}

		pr_info("TX irq registered: index = %d, vector = %d\n",
			queue_idx, entry->vector);

		continue;

err_request_irq_tx:
		kfree(port->tx_irq[queue_idx]);
		goto err_alloc_irq_tx;
	}

	return 0;

err_alloc_irq_tx:
	for(i = 0; i < num_tx_requested; i++){
		free_irq(port->tx_irq[i]->msix_entry->vector,
			port->tx_irq[i]);
		kfree(port->tx_irq[i]);
	}
err_alloc_irq_rx:
	for(i = 0; i < num_rx_requested; i++){
		free_irq(port->rx_irq[i]->msix_entry->vector,
			port->rx_irq[i]);
		kfree(port->rx_irq[i]);
	}
	kfree(port->tx_irq);
err_alloc_irq_array_tx:
	kfree(port->rx_irq);
err_alloc_irq_array_rx:
err_pci_enable_msix:
	pci_disable_msix(port->pdev);
	kfree(port->msix_entries);
	port->msix_entries = NULL;
err_allocate_msix_entries:
	return -1;
}

static inline void ufp_interrupt_disable(struct ufp_port *port)
{
	int vector;

	for (vector = 0; vector < port->num_q_vectors; vector++)
		synchronize_irq(port->msix_entries[vector].vector);
}

int ufp_up(struct ufp_port *port)
{
	int err;

	if(!port->num_rx_queues || !port->num_tx_queues){
		goto err_num_queues;
	}

	err = ufp_configure_msix(port);
        if(err < 0){
		pr_err("failed to allocate MSI-X\n");
		goto err_configure_msix;
	}

	/* User space application enables interrupts after */
	port->up = 1;

	return 0;

err_configure_msix:
err_num_queues:
	return -1;
}

int ufp_down(struct ufp_port *port)
{
        ufp_interrupt_disable(port);

	if (!pci_channel_offline(port->pdev))
		pr_info("pci channel state is abnormal");

        /* free irqs */
        ufp_free_msix(port);
        port->up = 0;

        return 0;
}

static int ufp_hw_init(struct ufp_port *port)
{
	struct ufp_hw *hw = port->hw;
	struct pci_dev *pdev = port->pdev;

	/* PCI config space info */
	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;
	pci_read_config_byte(pdev, PCI_REVISION_ID, &hw->revision_id);
	hw->subsystem_vendor_id = pdev->subsystem_vendor;
	hw->subsystem_device_id = pdev->subsystem_device;

	return 0;
}

static void ufp_hw_free(struct ufp_port *port)
{
	return;
}

static int ufp_probe(struct pci_dev *pdev,
	const struct pci_device_id *ent)
{
	struct ufp_port *port;
	struct ufp_hw	*hw;
	int pci_using_dac, err;

	pr_info("probing device %s\n", pci_name(pdev));

	err = pci_enable_device(pdev);
	if (err)
		goto err_enable_device;

	/*
	 * first argument of dma_set_mask(), &pdev->dev should be changed
	 * on Linux kernel version.
	 * Original ixgbe driver hides this issue with pci_dev_to_dev() macro.
	 */
	if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(64)) &&
	    !dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64))) {
		pci_using_dac = 1;
	} else {
		err = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
		if (err) {
			err = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
			if (err) {
				pr_err("No usable DMA configuration, aborting\n");
				goto err_dma;
			}
		}
		pci_using_dac = 0;
	}

        err = pci_request_regions(pdev, ufp_driver_name);
        if (err) {
                pr_err("pci_request_regions failed 0x%x\n", err);
                goto err_pci_reg;
        }

	pci_enable_pcie_error_reporting(pdev);
	pci_set_master(pdev);

        port = ufp_port_alloc();
        if (!port) {
                err = -ENOMEM;
                goto err_alloc;
        }
        hw = port->hw;

        pci_set_drvdata(pdev, port);
        port->pdev = pdev;

	/* ufp_hw INITIALIZATION */
	err = ufp_hw_init(port);
	if (err)
		goto err_hw_init;

	/*
	 * call save state here in standalone driver because it relies on
	 * port struct to exist.
	 */
        pci_save_state(pdev);

        port->iobase = pci_resource_start(pdev, 0);
        port->iolen  = pci_resource_len(pdev, 0);

        if(pci_using_dac){
                port->dma_mask = DMA_BIT_MASK(64);
        }else{
                port->dma_mask = DMA_BIT_MASK(32);
        }

        /* setup for userland pci register access */
        INIT_LIST_HEAD(&port->areas);
        hw->hw_addr = ufp_dma_map_iobase(port);
        if (!hw->hw_addr) {
                err = -EIO;
                goto err_ioremap;
        }

	pr_info("device[%u] %s initialized\n", port->id, pci_name(pdev));

	err = ufp_miscdev_register(port);
	if(err < 0)
		goto err_miscdev_register;

	return 0;

err_miscdev_register:
	ufp_dma_unmap_all(port);
err_ioremap:
	ufp_hw_free(port);
err_hw_init:
        ufp_port_free(port);
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
        struct ufp_port *port = pci_get_drvdata(pdev);

        if(port->up){
                ufp_down(port);
        }

        ufp_miscdev_deregister(port);
        ufp_dma_unmap_all(port);

	pr_info("device[%u] %s removed\n", port->id, pci_name(pdev));
	ufp_hw_free(port);
	ufp_port_free(port);

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
}

module_init(ufp_module_init);
module_exit(ufp_module_exit);

