/*
 * DS3231 RTC Driver Implementation
 * I2C Real-Time Clock with Temperature Compensated Crystal Oscillator
 */

#include "ds3231.h"
#include <zephyr/logging/log.h>
#include <errno.h>

LOG_MODULE_REGISTER(ds3231, LOG_LEVEL_DBG);

/* Helper function to convert decimal to BCD */
static uint8_t dec_to_bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

/* Helper function to convert BCD to decimal */
static uint8_t bcd_to_dec(uint8_t val)
{
    return ((val >> 4) * 10) + (val & 0x0F);
}

/* Write a single register */
static int ds3231_write_reg(struct ds3231_dev *dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_write(dev->i2c_dev, buf, sizeof(buf), dev->i2c_addr);
}

/* Read a single register */
static int ds3231_read_reg(struct ds3231_dev *dev, uint8_t reg, uint8_t *val)
{
    return i2c_write_read(dev->i2c_dev, dev->i2c_addr, &reg, 1, val, 1);
}

/* Read multiple registers */
static int ds3231_read_regs(struct ds3231_dev *dev, uint8_t reg, uint8_t *buf, uint8_t len)
{
    return i2c_write_read(dev->i2c_dev, dev->i2c_addr, &reg, 1, buf, len);
}

struct ds3231_dev *ds3231_init(const char *i2c_label)
{
    struct ds3231_dev *dev;
    uint8_t ctrl_reg;
    int ret;

    (void)i2c_label;  /* Unused - kept for API compatibility */

    dev = k_malloc(sizeof(struct ds3231_dev));
    if (!dev) {
        LOG_ERR("Failed to allocate memory for DS3231 device");
        return NULL;
    }

    /* Use devicetree API for SDK 2.x */
    dev->i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(dev->i2c_dev)) {
        LOG_ERR("I2C device not ready");
        k_free(dev);
        return NULL;
    }

    dev->i2c_addr = DS3231_I2C_ADDR;

    /* Enable oscillator and set to 24-hour mode */
    ret = ds3231_read_reg(dev, DS3231_REG_CONTROL, &ctrl_reg);
    if (ret < 0) {
        LOG_ERR("Failed to read control register: %d", ret);
        k_free(dev);
        return NULL;
    }

    /* Clear EOSC bit to enable oscillator */
    ctrl_reg &= ~DS3231_CTRL_EOSC;
    ret = ds3231_write_reg(dev, DS3231_REG_CONTROL, ctrl_reg);
    if (ret < 0) {
        LOG_ERR("Failed to write control register: %d", ret);
        k_free(dev);
        return NULL;
    }

    LOG_INF("DS3231 initialized successfully");
    return dev;
}

int ds3231_set_datetime(struct ds3231_dev *dev, const struct tm *tm)
{
    uint8_t buf[8];
    int ret;

    if (!dev || !tm) {
        return -EINVAL;
    }

    /* Prepare time/date data in BCD format */
    buf[0] = DS3231_REG_SECONDS;
    buf[1] = dec_to_bcd(tm->tm_sec);
    buf[2] = dec_to_bcd(tm->tm_min);
    buf[3] = dec_to_bcd(tm->tm_hour);  /* 24-hour format */
    buf[4] = dec_to_bcd(tm->tm_wday + 1);  /* Day of week: 1-7 */
    buf[5] = dec_to_bcd(tm->tm_mday);
    buf[6] = dec_to_bcd(tm->tm_mon + 1);  /* Month: 1-12 */
    buf[7] = dec_to_bcd(tm->tm_year % 100);  /* Year: 00-99 */

    /* Write all time/date registers in one transaction */
    ret = i2c_write(dev->i2c_dev, buf, sizeof(buf), dev->i2c_addr);
    if (ret < 0) {
        LOG_ERR("Failed to set datetime: %d", ret);
        return ret;
    }

    LOG_INF("DateTime set: %04d-%02d-%02d %02d:%02d:%02d",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec);

    return 0;
}

int ds3231_get_datetime(struct ds3231_dev *dev, struct tm *tm)
{
    uint8_t buf[7];
    int ret;

    if (!dev || !tm) {
        return -EINVAL;
    }

    /* Read all time/date registers */
    ret = ds3231_read_regs(dev, DS3231_REG_SECONDS, buf, sizeof(buf));
    if (ret < 0) {
        LOG_ERR("Failed to read datetime: %d", ret);
        return ret;
    }

    /* Convert BCD to decimal and populate tm structure */
    tm->tm_sec = bcd_to_dec(buf[0] & 0x7F);
    tm->tm_min = bcd_to_dec(buf[1] & 0x7F);
    tm->tm_hour = bcd_to_dec(buf[2] & 0x3F);  /* 24-hour format */
    tm->tm_wday = bcd_to_dec(buf[3] & 0x07) - 1;  /* Day of week: 0-6 */
    tm->tm_mday = bcd_to_dec(buf[4] & 0x3F);
    tm->tm_mon = bcd_to_dec(buf[5] & 0x1F) - 1;  /* Month: 0-11 */
    tm->tm_year = bcd_to_dec(buf[6]) + 100;  /* Years since 1900 */

    /* Calculate day of year (not stored in DS3231) */
    tm->tm_yday = 0;
    tm->tm_isdst = -1;

    LOG_DBG("DateTime read: %04d-%02d-%02d %02d:%02d:%02d",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec);

    return 0;
}

int ds3231_get_temperature(struct ds3231_dev *dev, float *temp_c)
{
    uint8_t buf[2];
    int ret;
    int16_t temp_raw;

    if (!dev || !temp_c) {
        return -EINVAL;
    }

    /* Read temperature registers */
    ret = ds3231_read_regs(dev, DS3231_REG_TEMP_MSB, buf, sizeof(buf));
    if (ret < 0) {
        LOG_ERR("Failed to read temperature: %d", ret);
        return ret;
    }

    /* Temperature is 10-bit: MSB (8 bits) + 2 bits from LSB */
    temp_raw = (int16_t)((buf[0] << 8) | buf[1]) >> 6;
    
    /* Convert to Celsius (resolution: 0.25°C) */
    *temp_c = temp_raw * 0.25f;

    LOG_DBG("Temperature: %.2f°C", (double)*temp_c);

    return 0;
}

int ds3231_check_oscillator(struct ds3231_dev *dev, bool *stopped)
{
    uint8_t status;
    int ret;

    if (!dev || !stopped) {
        return -EINVAL;
    }

    ret = ds3231_read_reg(dev, DS3231_REG_STATUS, &status);
    if (ret < 0) {
        LOG_ERR("Failed to read status register: %d", ret);
        return ret;
    }

    *stopped = (status & DS3231_STATUS_OSF) != 0;

    if (*stopped) {
        LOG_WRN("Oscillator has stopped - time may be invalid");
        
        /* Clear OSF flag */
        status &= ~DS3231_STATUS_OSF;
        ret = ds3231_write_reg(dev, DS3231_REG_STATUS, status);
        if (ret < 0) {
            LOG_ERR("Failed to clear OSF flag: %d", ret);
            return ret;
        }
    }

    return 0;
}
