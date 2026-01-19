/*
 * DS3231 RTC Driver Header
 * I2C Real-Time Clock with Temperature Compensated Crystal Oscillator
 */

#ifndef DS3231_H
#define DS3231_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <time.h>

/* DS3231 I2C Address */
#define DS3231_I2C_ADDR 0x68

/* DS3231 Register Addresses */
#define DS3231_REG_SECONDS      0x00
#define DS3231_REG_MINUTES      0x01
#define DS3231_REG_HOURS        0x02
#define DS3231_REG_DAY          0x03
#define DS3231_REG_DATE         0x04
#define DS3231_REG_MONTH        0x05
#define DS3231_REG_YEAR         0x06
#define DS3231_REG_CONTROL      0x0E
#define DS3231_REG_STATUS       0x0F
#define DS3231_REG_TEMP_MSB     0x11

/* Control Register Bits */
#define DS3231_CTRL_EOSC        (1 << 7)
#define DS3231_CTRL_INTCN       (1 << 2)

/* Status Register Bits */
#define DS3231_STATUS_OSF       (1 << 7)

/* DS3231 Device Structure */
struct ds3231_dev {
    const struct device *i2c_dev;
    uint16_t i2c_addr;
};

/**
 * @brief Initialize DS3231 RTC
 * 
 * @param i2c_label I2C device label (e.g., "I2C_0")
 * @return Pointer to ds3231_dev structure, or NULL on failure
 */
struct ds3231_dev *ds3231_init(const char *i2c_label);

/**
 * @brief Set date and time on DS3231
 * 
 * @param dev Pointer to DS3231 device structure
 * @param tm Pointer to tm structure containing date/time
 * @return 0 on success, negative error code on failure
 */
int ds3231_set_datetime(struct ds3231_dev *dev, const struct tm *tm);

/**
 * @brief Get date and time from DS3231
 * 
 * @param dev Pointer to DS3231 device structure
 * @param tm Pointer to tm structure to store date/time
 * @return 0 on success, negative error code on failure
 */
int ds3231_get_datetime(struct ds3231_dev *dev, struct tm *tm);

/**
 * @brief Get temperature from DS3231
 * 
 * @param dev Pointer to DS3231 device structure
 * @param temp_c Pointer to store temperature in Celsius
 * @return 0 on success, negative error code on failure
 */
int ds3231_get_temperature(struct ds3231_dev *dev, float *temp_c);

/**
 * @brief Check if oscillator has stopped
 * 
 * @param dev Pointer to DS3231 device structure
 * @param stopped Pointer to store result (true if stopped)
 * @return 0 on success, negative error code on failure
 */
int ds3231_check_oscillator(struct ds3231_dev *dev, bool *stopped);

#endif /* DS3231_H */
