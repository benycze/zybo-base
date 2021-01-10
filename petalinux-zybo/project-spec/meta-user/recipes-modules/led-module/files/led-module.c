/*  led-module.c - Example device driver for the LED device

* Copyright (C) 2013 - 2016 Pavel Benacek
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
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

/* Declare IOCTL handlers */
#define LED_IOCTL_MAGIC				'l'
#define LED_IOCTL_GET_INIT 			_IOR(LED_IOCTL_MAGIC, 0, int)
#define LED_IOCTL_SET_INIT			_IOW(LED_IOCTL_MAGIC, 1, int)
#define LED_IOCTL_GET_MASK			_IOR(LED_IOCTL_MAGIC, 2, int)
#define LED_IOCTL_SET_MASK			_IOW(LED_IOCTL_MAGIC, 3, int)
#define LED_IOCTL_RESET				_IO(LED_IOCTL_MAGIC, 4)

/* Configuration related to driver names, etc */
#define DRIVER_NAME "led_module"
#define DRIVER_SYSFS_CLASS "led_module"
#define DEVICE_ID_STR "led_module-%d"

/* Local driver buffer size */
#define BUFF_SIZE 32

/* We are able to work with four LEDs */
#define LED_INIT_MASK 0xF
/* Helping constants */
#define LED_INIT_VALUE 0x0
#define LED_OFFSET 0x0

/**
 * @brief Structure with driver settings relate to the HW
 * 
 */
struct led_io_config {
	u8 led_mask_val;	/* Mask used during the IO write - default value is LED_INIT_MASK */
	u8 led_init_val; 	/* Initial value used during the reset - default value is LED_INIT_VALUE */
};

/**
 * @brief Local structure with private module data used in all
 * calls
 * 
 */
struct led_module_local {
	unsigned long 			mem_start;		/* Start of IO memory */
	unsigned long 			mem_end;		/* End of IO memory */
	void __iomem 			*base_addr;		/* Base address of iomaped region */

	struct class			*sysclass;		/* sysfs class for the device */
	struct device			*device;		/* Allocated device structure */

	dev_t					devid;			/* Allocated device ID for MAJOR and MINOR */
	struct semaphore 		sem;			/* Semaphore for the mutual access to read/write */
	struct cdev   			cdev; 			/* Characted device structure */

	struct led_io_config	led_io_conf;	/* Configuration of the LED driver */
};

/**
 * @brief Initial method for data writing into the HW
 * 
 * @param led_data LED data to write
 * @param addr Destination address
 * @param Mask used for the IO operation
 */
static void write_led_data(u8 led_data, volatile void __iomem *addr, u8 mask) {
	/* The ARM version is using the WBM before any IO operation, the WMB is inserted
	if you want to port it on different device */
	u8 write_data = led_data & mask;
	iowrite8(write_data, addr);
	#if !defined(CONFIG_ARM)
	wmb();
	#endif
}

/* ==================================================================
 		Char device callbacks
   ================================================================== */

static long led_module_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	struct led_module_local *lp;
	struct led_io_config	*lc;
	long rc;

	lp = file->private_data;
	lc = &lp->led_io_conf;
	rc = 0;

	if (down_interruptible(&lp->sem)) {
		dev_err(lp->device, "Cannot acquire the device, it is being used by a different process.\n");
		return -ERESTARTSYS;
	}

	/* Generally, we allow to read data without a superuser account.
	Writing is, on the other hand allowed to the owner of the device or
	to a user which has the SYSADMIN capability.
	
	The we are returning the value by a pointer passed via the \p arg argument
	*/
	switch (cmd) {
	case LED_IOCTL_GET_INIT:
		rc = put_user(lc->led_init_val, (u8 __user*) arg);
		break;
	case LED_IOCTL_SET_INIT:
		if(!capable(CAP_SYS_ADMIN)) {
			return -EPERM;
		}
		rc = get_user(lc->led_init_val, (u8 __user*) arg);
		break;
	case LED_IOCTL_GET_MASK:
		rc = put_user(lc->led_mask_val, (u8 __user*) arg);
		break;
	case LED_IOCTL_SET_MASK:
		if(!capable(CAP_SYS_ADMIN)) {
			return -EPERM;
		}
		rc = get_user(lc->led_mask_val, (u8 __user*) arg);
		break;
	case LED_IOCTL_RESET:
		write_led_data(lc->led_init_val, LED_OFFSET, lc->led_mask_val);
		rc = 0;
		break;
	default:
		dev_info(lp->device, "Invalid ioctl cmd = 0x%08x\n", cmd);
		rc = -ENOTTY;
		break;
	}

	up(&lp->sem);
	return rc;
}

static loff_t led_module_cdev_llseek(struct file *file, loff_t offset, int whence) {
	/* Seeking in our case resets the device to default state because it doesn't remember 
	all passed data */
	struct led_module_local *lp;
	struct led_io_config *lc;
	loff_t rc;

	lp = file->private_data;
	lc = &lp->led_io_conf;
	if (down_interruptible(&lp->sem)) {
		dev_err(lp->device, "Cannot acquire the device, it is used by a different process.\n");
		return -ERESTARTSYS;
	}

	/* Restart the status and seek the offset based on whence */
	write_led_data(lc->led_init_val, LED_OFFSET, lc->led_mask_val);
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

static ssize_t led_module_cdev_read(struct file *file, char __user *buff, size_t count, loff_t *f_pos) {
	/* It is not allowed to read any data there, thereofe we will return error but we shouldn't get there */
	return -EFAULT;
}

static ssize_t led_module_cdev_write(struct file *file, const char __user *buff, size_t count, loff_t *f_pos) {
	struct led_module_local *lp;
	struct led_io_config 	*lc;
	u8 idx;
	unsigned long to_copy;
	ssize_t rc = 0;

	/* Set the buff size based on your requirements */
	size_t buff_size = BUFF_SIZE;
	u8 drv_buff[BUFF_SIZE];

	/* Try to lock the device, we need to exit if the process is waken up - we don't want to hang there */
	lp = file->private_data;
	lc = &lp->led_io_conf;
	if (down_interruptible(&lp->sem)) {
		dev_err(lp->device, "Cannot acquire the device, it is used by a different process.\n");
		return -ERESTARTSYS;
	}

	/* Get the data, write them to the device and update offsets, etc */
	to_copy = buff_size;
	if (count < buff_size) {
		to_copy = count;
	}

	/* Copy data from the user space, the function returns 0 iff all data were copied from the
	  user space */
	if (copy_from_user(drv_buff, buff, to_copy)) {
		dev_err(lp->device, "Cannot read data from the user space in cdev write routine.\n");
		rc = -EFAULT;
		goto cdev_write_out;
	}

	for (idx = 0; idx < to_copy; idx++) {
		/* Skip null charactets */
		if (drv_buff[idx] == '\0') continue;

		write_led_data(drv_buff[idx], lp->base_addr + LED_OFFSET, lc->led_mask_val);
		*f_pos += 1;
	}

	/* Return the number of bytes we wrote to the LED device :-) */
	rc = to_copy;

	cdev_write_out: 
		up(&lp->sem);

	return rc;
}

static int led_module_cdev_open(struct inode *inode, struct file *filp) {
	/* The driver there needs to check if the device was opened for writing, reading is not allowed there
	because it doesn't make any sense to read how LEDs are shining. */
	struct led_module_local *lp;

	/* Get the local structure and store it into the file_private data for other calls */
	lp = container_of(inode->i_cdev, struct led_module_local, cdev);
	filp->private_data = lp;

	/* Check if we are working with write_only, return error if not - defined in fnctl.h */
	if ((filp->f_flags & O_ACCMODE) != O_WRONLY) {
		dev_err(lp->device, "Device can be opened as WRITE ONLY!\n");
		return -EPERM;
	}

	return 0;
}

static int led_module_cdev_release(struct inode *inode, struct file *filp) {
	/* Nothing special to do here ... */
	return 0;
}

/**
 * @brief Structure with CDEV callbacks used by the
 * kernel
 * 
 */
struct file_operations fops = {
	.owner = THIS_MODULE,
	.llseek = led_module_cdev_llseek,
	.read = led_module_cdev_read,
	.write = led_module_cdev_write,
	.open = led_module_cdev_open,
	.release = led_module_cdev_release,
	.unlocked_ioctl = led_module_ioctl,
};

static int led_module_cdev_init(struct platform_device *pdev) {
	int rc = 0;
	struct device *dev = &pdev->dev;
	struct led_module_local *lp = dev_get_drvdata(dev);
	unsigned int n_major = 0;
	unsigned int n_minor = 0;

	dev_info(dev, "running the cdev init.\n");
	/* Prepare the samaphore - one device is allowed to work with the
	 led driver */
	sema_init(&lp->sem, 1);

	/* Dynamic allocation of Major number for the cdev */
	rc = alloc_chrdev_region(&lp->devid, 0, 1, DRIVER_NAME);
	if (rc < 0) {
		dev_err(dev, "error during the MAJOR and MINOR allocation\n");
		return rc;
	}

	n_major = MAJOR(lp->devid);
	n_minor = MINOR(lp->devid);
	dev_info(dev, "cdev init MAJOR=%d and MINOR=%d\n",n_major, n_minor);

	/* create sysfs class to have easier access to it */
    lp->sysclass = class_create(THIS_MODULE, DRIVER_SYSFS_CLASS);
	if (IS_ERR(lp->sysclass)) {
		dev_err(dev, "error during the sysfs class creation\n");
		rc = -EFAULT;
		goto cdev_add_err;
	}

	/* Register the cdev into the kernel */
	cdev_init(&lp->cdev, &fops);
	lp->cdev.owner = THIS_MODULE;

	rc = cdev_add(&lp->cdev, lp->devid, 1);
	if (rc < 0)  {
		dev_err(dev, "error during the cdev_add operation");
		goto cdev_release_sysfsclass;
	}

	/* Create a device in /dev and register it to the sysfs */
	lp->device = device_create(lp->sysclass, dev, lp->devid, NULL, DEVICE_ID_STR, lp->devid);
	if(IS_ERR(lp->device)) {
		dev_err(dev, "error during the device creation.\n");
		rc = -EFAULT;
		goto cdev_del_device;
	}

	return 0;

	/* Error handlers */
	cdev_del_device:
		cdev_del(&lp->cdev);
	cdev_release_sysfsclass:
		class_destroy(lp->sysclass);
		lp->sysclass = NULL;
	cdev_add_err:
		unregister_chrdev_region(lp->devid, 1);
	return rc;
}

static void led_module_cdev_exit(struct platform_device *pdev) {
	/* Deallocate already allocated structures */
	struct device *dev = &pdev->dev;
	struct led_module_local *lp = dev_get_drvdata(dev);
	dev_info(dev, "Running the cdev exit");
	device_destroy(lp->sysclass, lp->devid);
	cdev_del(&lp->cdev);
	class_destroy(lp->sysclass);
	lp->sysclass = NULL;
	unregister_chrdev_region(lp->devid, 1);
}

/* ==================================================================
 		Platform dependent callbacks
   ================================================================== */

static int led_module_probe(struct platform_device *pdev) {
	struct resource *r_mem; /* IO mem resources */
	struct device *dev = &pdev->dev;
	struct led_module_local *lp = NULL;
	unsigned long mem_region_size = 0;

	int rc = 0;
	dev_info(dev, "Device Tree Probing\n");
	/* Get iospace for the device */
	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_mem) {
		dev_err(dev, "invalid address\n");
		return -ENODEV;
	}
	lp = (struct led_module_local *) kzalloc(sizeof(struct led_module_local), GFP_KERNEL);
	if (!lp) {
		dev_err(dev, "Cound not allocate led-module device\n");
		return -ENOMEM;
	}
	dev_set_drvdata(dev, lp);
	lp->mem_start = r_mem->start;
	lp->mem_end = r_mem->end;
	mem_region_size = lp->mem_end - lp->mem_start + 1;

	/* Setup LED IO structure with default value */
	lp->led_io_conf.led_init_val = LED_INIT_VALUE;
	lp->led_io_conf.led_mask_val = LED_INIT_MASK;

	/* Reserve the memory region acessed by the driver */
	if (!request_mem_region(lp->mem_start,
				mem_region_size,
				DRIVER_NAME)) {
		dev_err(dev, "Couldn't lock memory region at %p\n",
			(void *)lp->mem_start);
		rc = -EBUSY;
		goto mem_region_err;
	}

	/* Remap the IO space to the virtual kernel space */
	lp->base_addr = ioremap(lp->mem_start, mem_region_size);
	if (!lp->base_addr) {
		dev_err(dev, "led-module: Could not allocate iomem\n");
		rc = -EIO;
		goto ioremap_err;
	}

	dev_info(dev,"led-module at 0x%08x mapped to 0x%08x \n",
		(unsigned int __force)lp->mem_start,
		(unsigned int __force)lp->base_addr);

	/* Register the CDEV, create device and sysfs */
	rc = led_module_cdev_init(pdev);
	if (rc < 0) {
		dev_err(dev, "Unable to create a cdev.\n");
		goto ioremap_err;
	}
	
	return 0;

ioremap_err:
	release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
mem_region_err:
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	return rc;
}

static int led_module_remove(struct platform_device *pdev) {
	struct device *dev = &pdev->dev;
	struct led_module_local *lp = dev_get_drvdata(dev);
	dev_info(dev, "led-module is being removed.\n");
	led_module_cdev_exit(pdev);
	iounmap(lp->base_addr);
	release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	return 0;
}

/**
 * @brief This function is called during the 
 * 
 * @param pdev pdev structure we are working with 
 */
static void led_module_shutdown(struct platform_device *pdev) {
	struct device			*dev = &pdev->dev;
	struct led_module_local *lp = dev_get_drvdata(dev);
	struct led_io_config	*lc = &lp->led_io_conf;

	write_led_data(lc->led_init_val,  lp->base_addr + LED_OFFSET, lc->led_mask_val);
	dev_info(&pdev->dev, "led-module is shutting down.\n");
}

static struct of_device_id led_module_of_match[] = {
	{ .compatible = "pb,ledmodule-1.0", },
	{ /* end of list */ },
};

static struct platform_driver led_module_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= led_module_of_match,
	},
	.probe		= led_module_probe,
	.remove		= led_module_remove,
	.shutdown   = led_module_shutdown,
};

module_platform_driver(led_module_driver);

/* Standard module information, edit as appropriate */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pavel Benacek");
MODULE_DESCRIPTION("led-module - example module for LED device control");
