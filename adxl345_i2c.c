#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/delay.h>

#include "adxl345.h"

static DECLARE_BITMAP(minors, N_I2C_MINORS);
static struct class *adxl345_class;

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

// Open function
static int adxl345_open(struct inode *inode, struct file *filp) 
{
    struct adxl345_data *adxl345;
    int status = -ENXIO;

    mutex_lock(&device_list_lock);

    list_for_each_entry(adxl345, &device_list, device_entry) 
    {
        if (adxl345->devt == inode->i_rdev) 
        {
            adxl345->users++;
            filp->private_data = adxl345;
            nonseekable_open(inode, filp);
            status = 0;
            break;
        }
    }
    mutex_unlock(&device_list_lock);
    return status;
}

// Release function
static int adxl345_release(struct inode *inode, struct file *filp) 
{
    struct adxl345_data *adxl345 = filp->private_data;

    mutex_lock(&device_list_lock);
    adxl345->users--;
    mutex_unlock(&device_list_lock);

    return 0;
}

// Write function
static ssize_t adxl345_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) 
{
    struct adxl345_data *adxl345 = filp->private_data;
    ssize_t status = 0;

    if (count > 4096)
        return -ENOMEM;

    if (copy_from_user(adxl345->tx_buffer, buf, count))
        return -EFAULT;

    mutex_lock(&adxl345->buf_lock);
    status = i2c_master_send(adxl345->client, adxl345->tx_buffer, count);
    mutex_unlock(&adxl345->buf_lock);

    return status;
}

// IOCTL function
static long adxl345_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct adxl345_data *adxl345;
    int status = 0;

    if (_IOC_TYPE(cmd) != ADXL345_IOC_MAGIC)
        return -ENOTTY;

    adxl345 = filp->private_data;

    if (_IOC_NR(cmd) > ADXL345_IOC_MAXNR) 
        return -ENOTTY;

    switch (cmd) {
        case ADXL345_IOC_SET_RANGE:
            // if (arg > 0x03) return -EINVAL;
            // mutex_lock(&adxl345->buf_lock);
            // // adxl345_send_command(adxl345, 0x31); // Set range command
            // // adxl345_send_command(adxl345, (__u8)arg);
            // mutex_unlock(&adxl345->buf_lock);
            break;

        default:
            return -ENOTTY;
    }

    return status;
}

// File operations structure
static const struct file_operations adxl345_fops = {
    .owner = THIS_MODULE,
    .open = adxl345_open,
    .release = adxl345_release,
    .write = adxl345_write,
    .unlocked_ioctl = adxl345_ioctl,
    .llseek = no_llseek,
};

// Probe function
static int adxl345_probe(struct i2c_client *client, const struct i2c_device_id *id) 
{
    struct adxl345_data *adxl345;
    int status;
    unsigned long minor;
    struct device *dev;

    adxl345 = kzalloc(sizeof(*adxl345), GFP_KERNEL);
    if (!adxl345)
        return -ENOMEM;

    adxl345->client = client;
    mutex_init(&adxl345->buf_lock);
    INIT_LIST_HEAD(&adxl345->device_entry);

    minor = find_first_zero_bit(minors, N_I2C_MINORS);
    if (minor < N_I2C_MINORS) {
        set_bit(minor, minors);
        adxl345->devt = MKDEV(ADXL345_MAJOR, minor);
        dev = device_create(adxl345_class, &client->dev, adxl345->devt, adxl345, "adxl345_i2c%d", minor);
        status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
    } else {
        dev_dbg(&client->dev, "no minor number available!\n");
        status = -ENODEV;
    }

    if (status == 0) {
        list_add(&adxl345->device_entry, &device_list);
        i2c_set_clientdata(client, adxl345);
    } else {
        kfree(adxl345);
    }

    return status;
}

// Remove function
static void adxl345_remove(struct i2c_client *client)
{
    struct adxl345_data *adxl345 = i2c_get_clientdata(client);

    // Prevent new opens
    mutex_lock(&device_list_lock);
    // Make sure ops on existing fds can abort cleanly
    mutex_lock(&adxl345->i2c_lock);
    adxl345->client = NULL;
    mutex_unlock(&adxl345->i2c_lock);

    list_del(&adxl345->device_entry);
    device_destroy(adxl345_class, adxl345->devt);
    clear_bit(MINOR(adxl345->devt), minors);
    if (adxl345->users == 0)
        kfree(adxl345);

    mutex_unlock(&device_list_lock);
}

// I2C driver structure
static const struct i2c_device_id adxl345_id[] = {
    { "adxl345", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, adxl345_id);

static struct i2c_driver adxl345_driver = {
    .driver = {
        .name = "adxl345",
        .owner = THIS_MODULE,
    },
    .probe = adxl345_probe,
    .remove = adxl345_remove,
    .id_table = adxl345_id,
};

// Send command function
static int adxl345_send_command(struct adxl345_data *adxl345, u8 command) {
    int ret;

    mutex_lock(&adxl345->i2c_lock);
    if (!adxl345->client) {
        mutex_unlock(&adxl345->i2c_lock);
        return -ENODEV;
    }

    ret = i2c_master_send(adxl345->client, &command, 1);
    mutex_unlock(&adxl345->i2c_lock);

    return ret;
}

// Send data function
static int adxl345_send_data(struct adxl345_data *adxl345, const u8 *data, size_t len) {
    int ret;

    mutex_lock(&adxl345->i2c_lock);
    if (!adxl345->client) {
        mutex_unlock(&adxl345->i2c_lock);
        return -ENODEV;
    }

    ret = i2c_master_send(adxl345->client, data, len);
    mutex_unlock(&adxl345->i2c_lock);

    return ret;
}

// Module init function
static int __init adxl345_init(void) 
{
    int status;
    printk(KERN_INFO "Initiate ADXL345 I2C Driver.\n");
    adxl345_class = class_create(THIS_MODULE, "adxl345_driver");
    if (IS_ERR(adxl345_class)) {
        return PTR_ERR(adxl345_class);
    }

    status = register_chrdev(ADXL345_MAJOR, "adxl345_driver", &adxl345_fops);
    if (status < 0) {
        class_destroy(adxl345_class);
        return status;
    }

    status = i2c_add_driver(&adxl345_driver);
    if (status < 0) {
        unregister_chrdev(ADXL345_MAJOR, adxl345_driver.driver.name);
        class_destroy(adxl345_class);
    }

    return status;
}

// Module exit function
static void __exit adxl345_exit(void) 
{
    i2c_del_driver(&adxl345_driver);
    unregister_chrdev(ADXL345_MAJOR, adxl345_driver.driver.name);
    class_destroy(adxl345_class);
}

module_init(adxl345_init);
module_exit(adxl345_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("ADXL345 I2C Driver");
MODULE_VERSION("1.0");

