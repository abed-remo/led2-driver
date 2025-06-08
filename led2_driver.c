#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "led2"
#define CLASS_NAME "led2_class"
#define BUF_SIZE 16

static dev_t dev_num;
static struct cdev led_cdev;
static struct class *led_class;
static struct device *led_device;
static int led_gpio;
static unsigned char buffer[BUF_SIZE] = "";

// Write-Funktion
static ssize_t led_write(struct file *file, const char __user *user_buff, size_t count, loff_t *offs)
{
    ssize_t to_copy, not_copied;

    if (*offs >= BUF_SIZE)
        return -ENOSPC;

    to_copy = min(count, (size_t)(BUF_SIZE - *offs));
    if (to_copy == 0)
        return -ENOSPC;

    not_copied = copy_from_user(&buffer[*offs], user_buff, to_copy);
    if (not_copied)
        return -EFAULT;

    switch (buffer[0]) {
        case '1':
            gpio_set_value(led_gpio, 1);
            break;
        case '0':
            gpio_set_value(led_gpio, 0);
            break;
        default:
            break;
    }

    *offs += to_copy;
    return to_copy;
}

static ssize_t led_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offs)
{
    ssize_t to_copy, not_copied;

    if (*offs >= BUF_SIZE)
        return 0;

    to_copy = min(count, (size_t)(BUF_SIZE - *offs));
    not_copied = copy_to_user(user_buffer, &buffer[*offs], to_copy);
    if (not_copied)
        return -EFAULT;

    *offs += to_copy;
    return to_copy;
}

static int led_open(struct inode *inode, struct file *file) {
    return 0;
}

static int led_close(struct inode *inode, struct file *file) {
    return 0;
}

static const struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .release = led_close,
    .read = led_read,
    .write = led_write,
};

// Platform Driver probe
static int led_probe(struct platform_device *pdev)
{
    struct device_node *node = pdev->dev.of_node;
    int ret;

    led_gpio = of_get_named_gpio(node, "led-gpio", 0);
    if (!gpio_is_valid(led_gpio)) {
        dev_err(&pdev->dev, "Invalid GPIO\n");
        return -EINVAL;
    }

    ret = gpio_request(led_gpio, "led2");
    if (ret) {
        dev_err(&pdev->dev, "Failed to request GPIO\n");
        return ret;
    }

    ret = gpio_direction_output(led_gpio, 0);
    if (ret) {
        gpio_free(led_gpio);
        return ret;
    }

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret)
        return ret;

    cdev_init(&led_cdev, &led_fops);
    ret = cdev_add(&led_cdev, dev_num, 1);
    if (ret)
        goto unregister_chrdev;

    led_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(led_class)) {
        ret = PTR_ERR(led_class);
        goto del_cdev;
    }

    led_device = device_create(led_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(led_device)) {
        ret = PTR_ERR(led_device);
        goto destroy_class;
    }

    dev_info(&pdev->dev, "led2-driver loaded.\n");
    return 0;

destroy_class:
    class_destroy(led_class);
del_cdev:
    cdev_del(&led_cdev);
unregister_chrdev:
    unregister_chrdev_region(dev_num, 1);
    gpio_free(led_gpio);
    return ret;
}

// Remove-Funktion
static int led_remove(struct platform_device *pdev)
{
    gpio_set_value(led_gpio, 0);
    gpio_free(led_gpio);
    device_destroy(led_class, dev_num);
    class_destroy(led_class);
    cdev_del(&led_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("led2-driver unloaded.\n");
    return 0;
}

// DeviceTree Match Table
static const struct of_device_id led_of_match[] = {
    { .compatible = "remo,led2-driver" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, led_of_match);

// Platform Driver Registrierung
static struct platform_driver led_driver = {
    .driver = {
        .name = "led2_driver",
        .of_match_table = led_of_match,
    },
    .probe = led_probe,
    .remove = led_remove,
};

module_platform_driver(led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Remo");
MODULE_DESCRIPTION("Platform LED2 Driver");
