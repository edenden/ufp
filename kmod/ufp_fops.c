#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/poll.h>
#include <linux/sysctl.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/if_ether.h>

#include "ufp_main.h"
#include "ufp_dma.h"
#include "ufp_fops.h"

static int ufp_cmd_start(struct ufp_device *device, void __user *argp);
static int ufp_cmd_stop(struct ufp_device *device, unsigned long arg);
static int ufp_cmd_info(struct ufp_device *device, void __user *argp);
static int ufp_cmd_map(struct ufp_device *device, void __user *argp);
static int ufp_cmd_unmap(struct ufp_device *device, void __user *argp);
static int ufp_cmd_irqbind(struct ufp_device *device, void __user *argp);
static ssize_t ufp_read(struct file *file, char __user *buf,
	size_t count, loff_t *pos);
static ssize_t ufp_write(struct file *file, const char __user *buf,
	size_t count, loff_t *pos);
static int ufp_open(struct inode *inode, struct file * file);
static int ufp_close(struct inode *inode, struct file *file);
static void ufp_vm_open(struct vm_area_struct *vma);
static void ufp_vm_close(struct vm_area_struct *vma);
static int ufp_vm_fault(struct vm_area_struct *area, struct vm_fault *fdata);
static int ufp_mmap(struct file *file, struct vm_area_struct *vma);
static long ufp_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static struct file_operations ufp_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= ufp_read,
	.write		= ufp_write,
	.open		= ufp_open,
	.release	= ufp_close,
	.mmap		= ufp_mmap,
	.unlocked_ioctl	= ufp_ioctl,
};

static struct vm_operations_struct ufp_mmap_ops = {
	.open		= ufp_vm_open,
	.close		= ufp_vm_close,
	.fault		= ufp_vm_fault
};

int ufp_miscdev_register(struct ufp_device *device)
{
	struct pci_dev *pdev = device->pdev;
	char *miscdev_name;
	int err;

	miscdev_name = kmalloc(MISCDEV_NAME_SIZE, GFP_KERNEL);
	if(!miscdev_name){
		goto err_alloc_name;
	}

	snprintf(miscdev_name, MISCDEV_NAME_SIZE, "ufp!p%ds%d",
		PCI_FUNC(pdev->devfn), PCI_SLOT(pdev->devfn));

	device->miscdev.minor = MISC_DYNAMIC_MINOR;
	device->miscdev.name = miscdev_name;
	device->miscdev.fops = &ufp_fops;
	err = misc_register(&device->miscdev);
	if (err) {
		pr_err("failed to register misc device\n");
		goto err_misc_register;
	}

	pr_info("misc device registered as %s\n", device->miscdev.name);
	return 0;

err_misc_register:
	kfree(device->miscdev.name);
err_alloc_name:
	return -1;
}

void ufp_miscdev_deregister(struct ufp_device *device)
{
	misc_deregister(&device->miscdev);

	pr_info("misc device %s unregistered\n", device->miscdev.name);
	kfree(device->miscdev.name);

	return;
}

static int ufp_cmd_start(struct ufp_device *device, void __user *argp)
{
	struct ufp_start_req req;
	int err = 0;

	err = copy_from_user(&req, argp, sizeof(req));
	if(err){
		err = -EFAULT;
		goto err_copy_from_user;
	}

	err = ufp_start(device, req.num_irqs);
	if (err){
		err = -EINVAL;
		goto err_start;
	}

	return 0;

err_start:
err_copy_from_user:
	return err;
}

static int ufp_cmd_stop(struct ufp_device *device, unsigned long arg)
{
	ufp_stop(device);
	return 0;
}

static int ufp_cmd_info(struct ufp_device *device, void __user *argp)
{
	struct ufp_info_req req;
	struct pci_dev *pdev = device->pdev;
	int err = 0;

	req.mmio_base = device->iobase;
	req.mmio_size = device->iolen;

	req.device_id = pdev->device;
	req.vendor_id = pdev->vendor;

	if (copy_to_user(argp, &req, sizeof(req))) {
		err = -EFAULT;
		goto out;
	}

out:
	return err;
}

static int ufp_cmd_map(struct ufp_device *device, void __user *argp)
{
	struct ufp_map_req req;
	unsigned long addr_dma;

	if (copy_from_user(&req, argp, sizeof(req)))
		return -EFAULT;

	if (!req.size)
		return -EINVAL;

	addr_dma = ufp_dma_map(device, req.addr_virt,
			req.size, req.cache);
	if(!addr_dma)
		return -EFAULT;

	req.addr_dma = addr_dma;

	if (copy_to_user(argp, &req, sizeof(req))) {
		ufp_dma_unmap(device, req.addr_dma);
		return -EFAULT;
	}

	return 0;
}

static int ufp_cmd_unmap(struct ufp_device *device, void __user *argp)
{
	struct ufp_unmap_req req;
	int ret;

	if (copy_from_user(&req, argp, sizeof(req)))
		return -EFAULT;

	ret = ufp_dma_unmap(device, req.addr_dma);
	if(ret != 0)
		return ret;

	return 0;
}

static int ufp_cmd_irqbind(struct ufp_device *device, void __user *argp)
{
	struct ufp_irqbind_req req;
	int ret;

	if (copy_from_user(&req, argp, sizeof(req)))
		return -EFAULT;

	ret = ufp_irq_bind(device, req.entry_idx, req.event_fd,
		&req.vector);
	if(ret != 0)
		return ret;

	if (copy_to_user(argp, &req, sizeof(req))) {
		return -EFAULT;
	}

	return 0;
}

static ssize_t ufp_read(struct file * file, char __user * buf,
	size_t count, loff_t *pos)
{
	return 0;
}

static ssize_t ufp_write(struct file * file, const char __user * buf,
			     size_t count, loff_t *pos)
{
	return 0;
}

static int ufp_open(struct inode *inode, struct file * file)
{
	struct ufp_device *device;
	struct miscdevice *miscdev = file->private_data;
	int err;

	device = container_of(miscdev, struct ufp_device, miscdev);
	pr_info("open req miscdev=%p\n", miscdev);

	down(&device->sem);

	// Only one process is alowed to open
	if (ufp_device_inuse(device)) {
		err = -EBUSY;
		goto out;
	}

	ufp_device_get(device);
	file->private_data = device;
	err = 0;

out:
	up(&device->sem);
	return err;
}

static int ufp_close(struct inode *inode, struct file *file)
{
	struct ufp_device *device = file->private_data;
	if (!device)
		return 0;

	pr_info("close device=%p\n", device);

	down(&device->sem);
	ufp_stop(device);
	up(&device->sem);

	ufp_device_put(device);
	return 0;
}

static void ufp_vm_open(struct vm_area_struct *vma)
{
}

static void ufp_vm_close(struct vm_area_struct *vma)
{
}

static int ufp_vm_fault(struct vm_area_struct *area, struct vm_fault *fdata)
{
	return VM_FAULT_SIGBUS;
}

static int ufp_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ufp_device *device = file->private_data;
	struct ufp_dma_area *area;

	unsigned long start = vma->vm_start;
	unsigned long size  = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pfn;

	if (!device)
		return -ENODEV;

	pr_info("mmap device %p start %lu size %lu\n", device, start, size);

	/* Currently no area used except offset=0 for pci registers */
	if(offset != 0)
		return -EINVAL;

	area = ufp_dma_area_lookup(device, device->iobase);
	if (!area)
		return -ENOENT;

	// We do not do partial mappings, sorry
	if (area->size != size)
		return -EOVERFLOW;

	pfn = area->addr_dma >> PAGE_SHIFT;

	switch (area->cache) {
	case IXGBE_DMA_CACHE_DISABLE:
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		break;

	case IXGBE_DMA_CACHE_WRITECOMBINE:
		#ifdef pgprot_writecombine
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
		#endif
		break;

	default:
		/* Leave as is */
		break;
	}

	if (remap_pfn_range(vma, start, pfn, size, vma->vm_page_prot))
		return -EAGAIN;

	vma->vm_ops = &ufp_mmap_ops;
	return 0;
}

static long ufp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct ufp_device *device = file->private_data;
	void __user * argp = (void __user *) arg;
	int err;

	if(!device)
		return -EBADFD;

	down(&device->sem);

	switch (cmd) {
	case UFP_INFO:
		err = ufp_cmd_info(device, argp);
		break;

	case UFP_START:
		err = ufp_cmd_start(device, argp);
		break;

	case UFP_MAP:
		err = ufp_cmd_map(device, argp);
		break;

	case UFP_UNMAP:
		err = ufp_cmd_unmap(device, argp);
		break;

	case UFP_STOP:
		err = ufp_cmd_stop(device, arg);
		break;

	case UFP_IRQBIND:
		err = ufp_cmd_irqbind(device, argp);
		break;

	default:
		err = -EINVAL;
		break;
	};

	up(&device->sem);

	return err;
}

