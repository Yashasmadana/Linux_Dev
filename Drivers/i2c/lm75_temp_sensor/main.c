/*
 ******************************************************************************
 * Linux I2C Temperature Sensor Driver
 *
 * Description: I2C driver for LM75/LM75A temperature sensor
 *              Demonstrates Linux kernel I2C driver development
 *
 * Compatible with: LM75, LM75A, DS75, TMP75, etc.
 * Tested on: Raspberry Pi 4, BeagleBone Black
 *
 * Author: Your Name
 * License: GPL v2
 * Date: 2025
 ******************************************************************************
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

// ============================================================================
// DRIVER INFORMATION
// ============================================================================

#define DRIVER_NAME "lm75_temp_sensor"
#define DRIVER_VERSION "1.0"
#define DRIVER_AUTHOR "Your Name <your.email@example.com>"
#define DRIVER_DESC "Linux I2C driver for LM75 temperature sensor"

// ============================================================================
// DEVICE REGISTERS
// ============================================================================

#define LM75_REG_TEMP 0x00   // Temperature register (read-only)
#define LM75_REG_CONFIG 0x01 // Configuration register
#define LM75_REG_THYST 0x02  // Hysteresis register
#define LM75_REG_TOS 0x03    // Over-temperature shutdown register

// Configuration bits
#define LM75_SHUTDOWN (1 << 0)
#define LM75_OS_COMP_INT (1 << 1)
#define LM75_OS_POL (1 << 2)
#define LM75_OS_FAULT_QUEUE (3 << 3)

// ============================================================================
// DEVICE STRUCTURE
// ============================================================================

struct lm75_data
{
    struct i2c_client *client; // I2C client
    struct device *dev;        // Device structure
    struct cdev cdev;          // Character device
    dev_t dev_num;             // Device number
    struct class *class;       // Device class

    int16_t temperature; // Current temperature (in 0.125°C steps)
    u8 config;           // Configuration register value

    struct mutex lock; // Mutex for thread safety
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * lm75_read_temp - Read temperature from sensor
 * @data: Pointer to device data structure
 *
 * Returns: Temperature in millidegrees Celsius, or negative error code
 */
static int lm75_read_temp(struct lm75_data *data)
{
    struct i2c_client *client = data->client;
    int16_t temp_raw;
    s32 temp;

    // Read 2 bytes from temperature register
    temp = i2c_smbus_read_word_data(client, LM75_REG_TEMP);
    if (temp < 0)
    {
        dev_err(&client->dev, "Failed to read temperature: %d\n", temp);
        return temp;
    }

    // Swap bytes (I2C returns MSB first, but word_data gives LSB first)
    temp_raw = swab16(temp);

    // LM75 temp format: 11-bit, 0.125°C per LSB
    // Right shift by 5 to get actual value
    temp_raw >>= 5;

    // Convert to millidegrees Celsius
    // Each LSB = 0.125°C = 125 milli°C
    return temp_raw * 125;
}

/**
 * lm75_write_config - Write to configuration register
 */
static int lm75_write_config(struct lm75_data *data, u8 config)
{
    struct i2c_client *client = data->client;
    int ret;

    ret = i2c_smbus_write_byte_data(client, LM75_REG_CONFIG, config);
    if (ret < 0)
    {
        dev_err(&client->dev, "Failed to write config: %d\n", ret);
        return ret;
    }

    data->config = config;
    return 0;
}

/**
 * lm75_read_config - Read configuration register
 */
static int lm75_read_config(struct lm75_data *data)
{
    struct i2c_client *client = data->client;
    s32 config;

    config = i2c_smbus_read_byte_data(client, LM75_REG_CONFIG);
    if (config < 0)
    {
        dev_err(&client->dev, "Failed to read config: %d\n", config);
        return config;
    }

    data->config = config;
    return 0;
}

// ============================================================================
// SYSFS ATTRIBUTES
// ============================================================================

/**
 * temperature_show - Show temperature via sysfs
 * Path: /sys/class/lm75_temp_sensor/lm75/temperature
 */
static ssize_t temperature_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
    struct lm75_data *data = dev_get_drvdata(dev);
    int temp;

    mutex_lock(&data->lock);
    temp = lm75_read_temp(data);
    mutex_unlock(&data->lock);

    if (temp < 0)
        return temp;

    // Return temperature in format: XX.XXX (degrees Celsius)
    return sprintf(buf, "%d.%03d\n", temp / 1000, abs(temp % 1000));
}

/**
 * config_show - Show configuration register
 */
static ssize_t config_show(struct device *dev,
                           struct device_attribute *attr,
                           char *buf)
{
    struct lm75_data *data = dev_get_drvdata(dev);
    int ret;

    mutex_lock(&data->lock);
    ret = lm75_read_config(data);
    mutex_unlock(&data->lock);

    if (ret < 0)
        return ret;

    return sprintf(buf, "0x%02x\n", data->config);
}

/**
 * config_store - Write to configuration register
 */
static ssize_t config_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    struct lm75_data *data = dev_get_drvdata(dev);
    unsigned long val;
    int ret;

    ret = kstrtoul(buf, 0, &val);
    if (ret)
        return ret;

    if (val > 0xFF)
        return -EINVAL;

    mutex_lock(&data->lock);
    ret = lm75_write_config(data, (u8)val);
    mutex_unlock(&data->lock);

    if (ret < 0)
        return ret;

    return count;
}

/**
 * shutdown_store - Put device in shutdown mode
 */
static ssize_t shutdown_store(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
    struct lm75_data *data = dev_get_drvdata(dev);
    unsigned long val;
    int ret;
    u8 config;

    ret = kstrtoul(buf, 0, &val);
    if (ret)
        return ret;

    mutex_lock(&data->lock);

    ret = lm75_read_config(data);
    if (ret < 0)
        goto out;

    config = data->config;

    if (val)
        config |= LM75_SHUTDOWN;
    else
        config &= ~LM75_SHUTDOWN;

    ret = lm75_write_config(data, config);
    if (ret < 0)
        goto out;

    ret = count;

out:
    mutex_unlock(&data->lock);
    return ret;
}

// Define device attributes
static DEVICE_ATTR_RO(temperature);
static DEVICE_ATTR_RW(config);
static DEVICE_ATTR_WO(shutdown);

static struct attribute *lm75_attrs[] = {
    &dev_attr_temperature.attr,
    &dev_attr_config.attr,
    &dev_attr_shutdown.attr,
    NULL,
};

ATTRIBUTE_GROUPS(lm75);

// ============================================================================
// CHARACTER DEVICE FILE OPERATIONS
// ============================================================================

/**
 * lm75_open - Open device file
 */
static int lm75_open(struct inode *inode, struct file *file)
{
    struct lm75_data *data;

    data = container_of(inode->i_cdev, struct lm75_data, cdev);
    file->private_data = data;

    dev_info(data->dev, "Device opened\n");

    return 0;
}

/**
 * lm75_release - Close device file
 */
static int lm75_release(struct inode *inode, struct file *file)
{
    struct lm75_data *data = file->private_data;

    dev_info(data->dev, "Device closed\n");

    return 0;
}

/**
 * lm75_read - Read temperature from device file
 * Usage: cat /dev/lm75
 */
static ssize_t lm75_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    struct lm75_data *data = file->private_data;
    char temp_str[32];
    int temp, len;

    if (*ppos > 0)
        return 0; // EOF

    mutex_lock(&data->lock);
    temp = lm75_read_temp(data);
    mutex_unlock(&data->lock);

    if (temp < 0)
        return temp;

    // Format: "Temperature: XX.XXX°C\n"
    len = snprintf(temp_str, sizeof(temp_str),
                   "Temperature: %d.%03d°C\n",
                   temp / 1000, abs(temp % 1000));

    if (count < len)
        len = count;

    if (copy_to_user(buf, temp_str, len))
        return -EFAULT;

    *ppos += len;
    return len;
}

/**
 * lm75_write - Write to device (example: set config)
 */
static ssize_t lm75_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    struct lm75_data *data = file->private_data;
    char cmd[32];
    unsigned long val;
    int ret;

    if (count > sizeof(cmd) - 1)
        return -EINVAL;

    if (copy_from_user(cmd, buf, count))
        return -EFAULT;

    cmd[count] = '\0';

    ret = kstrtoul(cmd, 0, &val);
    if (ret)
        return ret;

    if (val > 0xFF)
        return -EINVAL;

    mutex_lock(&data->lock);
    ret = lm75_write_config(data, (u8)val);
    mutex_unlock(&data->lock);

    if (ret < 0)
        return ret;

    return count;
}

// File operations structure
static const struct file_operations lm75_fops = {
    .owner = THIS_MODULE,
    .open = lm75_open,
    .release = lm75_release,
    .read = lm75_read,
    .write = lm75_write,
};

// ============================================================================
// I2C DRIVER PROBE AND REMOVE
// ============================================================================

/**
 * lm75_probe - Called when device is detected
 */
static int lm75_probe(struct i2c_client *client,
                      const struct i2c_device_id *id)
{
    struct lm75_data *data;
    int ret;

    dev_info(&client->dev, "Probing LM75 temperature sensor\n");

    // Check I2C functionality
    if (!i2c_check_functionality(client->adapter,
                                 I2C_FUNC_SMBUS_BYTE_DATA |
                                     I2C_FUNC_SMBUS_WORD_DATA))
    {
        dev_err(&client->dev, "I2C adapter doesn't support required functionality\n");
        return -ENODEV;
    }

    // Allocate device data
    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->client = client;
    i2c_set_clientdata(client, data);
    mutex_init(&data->lock);

    // Read initial configuration
    ret = lm75_read_config(data);
    if (ret < 0)
    {
        dev_err(&client->dev, "Failed to read initial config\n");
        return ret;
    }

    // Wake up device if in shutdown mode
    if (data->config & LM75_SHUTDOWN)
    {
        ret = lm75_write_config(data, data->config & ~LM75_SHUTDOWN);
        if (ret < 0)
            return ret;
        msleep(1); // Wait for sensor to wake up
    }

    // Allocate character device number
    ret = alloc_chrdev_region(&data->dev_num, 0, 1, DRIVER_NAME);
    if (ret < 0)
    {
        dev_err(&client->dev, "Failed to allocate char dev region\n");
        return ret;
    }

    // Initialize character device
    cdev_init(&data->cdev, &lm75_fops);
    data->cdev.owner = THIS_MODULE;

    ret = cdev_add(&data->cdev, data->dev_num, 1);
    if (ret < 0)
    {
        dev_err(&client->dev, "Failed to add char device\n");
        goto err_cdev;
    }

    // Create device class
    data->class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(data->class))
    {
        ret = PTR_ERR(data->class);
        dev_err(&client->dev, "Failed to create device class\n");
        goto err_class;
    }

    // Add sysfs groups
    data->class->dev_groups = lm75_groups;

    // Create device node
    data->dev = device_create(data->class, &client->dev,
                              data->dev_num, data,
                              DRIVER_NAME);
    if (IS_ERR(data->dev))
    {
        ret = PTR_ERR(data->dev);
        dev_err(&client->dev, "Failed to create device\n");
        goto err_device;
    }

    // Read and display initial temperature
    ret = lm75_read_temp(data);
    if (ret < 0)
    {
        dev_warn(&client->dev, "Failed to read initial temperature\n");
    }
    else
    {
        dev_info(&client->dev, "Initial temperature: %d.%03d°C\n",
                 ret / 1000, abs(ret % 1000));
    }

    dev_info(&client->dev, "LM75 driver probed successfully\n");
    dev_info(&client->dev, "Device node: /dev/%s\n", DRIVER_NAME);
    dev_info(&client->dev, "Sysfs: /sys/class/%s/%s/\n",
             DRIVER_NAME, DRIVER_NAME);

    return 0;

err_device:
    class_destroy(data->class);
err_class:
    cdev_del(&data->cdev);
err_cdev:
    unregister_chrdev_region(data->dev_num, 1);
    return ret;
}

/**
 * lm75_remove - Called when device is removed
 */
static int lm75_remove(struct i2c_client *client)
{
    struct lm75_data *data = i2c_get_clientdata(client);

    dev_info(&client->dev, "Removing LM75 driver\n");

    // Clean up device
    device_destroy(data->class, data->dev_num);
    class_destroy(data->class);
    cdev_del(&data->cdev);
    unregister_chrdev_region(data->dev_num, 1);

    // Put device in shutdown mode to save power
    lm75_write_config(data, data->config | LM75_SHUTDOWN);

    dev_info(&client->dev, "LM75 driver removed\n");

    return 0;
}

// ============================================================================
// DEVICE ID TABLE AND DRIVER STRUCTURE
// ============================================================================

// I2C device ID table
static const struct i2c_device_id lm75_id[] = {
    {"lm75", 0},
    {"lm75a", 0},
    {"ds75", 0},
    {"tmp75", 0},
    {}};
MODULE_DEVICE_TABLE(i2c, lm75_id);

// Device tree compatible table
#ifdef CONFIG_OF
static const struct of_device_id lm75_of_match[] = {
    {.compatible = "nxp,lm75"},
    {.compatible = "nxp,lm75a"},
    {.compatible = "dallas,ds75"},
    {.compatible = "ti,tmp75"},
    {}};
MODULE_DEVICE_TABLE(of, lm75_of_match);
#endif

// I2C driver structure
static struct i2c_driver lm75_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = of_match_ptr(lm75_of_match),
#endif
    },
    .probe = lm75_probe,
    .remove = lm75_remove,
    .id_table = lm75_id,
};

// Register I2C driver
module_i2c_driver(lm75_driver);

// ============================================================================
// MODULE INFORMATION
// ============================================================================

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);

/*
 ******************************************************************************
 * USAGE INSTRUCTIONS
 ******************************************************************************
 *
 * 1. COMPILE:
 *    Create Makefile:
 *    ---
 *    obj-m += lm75_temp_sensor.o
 *
 *    all:
 *        make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
 *
 *    clean:
 *        make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
 *    ---
 *
 *    Then: make
 *
 * 2. LOAD MODULE:
 *    sudo insmod lm75_temp_sensor.ko
 *
 * 3. CHECK IF LOADED:
 *    lsmod | grep lm75
 *    dmesg | tail -20
 *
 * 4. DEVICE TREE OVERLAY (for auto-detection):
 *    Create: lm75-overlay.dts
 *    ---
 *    /dts-v1/;
 *    /plugin/;
 *
 *    / {
 *        compatible = "brcm,bcm2835";
 *
 *        fragment@0 {
 *            target = <&i2c1>;
 *            __overlay__ {
 *                #address-cells = <1>;
 *                #size-cells = <0>;
 *
 *                lm75@48 {
 *                    compatible = "nxp,lm75";
 *                    reg = <0x48>;
 *                };
 *            };
 *        };
 *    };
 *    ---
 *
 *    Compile: dtc -@ -I dts -O dtb -o lm75.dtbo lm75-overlay.dts
 *    Copy: sudo cp lm75.dtbo /boot/overlays/
 *    Add to /boot/config.txt: dtoverlay=lm75
 *
 * 5. MANUAL I2C DEVICE CREATION (without device tree):
 *    echo lm75 0x48 | sudo tee /sys/bus/i2c/devices/i2c-1/new_device
 *
 * 6. READ TEMPERATURE:
 *    Via sysfs:
 *      cat /sys/class/lm75_temp_sensor/lm75_temp_sensor/temperature
 *
 *    Via device file:
 *      cat /dev/lm75_temp_sensor
 *
 * 7. VIEW/MODIFY CONFIG:
 *    cat /sys/class/lm75_temp_sensor/lm75_temp_sensor/config
 *    echo 0x00 | sudo tee /sys/class/lm75_temp_sensor/lm75_temp_sensor/config
 *
 * 8. SHUTDOWN DEVICE:
 *    echo 1 | sudo tee /sys/class/lm75_temp_sensor/lm75_temp_sensor/shutdown
 *
 * 9. UNLOAD MODULE:
 *    sudo rmmod lm75_temp_sensor
 *
 * 10. DEBUGGING:
 *     Enable I2C debugging: echo 1 | sudo tee /sys/module/i2c_core/parameters/debug
 *     View messages: dmesg -w
 *     Check I2C bus: i2cdetect -y 1
 *
 ******************************************************************************
 */