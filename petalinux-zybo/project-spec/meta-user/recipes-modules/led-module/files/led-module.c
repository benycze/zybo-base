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

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#define DRIVER_NAME "led-module"

struct led_module_local {
	unsigned long mem_start;
	unsigned long mem_end;
	void __iomem *base_addr;
};

static int led_module_probe(struct platform_device *pdev) {
	struct resource *r_mem; /* IO mem resources */
	struct device *dev = &pdev->dev;
	struct led_module_local *lp = NULL;

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
	unsigned long mem_region_size = lp->mem_end - lp->mem_start + 1;

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
	dev_info(dev, "led-module is being removed.");
	iounmap(lp->base_addr);
	release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	return 0;
}

static void led_module_shutdown(struct platform_device *pdev) {
	// TODO: Implement write to offset here
	dev_info(&pdev->dev, "led-module is shutting down.");
}

static struct of_device_id led_module_of_match[] = {
	{ .compatible = "vendor,led-module", },
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

module_platform_driver(led_module_driver)

/* Standard module information, edit as appropriate */
MODULE_LICENSE("GPL");
MODULE_AUTHOR
    ("Pavel Benacek");
MODULE_DESCRIPTION
    ("led-module - example module for LED device control");