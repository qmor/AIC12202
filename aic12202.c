/*
 * short.c -- Simple Hardware Operations and Raw Tests
 * short.c -- also a brief example of interrupt handling ("short int")
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 * $Id: short.c,v 1.16 2004/10/29 16:45:40 corbet Exp $
 */

/*
 * FIXME: this driver is not safe with concurrent readers or
 * writers.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/delay.h>	/* udelay */
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/io.h>

#define SHORT_NR_PORTS	16	/* use 16 ports by default */

/*
 * all of the parameters have no "short_" prefix, to save typing when
 * specifying them at load time
 */




/* default is the first printer port on PC's. "short_base" is there too
   because it's what we want to use in the code */
static unsigned long base = 0x140;
unsigned long short_base = 0;
module_param(base, long, 0);

static unsigned int cfake_major = 0;
struct cfake_dev
{
	struct cdev cdev;
	int data;
};
static struct cfake_dev *cfake_devices = NULL;
static struct class *cfake_class = NULL;
static int cfake_ndevices = 1;

MODULE_AUTHOR ("Oleg Moroz");
MODULE_LICENSE("Dual BSD/GPL");



/*
 * Atomicly increment an index into short_buffer
 */
static inline void short_incr_bp(volatile unsigned long *index, int delta)
{

}



int short_open (struct inode *inode, struct file *filp)
{
	return 0;
}


int short_release (struct inode *inode, struct file *filp)
{
	return 0;
}


/* first, the port-oriented device */

enum short_modes {SHORT_DEFAULT=0, SHORT_STRING, SHORT_MEMORY};

ssize_t do_short_read (struct inode *inode, struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}


/*
 * Version-specific methods for the fops structure.  FIXME don't need anymore.
 */
ssize_t short_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	//return do_short_read(filp->f_dentry->d_inode, filp, buf, count, f_pos);
	return 0;
}



ssize_t do_short_write (struct inode *inode, struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	return count;
}


ssize_t short_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	//return do_short_write(filp->f_dentry->d_inode, filp, buf, count, f_pos);
}




unsigned int short_poll(struct file *filp, poll_table *wait)
{
	return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
}






struct file_operations short_fops = {
		.owner	 = THIS_MODULE,
		.read	 = short_read,
		.write	 = short_write,
		.poll	 = short_poll,
		.open	 = short_open,
		.release = short_release,
};

/* then,  the interrupt-related device */

static void cfake_destroy_device(struct cfake_dev *dev, int minor, struct class *class)
{
	BUG_ON(dev == NULL || class == NULL);
	device_destroy(class, MKDEV(cfake_major, minor));
	cdev_del(&dev->cdev);
	return;
}

static void cfake_cleanup_module(int devices_to_destroy)
{
	int i;

	/* Get rid of character devices (if any exist) */
	if (cfake_devices) {
		for (i = 0; i < devices_to_destroy; ++i) {
			cfake_destroy_device(&cfake_devices[i], i, cfake_class);
		}
		kfree(cfake_devices);
	}

	if (cfake_class)
		class_destroy(cfake_class);

	/* [NB] cfake_cleanup_module is never called if alloc_chrdev_region()
	 * has failed. */
	unregister_chrdev_region(MKDEV(cfake_major, 0), cfake_ndevices);
	return;
}




static int cfake_construct_device(struct cfake_dev *dev, int minor,
	struct class *class)
{
	int err = 0;
	dev_t devno = MKDEV(cfake_major, minor);
	struct device *device = NULL;

	BUG_ON(dev == NULL || class == NULL);

	/* Memory is to be allocated when the device is opened the first time */
	dev->data = 0;


	cdev_init(&dev->cdev, &short_fops);
	dev->cdev.owner = THIS_MODULE;

	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
	{
		printk(KERN_WARNING "[target] Error %d while trying to add %s%d",
			err, "short", minor);
		return err;
	}

	device = device_create(class, NULL, /* no parent device */
		devno, NULL, /* no additional data */
		"short" "%d", minor);

	if (IS_ERR(device)) {
		err = PTR_ERR(device);
		printk(KERN_WARNING "[target] Error %d while trying to create %s%d",
			err, "short", minor);
		cdev_del(&dev->cdev);
		return err;
	}
	return 0;
}

/* Finally, init and cleanup */

int short_init(void)
{
	int result;
	int i;

	short_base = base;


	//release_region(short_base,SHORT_NR_PORTS);
	if (! request_region(short_base, SHORT_NR_PORTS, "short"))
	{
		printk(KERN_INFO "short: can't get I/O port address 0x%lx\n",short_base);
		return -EACCES;
	}
	  int k =inb(short_base+14);
	  int n =inb(short_base+15);
	    if (!((k=='C'||k=='V')&&(n==8||n==16))) {
	    	printk(KERN_INFO "short: ne plata\n",short_base);
	    	return -ENODEV;
	    };


	    outb(0x80,short_base+0);
	    printk(KERN_INFO "short: find device\n");

	    dev_t dev = 0;

	    result = alloc_chrdev_region(&dev, 0, 1, "short");



		if (result < 0)
		{
			printk(KERN_WARNING "[target] alloc_chrdev_region() failed\n");
			release_region(short_base,SHORT_NR_PORTS);
			return result;
		}

		cfake_major = MAJOR(dev);
		printk(KERN_INFO "short: major %d\n",cfake_major);


		cfake_class = class_create(THIS_MODULE, "short");
		if (IS_ERR(cfake_class)) {
			result = PTR_ERR(cfake_class);
			release_region(short_base,SHORT_NR_PORTS);
			return result;
		}

		/* Allocate the array of devices */
			cfake_devices = (struct cfake_dev *)kzalloc(
				cfake_ndevices * sizeof(struct cfake_dev),
				GFP_KERNEL);
			if (cfake_devices == NULL) {
				result = -ENOMEM;
				return result;
			}

			/* Construct devices */
			for (i = 0; i < cfake_ndevices; ++i) {
				result = cfake_construct_device(&cfake_devices[i], i, cfake_class);
				if (result) {

					return result;
				}
			}

		return 0;
	}

	void short_cleanup(void)
	{
		/* Make sure we don't leave work queue/tasklet functions running */
		unregister_chrdev(cfake_major, "short");
		release_region(short_base,SHORT_NR_PORTS);
	}

	module_init(short_init);
	module_exit(short_cleanup);
