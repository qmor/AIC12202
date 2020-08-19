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

#define AIC12202_NR_PORTS	16	/* use 16 ports by default */
#define AIC12202_DEVICE_NAME "aic12202"

static unsigned long base = 0x140;
unsigned long aic12202_base = 0;
module_param(base, long, 0);

static unsigned int aic12202_major = 0;
struct aic12202_dev
{
	struct cdev cdev;
	int data;
};
static struct aic12202_dev *aic12202_devices = NULL;
static struct class *aic12202_class = NULL;
static int aic12202_ndevices = 1;

MODULE_AUTHOR ("Oleg Moroz");
MODULE_LICENSE("Dual BSD/GPL");


int aic12202_open (struct inode *inode, struct file *filp)
{
    unsigned int mj = imajor(inode);
	unsigned int mn = iminor(inode);
    printk(KERN_INFO "%s open dev %d %d\n",AIC12202_DEVICE_NAME,mj,mn);
    struct aic12202_dev *dev = NULL;
	
	if (mj != aic12202_major || mn < 0 || mn >= aic12202_ndevices)
	{
		printk(KERN_WARNING "[target] "	"No device found with minor=%d and major=%d\n", mj, mn);
		return -ENODEV; /* No such device */
	}
	
	dev = &aic12202_devices[mn];
	filp->private_data = dev; 
	
	if (inode->i_cdev != &dev->cdev)
	{
		printk(KERN_WARNING "[target] open: internal error\n");
		return -ENODEV; /* No such device */
	}
    
	return 0;
}


int aic12202_release (struct inode *inode, struct file *filp)
{
	return 0;
}



ssize_t aic12202_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}





/**
 * buf must be 4 bytes len
 * byte 0 - channel
 * byte 1,2 value byte1<<8|byte2
 * byte 3 - 0
 * */
ssize_t aic12202_write(struct file *filp, const char __user *buf, size_t count,	loff_t *f_pos)
{
    uint16_t value;
    uint8_t channel;

    if (count == 4 && aic12202_base!=0)
    {
        uint8_t _buf[4];
        copy_from_user(_buf, buf, count);
        channel = _buf[0];
        if (channel > (AIC12202_NR_PORTS-1))
            return 0;
        value = _buf[1]<<8|_buf[2];
        //printk(KERN_INFO "%02X %02X %02X %02X\n",_buf[0], _buf[1],_buf[2],_buf[3]);
        //printk(KERN_INFO "channel %d value %04X\n",channel, value);
        while ((1<<5)&inb(aic12202_base+0));
        outw (((channel<<12)|(value&4095)),aic12202_base+2);
        
        
    }
    return count;
}



unsigned int aic12202_poll(struct file *filp, poll_table *wait)
{
	return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
}


struct file_operations aic12202_fops = {
		.owner	 = THIS_MODULE,
		.read	 = aic12202_read,
		.write	 = aic12202_write,
		.poll	 = aic12202_poll,
		.open	 = aic12202_open,
		.release = aic12202_release,
};

/* then,  the interrupt-related device */

static void aic12202_destroy_device(struct aic12202_dev *dev, int minor, struct class *class)
{
	BUG_ON(dev == NULL || class == NULL);
	device_destroy(class, MKDEV(aic12202_major, minor));
	cdev_del(&dev->cdev);
	return;
}

static void aic12202_cleanup_module(int devices_to_destroy)
{
	int i;

	/* Get rid of character devices (if any exist) */
	if (aic12202_devices) {
		for (i = 0; i < devices_to_destroy; ++i) {
			aic12202_destroy_device(&aic12202_devices[i], i, aic12202_class);
		}
		kfree(aic12202_devices);
	}

	if (aic12202_class)
		class_destroy(aic12202_class);

	/* [NB] aic12202_cleanup_module is never called if alloc_chrdev_region()
	 * has failed. */
	unregister_chrdev_region(MKDEV(aic12202_major, 0),aic12202_ndevices);
	return;
}




static int aic12202_construct_device(struct aic12202_dev *dev, int minor,
	struct class *class)
{
	int err = 0;
	dev_t devno = MKDEV(aic12202_major, minor);
	struct device *device = NULL;

	BUG_ON(dev == NULL || class == NULL);

	/* Memory is to be allocated when the device is opened the first time */
	dev->data = 0;


	cdev_init(&dev->cdev, &aic12202_fops);
	dev->cdev.owner = THIS_MODULE;

	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
	{
		printk(KERN_WARNING "[target] Error %d while trying to add %s%d",
			err, AIC12202_DEVICE_NAME, minor);
		return err;
	}

	device = device_create(class, NULL, devno, NULL, AIC12202_DEVICE_NAME "%d", minor);

	if (IS_ERR(device)) {
		err = PTR_ERR(device);
		printk(KERN_WARNING "[target] Error %d while trying to create %s%d",err, AIC12202_DEVICE_NAME, minor);
		cdev_del(&dev->cdev);
		return err;
	}
	return 0;
}

/* Finally, init and cleanup */

int aic12202_init(void)
{
	int result;
	int i;
	int devices_to_destroy = 0;
	aic12202_base = base;


	//release_region(aic12202_base,AIC12202_NR_PORTS);
	if (! request_region(aic12202_base, AIC12202_NR_PORTS, AIC12202_DEVICE_NAME))
	{
		printk(KERN_INFO "%s: can't get I/O port address 0x%lx\n",AIC12202_DEVICE_NAME,aic12202_base);
		return -EACCES;
	}
	  int k =inb(aic12202_base+14);
	  int n =inb(aic12202_base+15);
	    if (!((k=='C'||k=='V')&&(n==8||n==16))) {
	    	printk(KERN_INFO "%s: Couldn't find device at base %04X\n",AIC12202_DEVICE_NAME,aic12202_base);
	    	return -ENODEV;
	    };


	    outb(0x80,aic12202_base+0);
	    printk(KERN_INFO "%s: find device\n",AIC12202_DEVICE_NAME);

	    dev_t dev = 0;

	    result = alloc_chrdev_region(&dev, 0, 1, AIC12202_DEVICE_NAME);



		if (result < 0)
		{
			printk(KERN_WARNING "[target] alloc_chrdev_region() failed\n");
			release_region(aic12202_base,AIC12202_NR_PORTS);
			return result;
		}

		aic12202_major = MAJOR(dev);
		printk(KERN_INFO "%s: major %d\n",AIC12202_DEVICE_NAME,aic12202_major);


		aic12202_class = class_create(THIS_MODULE, AIC12202_DEVICE_NAME);
		if (IS_ERR(aic12202_class)) {
			result = PTR_ERR(aic12202_class);
			goto fail;
		}

		/* Allocate the array of devices */
			aic12202_devices = (struct aic12202_dev *)kzalloc(aic12202_ndevices * sizeof(struct aic12202_dev),GFP_KERNEL);
			if (aic12202_devices == NULL) 
			{
				result = -ENOMEM;
				return result;
			}

			/* Construct devices */
			for (i = 0; i < aic12202_ndevices; ++i) {
				result = aic12202_construct_device(&aic12202_devices[i], i, aic12202_class);
				if (result) {
					devices_to_destroy = i;
					goto fail;
				}
			}
			
			return 0;
			
		
		fail:
		aic12202_cleanup_module(devices_to_destroy);
		release_region(aic12202_base,AIC12202_NR_PORTS);
		return result;
	}

	void aic12202_cleanup(void)
	{
		/* Make sure we don't leave work queue/tasklet functions running */
		unregister_chrdev(aic12202_major, AIC12202_DEVICE_NAME);
		release_region(aic12202_base,AIC12202_NR_PORTS);
		aic12202_cleanup_module(1);
		
	}

	module_init(aic12202_init);
	module_exit(aic12202_cleanup);
