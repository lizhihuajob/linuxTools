#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/sysfs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SoloCoder");
MODULE_DESCRIPTION("GPIO Character Device Driver with SysFS Interface");
MODULE_VERSION("1.0");

static int gpio_pin = -1;
module_param(gpio_pin, int, S_IRUGO);
MODULE_PARM_DESC(gpio_pin, "GPIO pin number to monitor");

static dev_t dev_num;
static struct cdev gpio_cdev;
static struct class *gpio_class = NULL;
static struct device *gpio_device = NULL;

static int gpio_open(struct inode *inode, struct file *file)
{
    if (gpio_pin < 0) {
        printk(KERN_ERR "gpio_driver: GPIO pin not specified\n");
        return -EINVAL;
    }
    
    if (!gpio_is_valid(gpio_pin)) {
        printk(KERN_ERR "gpio_driver: Invalid GPIO pin %d\n", gpio_pin);
        return -EINVAL;
    }
    
    return 0;
}

static ssize_t gpio_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    int value;
    char value_str[3];
    int len;
    
    if (*ppos > 0)
        return 0;
    
    value = gpio_get_value(gpio_pin);
    len = snprintf(value_str, sizeof(value_str), "%d\n", value);
    
    if (copy_to_user(buf, value_str, len))
        return -EFAULT;
    
    *ppos = len;
    return len;
}

static const struct file_operations gpio_fops = {
    .owner = THIS_MODULE,
    .open = gpio_open,
    .read = gpio_read,
};

static ssize_t gpio_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int value = gpio_get_value(gpio_pin);
    return sprintf(buf, "%d\n", value);
}

static DEVICE_ATTR(value, S_IRUGO, gpio_value_show, NULL);

static int __init gpio_driver_init(void)
{
    int ret;
    
    if (gpio_pin < 0) {
        printk(KERN_ERR "gpio_driver: GPIO pin must be specified with gpio_pin parameter\n");
        return -EINVAL;
    }
    
    if (!gpio_is_valid(gpio_pin)) {
        printk(KERN_ERR "gpio_driver: Invalid GPIO pin %d\n", gpio_pin);
        return -EINVAL;
    }
    
    ret = gpio_request(gpio_pin, "gpio_driver");
    if (ret) {
        printk(KERN_ERR "gpio_driver: Failed to request GPIO %d\n", gpio_pin);
        return ret;
    }
    
    ret = gpio_direction_input(gpio_pin);
    if (ret) {
        printk(KERN_ERR "gpio_driver: Failed to set GPIO %d as input\n", gpio_pin);
        gpio_free(gpio_pin);
        return ret;
    }
    
    ret = alloc_chrdev_region(&dev_num, 0, 1, "gpio_driver");
    if (ret) {
        printk(KERN_ERR "gpio_driver: Failed to allocate character device region\n");
        gpio_free(gpio_pin);
        return ret;
    }
    
    cdev_init(&gpio_cdev, &gpio_fops);
    gpio_cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&gpio_cdev, dev_num, 1);
    if (ret) {
        printk(KERN_ERR "gpio_driver: Failed to add character device\n");
        unregister_chrdev_region(dev_num, 1);
        gpio_free(gpio_pin);
        return ret;
    }
    
    gpio_class = class_create(THIS_MODULE, "gpio_driver");
    if (IS_ERR(gpio_class)) {
        printk(KERN_ERR "gpio_driver: Failed to create device class\n");
        cdev_del(&gpio_cdev);
        unregister_chrdev_region(dev_num, 1);
        gpio_free(gpio_pin);
        return PTR_ERR(gpio_class);
    }
    
    gpio_device = device_create(gpio_class, NULL, dev_num, NULL, "gpio%d", gpio_pin);
    if (IS_ERR(gpio_device)) {
        printk(KERN_ERR "gpio_driver: Failed to create device\n");
        class_destroy(gpio_class);
        cdev_del(&gpio_cdev);
        unregister_chrdev_region(dev_num, 1);
        gpio_free(gpio_pin);
        return PTR_ERR(gpio_device);
    }
    
    ret = device_create_file(gpio_device, &dev_attr_value);
    if (ret) {
        printk(KERN_ERR "gpio_driver: Failed to create sysfs attribute\n");
        device_destroy(gpio_class, dev_num);
        class_destroy(gpio_class);
        cdev_del(&gpio_cdev);
        unregister_chrdev_region(dev_num, 1);
        gpio_free(gpio_pin);
        return ret;
    }
    
    printk(KERN_INFO "gpio_driver: Module loaded successfully for GPIO pin %d\n", gpio_pin);
    printk(KERN_INFO "gpio_driver: Device node: /dev/gpio%d\n", gpio_pin);
    printk(KERN_INFO "gpio_driver: Sysfs interface: /sys/class/gpio_driver/gpio%d/value\n", gpio_pin);
    
    return 0;
}

static void __exit gpio_driver_exit(void)
{
    device_remove_file(gpio_device, &dev_attr_value);
    device_destroy(gpio_class, dev_num);
    class_destroy(gpio_class);
    cdev_del(&gpio_cdev);
    unregister_chrdev_region(dev_num, 1);
    gpio_free(gpio_pin);
    
    printk(KERN_INFO "gpio_driver: Module unloaded\n");
}

module_init(gpio_driver_init);
module_exit(gpio_driver_exit);