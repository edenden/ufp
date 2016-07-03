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
#include "ufp_mac.h"

static int ufp_cmd_up(struct ufp_port *port, void __user *argp);
static int ufp_cmd_down(struct ufp_port *port, unsigned long arg);
static int ufp_cmd_reset(struct ufp_port *port, unsigned long arg);
static int ufp_cmd_info(struct ufp_port *port, void __user *argp);
static int ufp_cmd_map(struct ufp_port *port, void __user *argp);
static int ufp_cmd_unmap(struct ufp_port *port, void __user *argp);
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

int ufp_miscdev_register(struct ufp_port *port)
{
	char *miscdev_name;
	int err;

	miscdev_name = kmalloc(MISCDEV_NAME_SIZE, GFP_KERNEL);
	if(!miscdev_name){
		goto err_alloc_name;
	}
	snprintf(miscdev_name, MISCDEV_NAME_SIZE, "ixgbe%d", port->id);

	port->miscdev.minor = MISC_DYNAMIC_MINOR;
	port->miscdev.name = miscdev_name;
	port->miscdev.fops = &ufp_fops;
	err = misc_register(&port->miscdev);
	if (err) {
		pr_err("failed to register misc device\n");
		goto err_misc_register;
	}

	pr_info("misc device registered as %s\n", port->miscdev.name);
	return 0;

err_misc_register:
        kfree(port->miscdev.name);
err_alloc_name:
	return -1;
}

void ufp_miscdev_deregister(struct ufp_port *port)
{
	misc_deregister(&port->miscdev);

	pr_info("misc device %s unregistered\n", port->miscdev.name);
	kfree(port->miscdev.name);

	return;
}

static int ufp_cmd_up(struct ufp_port *port, void __user *argp)
{
	struct ufp_up_req req;
	int err = 0;

	if(port->up){
		return -EALREADY;
	}

	if (copy_from_user(&req, argp, sizeof(req)))
		return -EFAULT;

	pr_info("open req\n");

	port->num_interrupt_rate = req.num_interrupt_rate;
	port->num_rx_queues = req.num_rx_queues;
	port->num_tx_queues = req.num_tx_queues;

	err = ufp_up(port);
	if (err){
		err = -EINVAL;
		goto err_up_complete;
	}

	return 0;

err_up_complete:
	return err;
}

static int ufp_cmd_down(struct ufp_port *port,
	unsigned long arg)
{
	if(!port->up){
		return -EALREADY;
	}

	pr_info("down req\n");
	ufp_down(port);

	return 0;
}

static int ufp_cmd_reset(struct ufp_port *port,
	unsigned long arg)
{
	if(!port->up){
		return 0;
	}

	pr_info("reset req\n");
	ufp_reset(port);

	return 0;
}

static int ufp_cmd_info(struct ufp_port *port,
	void __user *argp)
{
	struct ufp_info_req req;
	struct ufp_hw *hw = port->hw;
	struct ufp_mac_info *mac = hw->mac;
	int err = 0;

	req.mmio_base = port->iobase;
	req.mmio_size = port->iolen;

	memcpy(req.mac_addr, mac->perm_addr, ETH_ALEN);

	req.num_interrupt_rate = port->num_interrupt_rate;
	req.max_interrupt_rate = IXGBE_MAX_EITR;

	req.num_rx_queues = port->num_rx_queues;
	req.num_tx_queues = port->num_tx_queues;

	/* Currently we support only RX/TX RSS mode */
	req.max_rx_queues = mac->max_rx_queues;
	req.max_tx_queues = mac->max_tx_queues;

	if (copy_to_user(argp, &req, sizeof(req))) {
		err = -EFAULT;
		goto out;
	}

out:
	return err;
}

static int ufp_cmd_map(struct ufp_port *port,
	void __user *argp)
{
	struct ufp_map_req req;
	unsigned long addr_dma;

	if (copy_from_user(&req, argp, sizeof(req)))
		return -EFAULT;

	if (!req.size)
		return -EINVAL;

	addr_dma = ufp_dma_map(port, req.addr_virtual,
			req.size, req.cache);
	if(!addr_dma)
		return -EFAULT;

	req.addr_dma = addr_dma;

	if (copy_to_user(argp, &req, sizeof(req))) {
		ufp_dma_unmap(port, req.addr_dma);
		return -EFAULT;
	}

	return 0;
}

static int ufp_cmd_unmap(struct ufp_port *port,
	void __user *argp)
{
	struct ufp_unmap_req req;
	int ret;

	if (copy_from_user(&req, argp, sizeof(req)))
		return -EFAULT;

	ret = ufp_dma_unmap(port, req.addr_dma);
	if(ret != 0)
		return ret;

	return 0;
}

static int ufp_cmd_irq(struct ufp_port *port,
	void __user *argp)
{
	struct ufp_irq_req req;
	int ret;

	if (copy_from_user(&req, argp, sizeof(req)))
		return -EFAULT;

	ret = ufp_irq_assign(port, req.type, req.queue_idx,
		req.event_fd, &req.vector, &req.entry);
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
	struct ufp_port *port;
	struct miscdevice *miscdev = file->private_data;
	int err;

	port = container_of(miscdev, struct ufp_port, miscdev);
	pr_info("open req miscdev=%p\n", miscdev);

	down(&port->sem);

	// Only one process is alowed to open
	if (ufp_port_inuse(port)) {
		err = -EBUSY;
		goto out;
	}

	ufp_port_get(port);
	file->private_data = port;
	err = 0;

out:
	up(&port->sem);
	return err;
}

static int ufp_close(struct inode *inode, struct file *file)
{
	struct ufp_port *port = file->private_data;
	if (!port)
		return 0;

	pr_info("close port=%p\n", port);

	down(&port->sem);
	ufp_cmd_down(port, 0);
	up(&port->sem);

	ufp_port_put(port);
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
	struct ufp_port *port = file->private_data;
	struct ufp_dma_area *area;

	unsigned long start = vma->vm_start;
	unsigned long size  = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pfn;

	if (!port)
		return -ENODEV;

	pr_info("mmap port %p start %lu size %lu\n", port, start, size);

	/* Currently no area used except offset=0 for pci registers */
	if(offset != 0)
		return -EINVAL;

	area = ufp_dma_area_lookup(port, port->iobase);
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
	struct ufp_port *port = file->private_data;
	void __user * argp = (void __user *) arg;
	int err;

	if(!port)
		return -EBADFD;

	down(&port->sem);

	switch (cmd) {
	case IXMAP_INFO:
		err = ufp_cmd_info(port, argp);
		break;

	case IXMAP_UP:
		err = ufp_cmd_up(port, argp);
		break;

	case IXMAP_MAP:
		err = ufp_cmd_map(port, argp);
		break;

	case IXMAP_UNMAP:
		err = ufp_cmd_unmap(port, argp);
		break;

	case IXMAP_DOWN:
		err = ufp_cmd_down(port, arg);
		break;

	case IXMAP_RESET:
		err = ufp_cmd_reset(port, arg);
		break;

	case IXMAP_IRQ:
		err = ufp_cmd_irq(port, argp);
		break;

	default:
		err = -EINVAL;
		break;
	};

	up(&port->sem);

	return err;
}

