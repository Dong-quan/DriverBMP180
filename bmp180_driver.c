#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/math64.h>
#include <linux/of.h>


#define DRIVER_NAME "bmp180_driver"
#define CLASS_NAME "bmp180_class"
#define DEVICE_NAME "bmp180"

#define BMP180_REG_CONTROL     0xF4
#define BMP180_REG_RESULT      0xF6
#define BMP180_REG_CAL_AC1     0xAA
#define BMP180_REG_CAL_AC2     0xAC
#define BMP180_REG_CAL_AC3     0xAE
#define BMP180_REG_CAL_AC4     0xB0
#define BMP180_REG_CAL_AC5     0xB2
#define BMP180_REG_CAL_AC6     0xB4
#define BMP180_REG_CAL_B1      0xB6
#define BMP180_REG_CAL_B2      0xB8
#define BMP180_REG_CAL_MB      0xBA
#define BMP180_REG_CAL_MC      0xBC
#define BMP180_REG_CAL_MD      0xBE
#define BMP180_REG_TEMPERATURE 0x2E
#define BMP180_REG_PRESSURE    0x34

#define IOCTL_MAGIC           'b'
#define IOCTL_READ_TEMP       _IOR(IOCTL_MAGIC, 0, long)
#define IOCTL_READ_PRESSURE   _IOWR(IOCTL_MAGIC, 1, struct bmp180_pressure_data)
#define IOCTL_READ_ALTITUDE   _IOR(IOCTL_MAGIC, 2, long)
#define IOCTL_GET_TEMP_LEVEL  _IOR(IOCTL_MAGIC, 3, int)

struct bmp180_pressure_data {
    uint8_t oss;
    long pressure;
};

static struct i2c_client *bmp180_client;
static struct class *bmp180_class;
static struct device *bmp180_device;

static int major_number;
static DEFINE_MUTEX(bmp180_mutex);

static short AC1, AC2, AC3, B1, B2, MB, MC, MD;
static unsigned short AC4, AC5, AC6;
static long B5;

static s16 bmp180_read_s16(u8 reg)
{
    s16 msb = i2c_smbus_read_byte_data(bmp180_client, reg);
    s16 lsb = i2c_smbus_read_byte_data(bmp180_client, reg + 1);
    return (msb << 8) | lsb;
}
static u16 bmp180_read_u16(u8 reg)
{
    u16 msb = i2c_smbus_read_byte_data(bmp180_client, reg);
    u16 lsb = i2c_smbus_read_byte_data(bmp180_client, reg + 1);
    return (msb << 8) | lsb;
}

static long read_unTemperature(void)
{
    i2c_smbus_write_byte_data(bmp180_client, BMP180_REG_CONTROL, BMP180_REG_TEMPERATURE);
    msleep(5);
    return bmp180_read_s16(BMP180_REG_RESULT);
}

static long read_unPressure(uint8_t oss)
{
    int wait_ms;
    u8 msb, lsb, xlsb;
    long UP;

    i2c_smbus_write_byte_data(bmp180_client, BMP180_REG_CONTROL,
        BMP180_REG_PRESSURE + (oss << 6));

    switch (oss) {
    case 0: wait_ms = 5;  break;
    case 1: wait_ms = 8;  break;
    case 2: wait_ms = 14; break;
    case 3: wait_ms = 26; break;
    default: return -EINVAL;
    }
    msleep(wait_ms);

    msb = i2c_smbus_read_byte_data(bmp180_client, BMP180_REG_RESULT);
    lsb = i2c_smbus_read_byte_data(bmp180_client, BMP180_REG_RESULT + 1);
    xlsb = i2c_smbus_read_byte_data(bmp180_client, BMP180_REG_RESULT + 2);

    UP = ((msb << 16) | (lsb << 8) | xlsb) >> (8 - oss);
    return UP;
}

static long readTemperature(void)
{
    long UT = read_unTemperature();
    long X1 = ((UT - (long)AC6) * AC5) >> 15;
    long X2 = ((long)MC << 11) / (X1 + MD);
    B5 = X1 + X2;
    return (B5 + 8) >> 4; // 0.1°C
}

static long readPressure(uint8_t oss)
{
    long UP, B6, X1, X2, X3, B3, p, result;
    unsigned long B4, B7;

    UP = read_unPressure(oss);
    B6 = B5 - 4000;
    X1 = (B2 * ((B6 * B6) >> 12)) >> 11;
    X2 = (AC2 * B6) >> 11;
    X3 = X1 + X2;
    B3 = ((((long)AC1 * 4 + X3) << oss) + 2) >> 4;

    X1 = (AC3 * B6) >> 13;
    B4 = (AC4 * (unsigned long)(X3 + 32768)) >> 15;
    B7 = ((unsigned long)UP - B3) * (50000 >> oss);

    if (B7 < 0x80000000)
        p = (B7 << 1) / B4;
    else
        p = (B7 / B4) << 1;

    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    result = p + ((X1 + X2 + 3791) >> 4);

    return result;
}

static long calculateAltitude(long pressure)
{
    // Đơn giản và không cần pow/log/double
    return (44330 * (101325 - pressure)) / 101325;
}




static int classifyTemperature(long temp_decicelsius)
{
    int temp = temp_decicelsius / 10;
    if (temp < 20) return 0; // Cold
    else if (temp < 30) return 1; // Warm
    else return 2; // Hot
}

static long bmp180_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long temp, pressure, altitude;
    struct bmp180_pressure_data data;
    int temp_level;

    switch (cmd) {
    case IOCTL_READ_TEMP:
        mutex_lock(&bmp180_mutex);
        temp = readTemperature();
        mutex_unlock(&bmp180_mutex);
        if (copy_to_user((long __user *)arg, &temp, sizeof(temp)))
            return -EFAULT;
        break;

    case IOCTL_READ_PRESSURE:
        if (copy_from_user(&data, (struct bmp180_pressure_data __user *)arg, sizeof(data)))
            return -EFAULT;
        if (data.oss > 3) return -EINVAL;
        mutex_lock(&bmp180_mutex);
        readTemperature();
        data.pressure = readPressure(data.oss);
        mutex_unlock(&bmp180_mutex);
        if (copy_to_user((struct bmp180_pressure_data __user *)arg, &data, sizeof(data)))
            return -EFAULT;
        break;

    case IOCTL_READ_ALTITUDE:
        mutex_lock(&bmp180_mutex);
        readTemperature();
        pressure = readPressure(0);
        altitude = calculateAltitude(pressure);
        mutex_unlock(&bmp180_mutex);
        if (copy_to_user((long __user *)arg, &altitude, sizeof(altitude)))
            return -EFAULT;
        break;

    case IOCTL_GET_TEMP_LEVEL:
        mutex_lock(&bmp180_mutex);
        temp = readTemperature();
        temp_level = classifyTemperature(temp);
        mutex_unlock(&bmp180_mutex);
        if (copy_to_user((int __user *)arg, &temp_level, sizeof(temp_level)))
            return -EFAULT;
        break;

    default:
        return -EINVAL;
    }
    return 0;
}

static struct file_operations bmp180_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = bmp180_ioctl,
};

static int bmp180_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    bmp180_client = client;

    major_number = register_chrdev(0, DEVICE_NAME, &bmp180_fops);
    if (major_number < 0)
        return major_number;

    bmp180_class = class_create(THIS_MODULE, CLASS_NAME);
    bmp180_device = device_create(bmp180_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);

    AC1 = bmp180_read_s16(BMP180_REG_CAL_AC1);
    AC2 = bmp180_read_s16(BMP180_REG_CAL_AC2);
    AC3 = bmp180_read_s16(BMP180_REG_CAL_AC3);
    AC4 = bmp180_read_u16(BMP180_REG_CAL_AC4);
    AC5 = bmp180_read_u16(BMP180_REG_CAL_AC5);
    AC6 = bmp180_read_u16(BMP180_REG_CAL_AC6);
    B1  = bmp180_read_s16(BMP180_REG_CAL_B1);
    B2  = bmp180_read_s16(BMP180_REG_CAL_B2);
    MB  = bmp180_read_s16(BMP180_REG_CAL_MB);
    MC  = bmp180_read_s16(BMP180_REG_CAL_MC);
    MD  = bmp180_read_s16(BMP180_REG_CAL_MD);

    return 0;
}

static void bmp180_remove(struct i2c_client *client)
{
    device_destroy(bmp180_class, MKDEV(major_number, 0));
    class_destroy(bmp180_class);
    unregister_chrdev(major_number, DEVICE_NAME);
}

static const struct i2c_device_id bmp180_id[] = {
    { "bmp180", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, bmp180_id);

static const struct of_device_id bmp180_of_match[] = {
    { .compatible = "thiennhan,bmp180" },
    { }
};
MODULE_DEVICE_TABLE(of, bmp180_of_match);

static struct i2c_driver bmp180_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = bmp180_of_match,
    },
    .probe    = bmp180_probe,
    .remove   = bmp180_remove,
    .id_table = bmp180_id,
};

module_i2c_driver(bmp180_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("thiennhan");
MODULE_DESCRIPTION("BMP180 I2C Driver with Altitude and Temperature Classification");
MODULE_VERSION("1.1");
