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
#include <linux/ioctl.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

/* Configuration related to driver names, etc */
#define DRIVER_NAME "rgb-led-module"
#define DRIVER_SYSFS_CLASS "rgb-led-module"
#define DEVICE_ID_STR "rgb-led-module-%d"

/* PWM configuration & address space */
#define PWM_PERIOD_CLK 		4096
#define PWM_MAX_DIV			8

#define PWM_AXI_CTRL_REG_OFFSET 	0
#define PWM_AXI_PERIOD_REG_OFFSET 	8
#define PWM_AXI_DUTY_REG_OFFSET 	64

#define PWM_AXI_ENABLE_CMD  1
#define PWM_AXI_DISABLE_CMD 0

/* Supported IOCTL handlers */
#define LED_IOCTL_MAGIC			'l'
#define LED_IOCTL_GET_VAL		_IOR(LED_IOCTL_MAGIC, 0, unsigned long)
#define LED_IOCTL_SET_VAL		_IOW(LED_IOCTL_MAGIC, 1, unsigned long)
#define LED_IOCTL_SET_PERIOD	_IOW(LED_IOCTL_MAGIC, 2, unsigned long)
#define LED_IOCTL_GET_PERIOD	_IOR(LED_IOCTL_MAGIC, 3, unsigned long)

/* Othe configuration */
#define BUFF_SIZE			512
#define BUFF_CONF_STR_LEN	15

/**
 * @brief Decoded RGB values
 * 
 */
struct rgb_val {
	u32 r; /* Red 	*/
	u32 g; /* Green */
	u32 b; /* Blue 	*/
};

/**
 * @brief Decode the passed hex value to RGB structure
 * 
 * @param val Value to decode
 * @return struct rgb_val decoded RGB value
 */
static struct rgb_val decode_rgb(u32 val) {
	struct rgb_val ret;
	ret.r = (val >> 16) & 0xff;
	ret.g = (val >> 8) & 0xff;
	ret.b = val & 0xff;
	return ret;
}

/**
 * @brief Encode the passed RGB value to hex value
 * 
 * @param val Structure to encode
 * @return u32 value of encoded data
 */
static u32 encode_rgb(const struct rgb_val *val) {
	u32 r = val->r << 16;
	u32 g = val->g << 8;
	u32 b = val->b;
	return r | g | b;
}

/**
 * @brief Local structure with temporal device data
 * 
 */
struct rgb_led_module_local {
	unsigned long mem_start;
	unsigned long mem_end;
	void __iomem *base_addr;

	struct class	*sysclass;		/* sysfs class for the device */
	struct device	*device;		/* Allocated device structure */
	dev_t			 devid;			/* Assigned device ID */
	struct cdev		 cdev;			/* Characted device structure */

	struct semaphore 	sem;		/* Semaphore for the access serialization */
	struct rgb_val		rgbval;		/* Current set RGB value */
	u32	period;						/* PWM period value */

	char wr_buf[BUFF_SIZE];			/* Device buffer */
	char rd_buff[BUFF_SIZE];		/* Device read buffer */
};

/* ==================================================================
 		Device IO
   ================================================================== */

/**
 * @brief Scale the RGB value to the maximal 
 * 
 * @param period Number of clocks for the period
 * @param val Value to convert
 * @param pwm_max_on Fraction of the period where the duty cycle can be enabled
 * @return u32 New scaled duty cycle RGB value
 */
static u32 pwm_scale_rgb(int period, int val, int pwm_max_on) {	
	u32 max_clk_cycles = period / pwm_max_on;
	u32 scale = max_clk_cycles / 256;
	u32 ret = scale * val;
	return ret;
}

/**
 * @brief Setup one part of the color - PWM duty cycle configuration
 * 
 */
static void set_pwm_duty(const u32 val, void __iomem  *base) {
	void __iomem *addr = base + PWM_AXI_DUTY_REG_OFFSET;
	iowrite32(val, addr);
}

/**
 * @brief Set the pwm period value
 * 
 * @param period Period value to set
 * @param base Base address value
 */
static void set_pwm_period(const u32 period, void __iomem *base) {
	iowrite32(period, base + PWM_AXI_PERIOD_REG_OFFSET);
}

/**
 * @brief Set the rgb configuration into the device
 * 
 * @param val Structure with RGB configuration 
 * @param base Base value address
 */
static void set_rgb_config(const struct rgb_val *val, struct rgb_led_module_local *lp) {
	/* Re-scale the RGB values regarding the PWM configuration */
	struct rgb_val rs_val = {
		pwm_scale_rgb(lp->period, val->r, PWM_MAX_DIV),
		pwm_scale_rgb(lp->period, val->g, PWM_MAX_DIV),
		pwm_scale_rgb(lp->period, val->b, PWM_MAX_DIV)
	};

	/* Each RGB part has its own configuration  - each pwm duty cycle is shifted by
	4 bytes*/
	set_pwm_duty(rs_val.b, lp->base_addr);
	set_pwm_duty(rs_val.g, lp->base_addr + 4);
	set_pwm_duty(rs_val.r, lp->base_addr + 8);

	set_pwm_period(lp->period, lp->base_addr);

	/* Update the RGB configuration */
	lp->rgbval = *val;
}

/**
 * @brief Enable the PWM device
 * 
 * @param base Base value address
 */
static void enable_device(void __iomem *base) {
	iowrite32(PWM_AXI_ENABLE_CMD, base + PWM_AXI_CTRL_REG_OFFSET);
}

/**
 * @brief Disable the PWM device
 * 
 * @param base Base value address
 */
static void disable_device(void __iomem *base) {
	iowrite32(PWM_AXI_DISABLE_CMD, base + PWM_AXI_CTRL_REG_OFFSET);
}

/**
 * @brief Restart the device color
 * 
 * @param base Base device address
 */
static void reset_device_config(struct rgb_led_module_local *lp) {
	struct rgb_val val = {0,0,0};
	set_rgb_config(&val, lp);
}

/**
 * @brief Initialize the device - this function is typically called during the
 * module probe call.
 * 
 * @param lp Structure with the RGB device configuration
 */
static void init_device(struct rgb_led_module_local *lp) {
	reset_device_config(lp);
	enable_device(lp->base_addr);
}

/**
 * @brief Deinit the device - this is called during the shutdown or exit
 * calls
 * 
 * @param lm Structure with the RGB device configuration
 */
static void deinit_device(struct rgb_led_module_local *lp) {
	reset_device_config(lp);
	disable_device(lp->base_addr);
}

/* ==================================================================
 		Char device callbacks
   ================================================================== */

#ifdef DEBUG
	#define IOCTL_DEBUG_PRINT(dev,args...) {dev_info(dev,args);}
#else
	#define IOCTL_DEBUG_PRINT(dev,args...) {}
#endif

static long rgb_module_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	struct rgb_led_module_local *lp;
	long rc;
	unsigned long tmp_val;
	struct rgb_val rgb_val;

	lp = file->private_data;
	rc = 0;

	if (down_interruptible(&lp->sem)) {
		dev_err(lp->device, "Cannot acquire the device, it is being used by a different process.\n");
		return -ERESTARTSYS;
	}

	IOCTL_DEBUG_PRINT(lp->device, "IOCTL Handler has been called - cmd = 0x%x , arg = 0x%lx\n", cmd, arg);
	/* Generally, we allow to read data without a superuser account.
	Writing is, on the other hand allowed to the owner of the device or
	to a user which has the SYSADMIN capability.
	
	The we are returning the value by a pointer passed via the \p arg argument
	*/
	switch (cmd) {
	case LED_IOCTL_GET_VAL:
		tmp_val = encode_rgb(&lp->rgbval);
		rc = put_user(tmp_val, (unsigned long __user*) arg);
		IOCTL_DEBUG_PRINT(lp->device, "Sending the RGB value 0x%lx (rc = %ld)\n", tmp_val, rc);
		break;

	case LED_IOCTL_SET_VAL:
		if (!capable(CAP_SYS_ADMIN)) {
			IOCTL_DEBUG_PRINT(lp->device,"User is not capable to set led value\n");
			return -EPERM;
		}

		rc = get_user(tmp_val, (unsigned long __user*) arg);
		if (rc != 0) {
			IOCTL_DEBUG_PRINT(lp->device,"Cannot copy value from user space.\n");
			break;
		}

		rgb_val = decode_rgb(tmp_val);
		set_rgb_config(&rgb_val, lp);
		IOCTL_DEBUG_PRINT(lp->device, "Setting the RGB value 0x%x (rc = %ld)\n", encode_rgb(&rgb_val), rc);
		break;

	case LED_IOCTL_GET_PERIOD:
		rc = put_user(lp->period, (unsigned long __user*) arg);
		IOCTL_DEBUG_PRINT(lp->device, "Sending the RGB value 0x%x (rc = %ld)\n", lp->period, rc);
		break;

	case LED_IOCTL_SET_PERIOD:
		if (!capable(CAP_SYS_ADMIN)) {
			IOCTL_DEBUG_PRINT(lp->device,"User is not capable to set led value\n");
			return -EPERM;
		}
		
		rc = get_user(lp->period, (unsigned long __user*) arg);
		if (rc != 0) {
			IOCTL_DEBUG_PRINT(lp->device,"Cannot copy value from user space.\n");
			break;
		}

		IOCTL_DEBUG_PRINT(lp->device, "Setting the period value 0x%x (rc = %ld)\n", lp->period, rc);
		break;

	default:
		dev_info(lp->device, "Invalid ioctl cmd = 0x%08x\n", cmd);
		rc = -ENOTTY;
		break;
	}

	IOCTL_DEBUG_PRINT(lp->device, "IOCTL Handler has been finished (rc = %ld)\n", rc);
	up(&lp->sem);
	return rc;
}

static loff_t rgb_module_cdev_llseek(struct file *file, loff_t offset, int whence) {
	/* Seeking in our case resets the device to default state because it doesn't remember 
	all passed data */
	struct rgb_led_module_local *lp;
	loff_t rc;

	lp = file->private_data;
	if (down_interruptible(&lp->sem)) {
		dev_err(lp->device, "Cannot acquire the device, it is used by a different process.\n");
		return -ERESTARTSYS;
	}

	/* Restart the status and seek the offset based on whence */
	reset_device_config(lp);
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

static ssize_t rgb_module_cdev_read(struct file *file, char __user *buff, size_t count, loff_t *f_pos) {
	/* The read function will be much easier because the only thing it needs to do is to create the
	 * output string iff the current offset is 0.
	 */
	size_t rc;
	struct rgb_led_module_local *lp;
	const struct rgb_val *rgb_val;
	char *buff_start;
	size_t to_send;

	/* Acquire the lock and prepare data */
	buff_start = NULL;
	lp = file->private_data;
	rgb_val = &lp->rgbval;
	rc = 0;
	if (down_interruptible(&lp->sem)) {
		dev_err(lp->device, "Cannot acquire the device, it is used by a different process.\n");
		rc = -ERESTARTSYS;
		goto rgb_read_end;
	}

	if (*f_pos == 0) {
		snprintf(lp->rd_buff, BUFF_SIZE, "0x%x 0x%x 0x%x\n",
			rgb_val->r, rgb_val->g, rgb_val->b);
	}

	/* Send data to the user */
	buff_start = lp->rd_buff + *f_pos;
	to_send = strnlen(buff_start, BUFF_SIZE);
	if (to_send == 0) {
		/* Nothing to send to the user */
		rc = 0;
		goto rgb_read_end;
	}

	/* Check amount of data to send and correct it regarding the size of dest. buffer */
	if (to_send > count) {
		to_send = count;
	}

	/* Send data and move the pointer */
	if (copy_to_user(buff, buff_start, to_send)) {
		dev_err(lp->device, "Cannot write data to the user space in cdev read routine.\n");
		rc = -EFAULT;
		goto rgb_read_end;
	}

	rc = to_send;
	*f_pos += to_send;

rgb_read_end:
	up(&lp->sem);
	return rc;
}

static ssize_t rgb_module_cdev_write(struct file *file, const char __user *buff, size_t count, loff_t *f_pos) {
	/* Write into the device means that we need to extract the passed HEX value of RGB
	 * and set it via the device dependent calls. No defferred work is required here because we
	 * are writing one value only 
	 */
	size_t to_copy;
	size_t rc;
	size_t rem_buff;
	int matched;
	int str_len;
	char *buff_start;
	struct rgb_led_module_local *lp;
	struct rgb_val rgb_conf;

	/* Get the semaphore and receive user data */
	lp = file->private_data;
	rc = 0;
	if (down_interruptible(&lp->sem)) {
		dev_err(lp->device, "Cannot acquire the device, it is used by a different process.\n");
		return -ERESTARTSYS;
	}

	/* Check the buffer size and set the correct data count to acquire, also don't forget to 
	 * move the buffer pointer to the next free part
	 */
	rem_buff = BUFF_SIZE - *f_pos;
	if (count > rem_buff) {
		to_copy = rem_buff;
	} else {
		to_copy = count;
	}

	buff_start = lp->wr_buf + *f_pos;
	if (copy_from_user(buff_start, buff, to_copy)) {
		dev_err(lp->device, "Cannot read data from the user space in cdev write routine.\n");
		rc = -EFAULT;
		goto rgb_write_end;
	}

	/* Parse input if we have enough of data  - format is 0xAA 0xBB 0xCC */
	rc = to_copy;
	str_len = strnlen(lp->wr_buf, BUFF_SIZE);
	if (str_len < BUFF_CONF_STR_LEN) {
		/* Don't have enough data - skip to end*/
		goto rgb_write_end;
	}

	if (str_len > BUFF_CONF_STR_LEN) {
		/* More data than needed, report the error */
		dev_err(lp->device, "More data than needed the format is: 0xAA 0xBB 0xCC \n");
		rc = -EINVAL;
		/* Initialize the buffer */
		goto rgb_write_end;
	}

	/* Match Input data and configure the device */
	matched = sscanf(lp->wr_buf, "%x %x %x\n",
		&rgb_conf.r, &rgb_conf.g, &rgb_conf.b);

	if (matched != 3) {
		/* Invalid format of config data */
		dev_err(lp->device, "Invalid format of configuration data. Allowed format is: 0xAA 0xBB 0xCC \n");
		rc = -EINVAL;
		goto rgb_write_end;
	}

	set_rgb_config(&rgb_conf, lp);

	/* Shift the file pointer offset */
	*f_pos += rc;

rgb_write_end:
	up(&lp->sem);
	return rc;
}

static int rgb_module_cdev_open(struct inode *inode, struct file *filp) {
	struct rgb_led_module_local *lp;

	/* Get the local structure and store it into the file_private data for other calls */
	lp = container_of(inode->i_cdev, struct rgb_led_module_local, cdev);
	filp->private_data = lp;

	return 0;
}

static int rgb_module_cdev_release(struct inode *inode, struct file *filp) {
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
	.llseek = rgb_module_cdev_llseek,
	.read = rgb_module_cdev_read,
	.write = rgb_module_cdev_write,
	.open = rgb_module_cdev_open,
	.release = rgb_module_cdev_release,
	.unlocked_ioctl = rgb_module_ioctl,
};

static int rgb_module_cdev_init(struct platform_device *pdev) {
	int rc = 0;
	struct device *dev = &pdev->dev;
	struct rgb_led_module_local *lp = dev_get_drvdata(dev);
	unsigned int n_major = 0;
	unsigned int n_minor = 0;

	dev_info(dev, "Running the cdev init.\n");
	/* Prepare the samaphore - one device is allowed to work with the
	 led driver */
	sema_init(&lp->sem, 1);

	/* Dynamic allocation of Major number for the cdev */
	rc = alloc_chrdev_region(&lp->devid, 0, 1, DRIVER_NAME);
	if (rc < 0) {
		dev_err(dev, "Error during the MAJOR and MINOR allocation\n");
		return rc;
	}

	n_major = MAJOR(lp->devid);
	n_minor = MINOR(lp->devid);
	dev_info(dev, "cdev init MAJOR=%d and MINOR=%d\n",n_major, n_minor);

	/* create sysfs class to have easier access to it */
    lp->sysclass = class_create(THIS_MODULE, DRIVER_SYSFS_CLASS);
	if (IS_ERR(lp->sysclass)) {
		dev_err(dev, "Error during the sysfs class creation\n");
		rc = -EFAULT;
		goto cdev_add_err;
	}

	/* Register the cdev into the kernel */
	cdev_init(&lp->cdev, &fops);
	lp->cdev.owner = THIS_MODULE;

	rc = cdev_add(&lp->cdev, lp->devid, 1);
	if (rc < 0)  {
		dev_err(dev, "Error during the cdev_add operation");
		goto cdev_release_sysfsclass;
	}

	/* Create a device in /dev and register it to the sysfs */
	lp->device = device_create(lp->sysclass, dev, lp->devid, NULL, DEVICE_ID_STR, lp->devid);
	if(IS_ERR(lp->device)) {
		dev_err(dev, "Error during the device creation.\n");
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

static void rgb_module_cdev_exit(struct platform_device *pdev) {
	/* Deallocate already allocated structures */
	struct device *dev = &pdev->dev;
	struct rgb_led_module_local *lp = dev_get_drvdata(dev);
	dev_info(dev, "Running the cdev exit.\n");
	device_destroy(lp->sysclass, lp->devid);
	cdev_del(&lp->cdev);
	class_destroy(lp->sysclass);
	lp->sysclass = NULL;
	unregister_chrdev_region(lp->devid, 1);
}

/* ==================================================================
 		Platform dependent callbacks
   ================================================================== */

static int rgb_led_module_probe(struct platform_device *pdev) {
	struct resource *r_mem; /* IO mem resources */
	struct device *dev = &pdev->dev;
	struct rgb_led_module_local *lp = NULL;

	int rc = 0;
	dev_info(dev, "Device Tree Probing\n");
	/* Get iospace for the device */
	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_mem) {
		dev_err(dev, "Invalid address\n");
		return -ENODEV;
	}
	lp = (struct rgb_led_module_local *) kmalloc(sizeof(struct rgb_led_module_local), GFP_KERNEL);
	if (!lp) {
		dev_err(dev, "Could not allocate rgb-led-module device\n");
		return -ENOMEM;
	}

	dev_set_drvdata(dev, lp);
	lp->mem_start = r_mem->start;
	lp->mem_end = r_mem->end;

	if (!request_mem_region(lp->mem_start,
				lp->mem_end - lp->mem_start + 1,
				DRIVER_NAME)) {
		dev_err(dev, "Couldn't lock memory region at %p\n",
			(void *)lp->mem_start);
		rc = -EBUSY;
		goto err_region_req;
	}

	lp->base_addr = ioremap(lp->mem_start, lp->mem_end - lp->mem_start + 1);
	if (!lp->base_addr) {
		dev_err(dev, "rgb-led-module: Could not allocate iomem\n");
		rc = -EIO;
		goto err_remap;
	}

	/* Initialize the device with default values */
	lp->sysclass = NULL;
	lp->device = NULL;
	lp->period = PWM_PERIOD_CLK;
	init_device(lp);

	/* Initialize the character device */
	rc = rgb_module_cdev_init(pdev);
	if (rc) {
		dev_err(dev, "Unable to create a cdev.\n");
		goto err_cdev_init;
	}
	
	dev_info(dev,"rgb-led-module at 0x%08x mapped to 0x%08x\n",
		(unsigned int __force)lp->mem_start,
		(unsigned int __force)lp->base_addr);

	return 0;

err_cdev_init:
	rgb_module_cdev_exit(pdev);
	iounmap(lp->base_addr);
err_remap:
	release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
err_region_req:
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	return rc;
}

static int rgb_led_module_remove(struct platform_device *pdev) {
	struct device *dev = &pdev->dev;
	struct rgb_led_module_local *lp = dev_get_drvdata(dev);
	rgb_module_cdev_exit(pdev);
	deinit_device(lp);
	iounmap(lp->base_addr);
	release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	dev_info(&pdev->dev, "rgb-led-module is being removed.\n");
	return 0;
}

static void rgb_led_module_shutdown(struct platform_device *pdev) {
	struct device *dev = &pdev->dev;
	struct rgb_led_module_local *lp = dev_get_drvdata(dev);

	deinit_device(lp);
	dev_info(&pdev->dev, "rgb-led-module is shutting down.\n");
}

static struct of_device_id rgb_led_module_of_match[] = {
	{ .compatible = "pb,rgb-led-module-1.0", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, rgb_led_module_of_match);

static struct platform_driver rgb_led_module_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= rgb_led_module_of_match,
	},
	.probe		= rgb_led_module_probe,
	.remove		= rgb_led_module_remove,
	.shutdown   = rgb_led_module_shutdown,
};


module_platform_driver(rgb_led_module_driver);

/* Standard module information, edit as appropriate */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pavel Benacek");
MODULE_DESCRIPTION("rgb-led-module - loadable module for tri-color LED");
