/*  led-module.c - Example device driver for the LED device

* Copyright (C) 2020 Pavel Benacek
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.

*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License along
*   with this program. If not, see <http://www.gnu.org/licenses/>.

*/


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/capability.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

/* Declare IOCTL handlers */
#define SW_IOCTL_MAGIC			'l'
#define SW_IOCTL_GET_MASK		_IOR(SW_IOCTL_MAGIC, 0, int)
#define SW_IOCTL_SET_MASK		_IOW(SW_IOCTL_MAGIC, 1, int)
#define SW_IOCTL_GET_VALUE		_IOW(SW_IOCTL_MAGIC, 2, int)

/* Configuration related to driver names, etc */
#define DRIVER_NAME "switch_module"
#define DRIVER_SYSFS_CLASS "switch_module"
#define DEVICE_ID_STR "switch_module-%d"

/* Local driver buffer size */
#define BUFF_SIZE 32

/* Initial values */
#define SWITCH_INIT_MASK 0xf

/**
 * @brief Local device structure which is accessible in all
 * callback structures.
 * 
 */
struct switch_module_local {
	/* IO space */
	unsigned long mem_start;
	unsigned long mem_end;
	void __iomem *base_addr;

	/* Device info */
	dev_t devid;
	struct device *device;
	struct class *sysclass;
	struct cdev cdev;

	/* Local device data */
	struct semaphore sem;
	u8 mask; /* Mask applied to switch values */
	char loc_buff[BUFF_SIZE];
};

/**
 * @brief Read data from the device \p addr and apply the mask
 * 
 */
static u8 read_device(const struct switch_module_local *m) {
	u8 rd = ioread8(m->base_addr);
	return rd & m->mask;
}

/* ==================================================================
 		Character device callbacks
   ================================================================== */

#ifdef DEBUG
	#define IOCTL_DEBUG_PRINT(dev,args...) {dev_info(dev,args);}
#else
	#define IOCTL_DEBUG_PRINT(dev,args...) {}
#endif

static long switch_module_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	long rc;
	struct switch_module_local *lp;
	u8 tmp_val;

	/* Setup initial values and acquire the lock */
	rc = 0;
	lp = file->private_data;
	if (down_interruptible(&lp->sem)) {
		dev_err(lp->device, "Cannot acquire the device, it is being used by a different process.\n");
		return -ERESTARTSYS;
	}

	IOCTL_DEBUG_PRINT(lp->device, "IOCTL Handler has been called - cmd = 0x%x , arg = 0x%lx\n", cmd, arg);
	switch (cmd) {
		case SW_IOCTL_GET_MASK:
			rc = put_user(lp->mask, (int __user*) arg);
			IOCTL_DEBUG_PRINT(lp->device, "Sending the mask value 0x%x (rc = %ld)\n", lp->mask, rc);
			break;
		case SW_IOCTL_SET_MASK:
			if (!capable(CAP_SYS_ADMIN)) {
				IOCTL_DEBUG_PRINT(lp->device,"User is not capable to set the mask value\n");
				return -EPERM;
			}
			lp->mask = arg;
			IOCTL_DEBUG_PRINT(lp->device, "Sending the mask value 0x%x (rc = %ld)\n", lp->mask, rc);
			break;
		case SW_IOCTL_GET_VALUE:
			tmp_val = read_device(lp);
			rc = put_user(tmp_val, (int __user*) arg);
			IOCTL_DEBUG_PRINT(lp->device, "Sending the current value 0x%x (rc = %ld)\n", tmp_val, rc);
			break;
		default:
			dev_info(lp->device, "Invalid ioctl cmd = 0x%08x\n", cmd);
			rc = -ENOTTY;
			break;
	}

	up(&lp->sem);
	return rc;
}

static loff_t switch_module_cdev_llseek(struct file *file, loff_t offset, int whence) {
	struct switch_module_local *lp;
	loff_t rc;

	lp = file->private_data;
	if (down_interruptible(&lp->sem)) {
		dev_err(lp->device, "Cannot acquire the device, it is used by a different process.\n");
		return -ERESTARTSYS;
	}

	/* Restart the status and seek the offset based on whence */
	switch (whence) {
		case SEEK_SET: /* Set from the beginning */
			rc = offset;
			break;

		case SEEK_CUR: /* Move current offset */
			rc = file->f_pos + offset;
			break;

		default: /* This can never happen - like seeking at the end of the file in our case :-)*/
			rc = -EINVAL;
	}
	
	/* Put the semaphore up and return the new llseek value */
	up(&lp->sem);
	return rc;
}

static ssize_t switch_module_cdev_read(struct file *file, char __user *buff, size_t count, loff_t *f_pos) {
	struct switch_module_local *lp;
	ssize_t ret;
	u8 tmp_data;
	char* bf_start;


	/* Structure initilization */
	ret = 0;
	lp = file->private_data;

	/* Check if we are done (could be numbers from 0 to 15) */
	if (*f_pos > 1) {
		return 0;
	}

	/* Acquire the lock */
	if (down_interruptible(&lp->sem)) {
		dev_err(lp->device, "Cannot acquire the device, it is used by a different process.\n");
		return -ERESTARTSYS;
	}

	/* Read data from the device iff the starting offset is 0 */
	if (*f_pos == 0) {
		tmp_data = read_device(lp);
		snprintf(lp->loc_buff, BUFF_SIZE, "%d\n",tmp_data);
	}

	/* Send the data based on on offset */
	bf_start = lp->loc_buff + *f_pos;
	ret = strnlen(bf_start, BUFF_SIZE);
	if (ret == 0) {
		// We don't have nothing to send
		up(&lp->sem);
		return 0;
	}

	/* Check the buffer size and send the maximal amount of data */
	if (ret > count) {
		ret = count;
	}

	/* Send data to user and shift the file pointer */
	if (copy_to_user(buff, bf_start, ret)) {
		up(&lp->sem);
		return -EFAULT;
	}

	*f_pos += ret;
	up(&lp->sem);
	return ret;
}

static ssize_t switch_module_cdev_write(struct file *file, const char __user *buff, size_t count, loff_t *f_pos) {
	/* It is not allowed to write into the device */
	return -EINVAL;
}

static int switch_module_cdev_open(struct inode *inode, struct file *filp) {
	/* The device can be opened for reading only, write is not allowed because we cannot
	 * move the switch using the write operation */
	struct switch_module_local *lp;

	/* Get the parent container and store it into the private data pointer inside the fs ops */
	lp = container_of(inode->i_cdev, struct switch_module_local, cdev);
	filp->private_data = lp;

	/* Check we opened the device read only */
	//if ((filp->f_flags & O_ACCMODE) != O_RDONLY) {
	//	dev_err(lp->device, "Device can be opened in the read-only mode.\n");
	//	return -EPERM;
	//}

	return 0;
}

static int switch_module_cdev_release(struct inode *inode, struct file *filp) {
	/* Nothing special needs to be done in the release mode */
	filp->private_data = NULL;
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.llseek = switch_module_cdev_llseek,
	.read = switch_module_cdev_read,
	.write = switch_module_cdev_write,
	.open = switch_module_cdev_open,
	.release = switch_module_cdev_release,
	.unlocked_ioctl = switch_module_ioctl,
};

static int switch_module_cdev_init(struct platform_device *pdev) {
	int rc = 0;
	struct device *dev = &pdev->dev;
	struct switch_module_local *lp = dev_get_drvdata(dev);
	unsigned int n_major = 0;
	unsigned int n_minor = 0;

	/* Initialize the semaphore */
	sema_init(&lp->sem, 1);

	/* Allocate MAJOR and MINOR numbers */
	rc = alloc_chrdev_region(&lp->devid, 0, 1, DRIVER_NAME);
	if (rc < 0) {
		dev_err(dev, "Unable to allocate the major and minor numbers");
		return rc;
	}

	n_major = MAJOR(lp->devid);
	n_minor = MINOR(lp->devid);
	dev_info(dev, "cdev init MAJOR=%d and MINOR=%d\n",n_major, n_minor);

	/* Create the sysfs class */
    lp->sysclass = class_create(THIS_MODULE, DRIVER_SYSFS_CLASS);
	if (IS_ERR(lp->sysclass)) {
		dev_err(dev, "Error during the sysfs class creation\n");
		rc = -EFAULT;
		goto err_dealloc_chrdev;
	}

	/* Create device and register it in the sysfs */
	lp->device = device_create(lp->sysclass, dev, lp->devid, NULL, DEVICE_ID_STR, lp->devid);
	if (IS_ERR(lp->device)) {
		dev_err(dev, "Error during the device creation.\n");
		rc = -EFAULT;
		goto err_class_create;
	}

	/* Register the character device */
	dev_info(dev, "Registering the cdev.\n");
	cdev_init(&lp->cdev, &fops);
	lp->cdev.owner = THIS_MODULE;

	rc = cdev_add(&lp->cdev, lp->devid, 1);
	if (rc < 0)  {
		dev_err(dev, "Error during the cdev_add operation.\n");
		goto err_create_device;
	}
	
	dev_info(dev, "cdev init done!\n");
	return 0;

err_create_device:
	device_destroy(lp->sysclass, lp->devid);
err_class_create:
	class_destroy(lp->sysclass);
err_dealloc_chrdev:
	unregister_chrdev_region(lp->devid, 1);
	lp->devid = 0;
	return rc;
}

static void switch_module_cdev_exit(struct platform_device *pdev) {
	/* Deallocate already allocated structures */
	struct device *dev = &pdev->dev;
	struct switch_module_local *lp = dev_get_drvdata(dev);

	dev_info(dev, "Running the cdev exit handler.\n");
	cdev_del(&lp->cdev);
	device_destroy(lp->sysclass, lp->devid);
	class_destroy(lp->sysclass);
	unregister_chrdev_region(lp->devid, 1);
	lp->devid = 0;
	lp->sysclass = NULL;
	lp->device = NULL;
}

/* ==================================================================
 		Platform dependent callbacks
   ================================================================== */
   
static int switch_module_probe(struct platform_device *pdev) {
	struct resource *r_mem; /* IO mem resources */
	struct device *dev = &pdev->dev;
	struct switch_module_local *lp = NULL;

	int rc = 0;
	/* Get iospace for the device and allocate the local device structure */
	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_mem) {
		dev_err(dev, "invalid address\n");
		return -ENODEV;
	}

	lp = (struct switch_module_local *) kmalloc(sizeof(struct switch_module_local), GFP_KERNEL);
	if (!lp) {
		dev_err(dev, "Cound not allocate switch-module device\n");
		return -ENOMEM;
	}

	/* Add the local structure to private data section */
	dev_set_drvdata(dev, lp);
	lp->mem_start = r_mem->start;
	lp->mem_end = r_mem->end;
	lp->mask = SWITCH_INIT_MASK;

	/* Allocate the region exclusively for the device */
	if (!request_mem_region(lp->mem_start,
				lp->mem_end - lp->mem_start + 1,
				DRIVER_NAME)) {
		dev_err(dev, "Couldn't lock memory region at %p\n",
			(void *)lp->mem_start);
		rc = -EBUSY;
		goto err_region;
	}

	/* Map the IO space to kernel virtual space */
	lp->base_addr = ioremap(lp->mem_start, lp->mem_end - lp->mem_start + 1);
	if (!lp->base_addr) {
		dev_err(dev, "switch-module: Could not allocate iomem\n");
		rc = -EIO;
		goto err_release;
	}

	/* Initialize the chardevice */
	if (switch_module_cdev_init(pdev)) {
		dev_err(dev, "switch-module: Could not initialize characted device\n");
		rc = -EFAULT;
		goto cdev_init_err;
	}

	dev_info(dev,"switch-module at 0x%08x mapped to 0x%08x\n",
		(unsigned int __force)lp->mem_start,
		(unsigned int __force)lp->base_addr);
	return 0;

cdev_init_err:
	switch_module_cdev_exit(pdev);
	iounmap(lp->base_addr);
err_release:
	release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
err_region:
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	return rc;
}

static int switch_module_remove(struct platform_device *pdev) {
	struct device *dev = &pdev->dev;
	struct switch_module_local *lp = dev_get_drvdata(dev);

	dev_info(dev, "Removing the switch module.\n");
	switch_module_cdev_exit(pdev);
	iounmap(lp->base_addr);
	release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	return 0;
}


static struct of_device_id switch_module_of_match[] = {
	{ .compatible = "pb,swmodule-1.0", },
	{ /* end of list */ },
};

static struct platform_driver switch_module_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= switch_module_of_match,
	},
	.probe		= switch_module_probe,
	.remove		= switch_module_remove,
};

static int __init switch_module_init(void)
{
	printk(KERN_INFO "switch-module driver - platform registration.\n");
	return platform_driver_register(&switch_module_driver);
}


static void __exit switch_module_exit(void)
{
	platform_driver_unregister(&switch_module_driver);
	printk(KERN_ALERT "switch-module driver - platform unregistration.\n");
}

module_init(switch_module_init);
module_exit(switch_module_exit);

/* Standard module information, edit as appropriate */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pavel Benacek");
MODULE_DESCRIPTION("switch-module - example module for SWITCH device reaad.");
