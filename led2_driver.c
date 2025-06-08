#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Moatasem");
MODULE_DESCRIPTION("LED Character Device Driver (GPIOD)");

#define DEVICE_NAME "led_driver"
#define CLASS_NAME  "led_class"
#define SIZE 15

static dev_t device_number;
static struct cdev st_characterDevice;
static struct class *led_class;
static struct device *led_device;
static unsigned char buffer[SIZE] = "";

static struct gpio_desc *led_gpio;

// Write-Funktion
static ssize_t driver_write(struct file *file, const char __user *user_buff, size_t count, loff_t *offs) {
    ssize_t to_copy, not_copied;

    if (*offs >= SIZE)
        return -ENOSPC;

    to_copy = min(count, (size_t)(SIZE - *offs));
    if (to_copy == 0)
        return -ENOSPC;

    not_copied = copy_from_user(&buffer[*offs], user_buff, to_copy);
    if (not_copied)
        return -EFAULT;

    // LED steuern nur beim ersten Byte
    switch (buffer[0]) {
        case '1':
            gpiod_set_value(led_gpio, 1);
            break;
        case '0':
            gpiod_set_value(led_gpio, 0);
            break;
        default:
            break;
    }

    *offs += to_copy;
    return to_copy;
}

// Read-Funktion
static ssize_t driver_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offs) {
    ssize_t to_copy, not_copied;

    if (*offs >= SIZE)
        return 0;

    to_copy = min(count, (size_t)(SIZE - *offs));
    not_copied = copy_to_user(user_buffer, &buffer[*offs], to_copy);
    if (not_copied)
        return -EFAULT;

    *offs += to_copy;
    return to_copy;
}

static int driver_open(struct inode *inode, struct file *file) {
    return 0;
}

static int driver_close(struct inode *inode, struct file *file) {
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .read = driver_read,
    .write = driver_write,
};

static int __init led_driver_init(void) {
    int retval;

    retval = alloc_chrdev_region(&device_number, 0, 1, DEVICE_NAME);
    if (retval != 0)
        return retval;

    cdev_init(&st_characterDevice, &fops);
    retval = cdev_add(&st_characterDevice, device_number, 1);
    if (retval != 0)
        goto unregister_chrdev;

    led_class = class_create(CLASS_NAME);
    if (IS_ERR(led_class)) {
        retval = PTR_ERR(led_class);
        goto del_cdev;
    }

    led_device = device_create(led_class, NULL, device_number, NULL, "led");
    if (IS_ERR(led_device)) {
        retval = PTR_ERR(led_device);
        goto destroy_class;
    }

    led_gpio = gpiod_get(NULL, "led", GPIOD_OUT_LOW);
    if (IS_ERR(led_gpio)) {
        retval = PTR_ERR(led_gpio);
        goto destroy_device;
    }

    return 0;

destroy_device:
    device_destroy(led_class, device_number);
destroy_class:
    class_destroy(led_class);
del_cdev:
    cdev_del(&st_characterDevice);
unregister_chrdev:
    unregister_chrdev_region(device_number, 1);
    return retval;
}

static void __exit led_driver_exit(void) {
    gpiod_set_value(led_gpio, 0);
    gpiod_put(led_gpio);
    device_destroy(led_class, device_number);
    class_destroy(led_class);
    cdev_del(&st_characterDevice);
    unregister_chrdev_region(device_number, 1);
}

module_init(led_driver_init);
module_exit(led_driver_exit);
