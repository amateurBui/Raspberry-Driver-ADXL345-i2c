#ifndef ADXL345_H
#define ADXL345_H

#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/cdev.h>

#define N_I2C_MINORS 15  // Adjust as needed
#define ADXL345_MAJOR 150 // Use an available major number

#define ADXL345_IOC_MAGIC 'a'
#define ADXL345_IOC_SET_RANGE _IOW(ADXL345_IOC_MAGIC, 1, __u8)
#define ADXL345_IOC_MAXNR 1

// Define data structure for ADXL345
struct adxl345_data 
{
    dev_t devt;
    struct i2c_client *client;
    struct list_head device_entry;
    struct cdev cdev;
    unsigned users;
    uint8_t *tx_buffer;
    uint8_t *rx_buffer;
    struct mutex buf_lock;
    struct mutex i2c_lock;  
    uint32_t speed_hz;
    uint32_t mode;
};

// Function prototypes

// Open function
static int adxl345_open(struct inode *inode, struct file *filp);

// Release function
static int adxl345_release(struct inode *inode, struct file *filp);

// Write function
static ssize_t adxl345_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

// IOCTL function
static long adxl345_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

// Send command function
static int adxl345_send_command(struct adxl345_data *adxl345, u8 command);

// Send data function
static int adxl345_send_data(struct adxl345_data *adxl345, const u8 *data, size_t len);

#endif // ADXL345_H

