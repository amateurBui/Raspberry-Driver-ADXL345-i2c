#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DRIVER_NAME     "adxl345_driver"
#define CLASS_NAME      "adxl345"
#define DEVICE_NAME     "adxl345"

#define ADXL345_REG_DATAX0       0x32
#define ADXL345_REG_PWR_CTL     0x2D
#define ADXL345_REG_DATA_FORMAT 0x31
// List of ioctl command
#define ADXL345_IOCTL_MAGIC 'a'
#define ADXL345_IOCTL_READ_X _IOR(ADXL345_IOCTL_MAGIC, 1, int)
#define ADXL345_IOCTL_READ_Y _IOR(ADXL345_IOCTL_MAGIC, 2, int)
#define ADXL345_IOCTL_READ_Z _IOR(ADXL345_IOCTL_MAGIC, 3, int)

static struct i2c_client *adxl345_client;
static struct class* adxl345_class = NULL;
static struct device* adxl345_device = NULL;
static int major_number;

static int adxl345_read_data(struct i2c_client *client, int axis)
{
    u8 buf[6];
    s16 accel_data[3];

    if(i2c_smbus_read_i2c_block_data(client, ADXL345_REG_DATAX0, sizeof(buf), buf) < 0){
        printk(KERN_INFO "Failed to read accelerometer data!!!\n");
        return -EIO;
    }

    accel_data[0] = ((buf[1] << 8) | buf[0]);
    accel_data[1] = ((buf[3] << 8) | buf[2]);
    accel_data[2] = ((buf[5] << 8) | buf[4]);

    return accel_data[axis]/29;
}

static int adxl345_open(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "ADXL345 device opened\n");
    return 0;
}
static int adxl345_release(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "ADXL345 device closed\n");
    return 0;
}
static long adxl345_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int data;
    switch(cmd){
        case ADXL345_IOCTL_READ_X:
            data = adxl345_read_data(adxl345_client, 0);
            break;
        case ADXL345_IOCTL_READ_Y:
            data = adxl345_read_data(adxl345_client, 1);
            break;
        case ADXL345_IOCTL_READ_Z:
            data = adxl345_read_data(adxl345_client, 2);
            break;
        default:
            return -EINVAL;
    }

    if(copy_to_user((int __user *)arg, &data, sizeof(data))){
        return -EFAULT;
    }
    return 0;
}

static struct file_operations fops = {
    .open               = adxl345_open,
    .release            = adxl345_release,
    .unlocked_ioctl     = adxl345_ioctl,
};

static int adxl345_probe(struct i2c_client *client, const struct i2c_device_id *id)
{   
	int ret;
    adxl345_client = client;
    ret = i2c_smbus_write_byte_data(client, ADXL345_REG_DATA_FORMAT, 0x08);
    if (ret < 0) {
        printk(KERN_ERR "Failed to set data format for ADXL345\n");
        return ret;
    }
    ret = i2c_smbus_write_byte_data(client, ADXL345_REG_PWR_CTL, 0x08);
    if (ret < 0) {
        printk(KERN_ERR "Failed to start ADXL345 measurement\n");
        return ret;
    }
    // Create a character device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if(major_number < 0){
        printk(KERN_ERR "Failed to register a major number\n");
        return major_number;
        
    }
    printk(KERN_INFO "ADXL345 driver installed: %d\n", major_number);

    adxl345_class = class_create(THIS_MODULE, CLASS_NAME);
    if(IS_ERR(adxl345_class)){
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to create class\n");
        return PTR_ERR(adxl345_class);
    }
    adxl345_device = device_create(adxl345_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if(IS_ERR(adxl345_device)){
        class_destroy(adxl345_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to create device\n");
        return PTR_ERR(adxl345_device);
    }
    return 0;
}

static void adxl345_remove(struct i2c_client *client)
{
    device_destroy(adxl345_class, MKDEV(major_number, 0));
    class_unregister(adxl345_class);
    class_destroy(adxl345_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    printk(KERN_INFO "ADXL345 driver removed!!!\n");
}

static const struct of_device_id adxl345_of_match[] = {
    { .compatible = "analog,adxl345", },
    { },
}; MODULE_DEVICE_TABLE(of, adxl345_of_match);

static struct i2c_driver adxl345_driver = {
    .driver = {
        .name   = DRIVER_NAME,
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(adxl345_of_match),
    },
    .probe      = adxl345_probe,
    .remove     = adxl345_remove,
};

static int __init adxl345_init (void)
{
    printk(KERN_INFO "Initializing ADXL345 driver!!!\n");
    return i2c_add_driver(&adxl345_driver); // add driver into i2c of system
}

static void __exit adxl345_exit (void)
{
    printk(KERN_INFO "Exiting ADXL345 driver!!!\n");
    i2c_del_driver(&adxl345_driver);
}

module_init(adxl345_init);
module_exit(adxl345_exit);

MODULE_AUTHOR("Syaoran");
MODULE_DESCRIPTION("ADXL345 I2C Client Driver");
MODULE_LICENSE("GPL");

