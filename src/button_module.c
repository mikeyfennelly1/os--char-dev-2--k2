#include <linux/module.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/wait.h>

#define DEVICE_NAME "button_device"
#define GPIO_BUTTON 10  // GPIO pin for button

static int irq_number;
static int button_pressed = 0;
static dev_t dev_num;
static struct cdev button_cdev;
static struct class *button_class;
static DECLARE_WAIT_QUEUE_HEAD(wait_queue);
static int flag = 0;

static irqreturn_t button_irq_handler(int irq, void *dev_id) {
    button_pressed = 1;
    flag = 1;
    wake_up_interruptible(&wait_queue);
    return IRQ_HANDLED;
}

static int button_open(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t button_read(struct file *file, char __user *buffer, size_t len, loff_t *offset) {
    if (wait_event_interruptible(wait_queue, flag != 0))
        return -ERESTARTSYS;

    flag = 0;
    button_pressed = 0;
    return 1;
}

static unsigned int button_poll(struct file *file, struct poll_table_struct *wait) {
    poll_wait(file, &wait_queue, wait);
    if (flag) return POLLIN | POLLRDNORM;
    return 0;
}

static int button_release(struct inode *inode, struct file *file) {
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = button_open,
    .read = button_read,
    .poll = button_poll,
    .release = button_release,
};

static int __init button_init(void) {
    int result;

    // Allocate a device number
    result = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (result < 0) {
        pr_err("Failed to allocate device number\n");
        return result;
    }

    // Initialize character device
    cdev_init(&button_cdev, &fops);
    result = cdev_add(&button_cdev, dev_num, 1);
    if (result < 0) {
        pr_err("Failed to add cdev\n");
        unregister_chrdev_region(dev_num, 1);
        return result;
    }

    // Create device class and node
    button_class = class_create("push_button");
    device_create(button_class, NULL, dev_num, NULL, DEVICE_NAME);

    // Request GPIO
    if (!gpio_is_valid(GPIO_BUTTON)) {
        pr_err("Invalid GPIO\n");
        return -ENODEV;
    }
    gpio_request(GPIO_BUTTON, "gpio_button");
    gpio_direction_input(GPIO_BUTTON);
    irq_number = gpio_to_irq(GPIO_BUTTON);

    // Request IRQ
    result = request_irq(irq_number, button_irq_handler, IRQF_TRIGGER_FALLING, "button_irq", NULL);
    if (result) {
        pr_err("Failed to request IRQ\n");
        return result;
    }

    pr_info("Button driver loaded\n");
    return 0;
}

static void __exit button_exit(void) {
    free_irq(irq_number, NULL);
    gpio_free(GPIO_BUTTON);
    device_destroy(button_class, dev_num);
    class_destroy(button_class);
    cdev_del(&button_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("Button driver unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("GPIO Button Character Device Driver");
MODULE_VERSION("1.0");

module_init(button_init);
module_exit(button_exit);
