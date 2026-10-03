/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
#include <linux/semaphore.h>

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Zachary W");
MODULE_LICENSE("Dual BSD/GPL");

int aesd_open(struct inode *inode, struct file *filp);
int aesd_release(struct inode *inode, struct file *filp);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
int aesd_init_module(void);
void aesd_cleanup_module(void);

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);

;    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
    * TODO: handle read
    */

    size_t byteOffset;
    if(down_interruptible(&aesd_device.writeLock) == 0)
    {

        struct aesd_buffer_entry* readBuffer =  aesd_circular_buffer_find_entry_offset_for_fpos(aesd_device.devBuffer, *f_pos, &byteOffset);
        if(readBuffer == NULL)
        {
            up(&aesd_device.writeLock);
            retval = 0;
            return retval;
        }

        int unread = copy_to_user(buf, (void*)(readBuffer->buffptr), readBuffer->size);
        if(unread != 0)
        {
            PDEBUG("Failed to copy all bytes to userspace!\n");
        }

        retval = (readBuffer->size);
        *f_pos += retval; 
        up(&aesd_device.writeLock);
    }
        
        return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    
     //Allocate memory and use for copy from userspace
    char* inputBuffer = kmalloc(count, GFP_KERNEL);
    if(inputBuffer == NULL)
    {
        PDEBUG("Unable to allocate mem!\n");
        //Fail and return here
        retval = -ENOMEM;
        return retval;
    }
    int notCopied = copy_from_user(inputBuffer, buf, count);
    if(notCopied != 0)
    {
        PDEBUG("Not copied!\n");
        //Fail and return here
        retval = -ENOMEM;
        kfree(inputBuffer);
        return retval;
    }
    PDEBUG("Checking data\n");
    //if count-1 (last data index) is \n then we can add to circ buffer if not, store until (maybe in another circ buffer?) \n received
    if(inputBuffer[count-1] != '\n')
    {
        PDEBUG("Entering partial entry\n");
        //store in another buffer until \n received
        int result = down_interruptible(&aesd_device.writeLock);
        if(result == 0)
        {
            char* tmpPtr = aesd_device.partialCmd->buffptr;
            aesd_device.partialCmd->buffptr = kmalloc(aesd_device.partialCmd->size + count, GFP_KERNEL);
            memcpy(aesd_device.partialCmd->buffptr, tmpPtr, aesd_device.partialCmd->size);
            memcpy(&aesd_device.partialCmd->buffptr[aesd_device.partialCmd->size], inputBuffer, count);
            aesd_device.partialCmd->size += count;
            kfree(tmpPtr);
            up(&aesd_device.writeLock);
            PDEBUG("Received partial entry\n");
            retval = count;
            return retval;
        }

    }
    else
    {
        if(aesd_device.partialCmd->size != 0)
        {
            PDEBUG("Entering partial entry\n");
            int result = down_interruptible(&aesd_device.writeLock);
            if(result == 0)
            {
                //store in another buffer until \n received
                char* tmpPtr = aesd_device.partialCmd->buffptr;
                aesd_device.partialCmd->buffptr = kmalloc(aesd_device.partialCmd->size + count, GFP_KERNEL);
                memcpy(aesd_device.partialCmd->buffptr, tmpPtr, aesd_device.partialCmd->size);
                memcpy(aesd_device.partialCmd->buffptr + aesd_device.partialCmd->size, inputBuffer, count);
                aesd_device.partialCmd->size += count;
                kfree(tmpPtr);
                up(&aesd_device.writeLock);
                PDEBUG("Received partial entry\n");
            }
        }
        else
        {
            PDEBUG("Entering full entry\n");
            int result = down_interruptible(&aesd_device.writeLock);
            if(result == 0)
            {
                aesd_device.partialCmd->buffptr = inputBuffer;
                aesd_device.partialCmd->size = count;
                up(&aesd_device.writeLock);
                *f_pos += (count);// * sizeof(char));
                PDEBUG("Received complete entry\n");
            }
        }

        //Write to circ buffer
        int result = down_interruptible(&aesd_device.writeLock);
        if(result == 0)
        {
            aesd_circular_buffer_add_entry(aesd_device.devBuffer, aesd_device.partialCmd);
            retval = aesd_device.partialCmd->size;
            //reset partialCmd size for next cmd once sucessfully written
            aesd_device.partialCmd->size = 0;
            up(&aesd_device.writeLock);
            PDEBUG("Wrote to circular buffer\n");
            aesd_device.partialCmd->buffptr = NULL;
        }
    }

    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    //printk(KERN_INFO "Made it to aesd portion.\n");
    aesd_device.devBuffer = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    if(aesd_device.devBuffer == NULL)
    {
        PDEBUG("Failed to allocate mem for dev buffer!\n");
    }
    aesd_circular_buffer_init(aesd_device.devBuffer);
    
    aesd_device.partialCmd = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
    if(aesd_device.partialCmd == NULL)
    {
        PDEBUG("Faield to allocate mem for partial cmd buffer entry!\n");
    }
    //printk(KERN_INFO "Setting up aesd cdev\n");
    result = aesd_setup_cdev(&aesd_device);
    
    sema_init(&aesd_device.writeLock, 1);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    //aesd_circular_buffer_clean(&aesd_device.devBuffer);
    struct aesd_buffer_entry* entryPtr;
    for(int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
    {
        kfree(aesd_device.devBuffer->entry[i].buffptr);
    }
    kfree(aesd_device.devBuffer);
    kfree(aesd_device.partialCmd);
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
