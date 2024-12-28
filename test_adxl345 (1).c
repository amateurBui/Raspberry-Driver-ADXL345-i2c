#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DEVICE_PATH "/dev/adxl345"
#define ADXL345_IOCTL_MAGIC 'a'
#define ADXL345_IOCTL_READ_X _IOR(ADXL345_IOCTL_MAGIC, 1, int)
#define ADXL345_IOCTL_READ_Y _IOR(ADXL345_IOCTL_MAGIC, 2, int)
#define ADXL345_IOCTL_READ_Z _IOR(ADXL345_IOCTL_MAGIC, 3, int)

int main() {
    int fd;
    int data;

    // Open the device
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open the device");
        return errno;
    }

    while (1) {
        // Read X-axis data
        if (ioctl(fd, ADXL345_IOCTL_READ_X, &data) < 0) {
            perror("Failed to read X-axis data");
            close(fd);
            return errno;
        }
        printf("X-axis: %d\n", data);

        // Read Y-axis data
        if (ioctl(fd, ADXL345_IOCTL_READ_Y, &data) < 0) {
            perror("Failed to read Y-axis data");
            close(fd);
            return errno;
        }
        printf("Y-axis: %d\n", data);

        // Read Z-axis data
        if (ioctl(fd, ADXL345_IOCTL_READ_Z, &data) < 0) {
            perror("Failed to read Z-axis data");
            close(fd);
            return errno;
        }
        printf("Z-axis: %d\n", data);

        // Sleep for a short duration before reading again (e.g., 1 second)
        sleep(1);
    }

    // Close the device (although it's never reached in this loop)
    close(fd);
    return 0;
}
