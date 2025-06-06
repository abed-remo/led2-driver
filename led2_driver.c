#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>  // für copy_from_user / copy_to_user

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Moatasem");
MODULE_DESCRIPTION("LED Character Device Driver");

#define DEVICE_NAME "led_driver"
#define CLASS_NAME  "led_class"
#define LED_PIN 17  // besserer GPIO-Pin auf dem Raspberry Pi 4
#define SIZE 15

static dev_t device_number;
static struct cdev st_characterDevice;
static struct class *led_class;
static struct device *led_device;
static unsigned char buffer[SIZE] = "";

// Write-Funktion
static ssize_t driver_write(struct file *file, const char __user *user_buff, size_t count, loff_t *offs) {
    ssize_t to_copy, not_copied;

    pr_info("%s: write request count=%zu, offs=%lld\n", __func__, count, *offs);

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
            gpio_set_value(LED_PIN, 1);
            pr_info("LED eingeschaltet\n");
            break;
        case '0':
            gpio_set_value(LED_PIN, 0);
            pr_info("LED ausgeschaltet\n");
            break;
        default:
            pr_info("Ungültiger Wert: %c\n", buffer[0]);
            break;
    }

    *offs += to_copy;
    return to_copy;
}

// Read-Funktion
static ssize_t driver_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offs) {
    ssize_t to_copy, not_copied;

    pr_info("%s: read request count=%zu, offs=%lld\n", __func__, count, *offs);

    if (*offs >= SIZE)
        return 0;  // EOF

    to_copy = min(count, (size_t)(SIZE - *offs));
    not_copied = copy_to_user(user_buffer, &buffer[*offs], to_copy);
    if (not_copied)
        return -EFAULT;

    *offs += to_copy;
    return to_copy;
}

// open / release
static int driver_open(struct inode *inode, struct file *file) {
    pr_info("Device file opened\n");
    return 0;
}

static int driver_close(struct inode *inode, struct file *file) {
    pr_info("Device file closed\n");
    return 0;
}

// File-Operations-Tabelle
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .read = driver_read,
    .write = driver_write,
};

// Modul-Initialisierung
static int __init led_driver_init(void) {
    int retval;

    pr_info("Initializing LED driver\n");

    retval = alloc_chrdev_region(&device_number, 0, 1, DEVICE_NAME);
    if (retval != 0) {
        pr_err("Could not allocate device number\n");
        return retval;
    }

    cdev_init(&st_characterDevice, &fops);
    retval = cdev_add(&st_characterDevice, device_number, 1);
    if (retval != 0) {
        pr_err("cdev_add failed\n");
        goto unregister_chrdev;
    }
led_class = class_create(CLASS_NAME);
    if (IS_ERR(led_class)) {
        pr_err("class_create failed\n");
        retval = PTR_ERR(led_class);
        goto del_cdev;
    }

    led_device = device_create(led_class, NULL, device_number, NULL, "led");
    if (IS_ERR(led_device)) {
        pr_err("device_create failed\n");
        retval = PTR_ERR(led_device);
        goto destroy_class;
    }

    retval = gpio_request(LED_PIN, "LED_GPIO");
    if (retval) {
        pr_err("GPIO request failed\n");
        goto destroy_device;
    }

    retval = gpio_direction_output(LED_PIN, 0);
    if (retval) {
        pr_err("GPIO direction set failed\n");
        goto free_gpio;
    }

    pr_info("LED driver wurde erfolgreich geloaded\n");
    return 0;

free_gpio:
    gpio_free(LED_PIN);
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

// Modul-Deinitialisierung
static void __exit led_driver_exit(void) {
    gpio_set_value(LED_PIN, 0);
    gpio_free(LED_PIN);
    device_destroy(led_class, device_number);
    class_destroy(led_class);
    cdev_del(&st_characterDevice);
    unregister_chrdev_region(device_number, 1);
    pr_info("LED driver unloaded\n");
}

module_init(led_driver_init);
module_exit(led_driver_exit);
