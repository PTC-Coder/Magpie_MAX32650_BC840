/*
 * DS3231 RTC Driver Usage Example
 * 
 * This file demonstrates how to use the DS3231 driver from main.c
 */

#include "ds3231.h"
#include <zephyr/logging/log.h>
#include <time.h>

LOG_MODULE_REGISTER(ds3231_example, LOG_LEVEL_INF);

/* Example 1: Initialize and set the current date/time */
void example_set_datetime(void)
{
    struct ds3231_dev *rtc;
    struct tm datetime;
    int ret;

    /* Initialize DS3231 on I2C_0 (adjust label as needed) */
    rtc = ds3231_init("I2C_0");
    if (!rtc) {
        LOG_ERR("Failed to initialize DS3231");
        return;
    }

    /* Set date/time: 2026-01-18 14:30:00 Saturday */
    datetime.tm_year = 2026 - 1900;  /* Years since 1900 */
    datetime.tm_mon = 0;              /* January (0-11) */
    datetime.tm_mday = 18;            /* Day of month (1-31) */
    datetime.tm_hour = 14;            /* Hour (0-23) */
    datetime.tm_min = 30;             /* Minute (0-59) */
    datetime.tm_sec = 0;              /* Second (0-59) */
    datetime.tm_wday = 6;             /* Saturday (0=Sunday, 6=Saturday) */

    ret = ds3231_set_datetime(rtc, &datetime);
    if (ret < 0) {
        LOG_ERR("Failed to set datetime: %d", ret);
        return;
    }

    LOG_INF("Date/time set successfully");
}

/* Example 2: Read the current date/time */
void example_get_datetime(void)
{
    struct ds3231_dev *rtc;
    struct tm datetime;
    int ret;
    bool osc_stopped;

    /* Initialize DS3231 */
    rtc = ds3231_init("I2C_0");
    if (!rtc) {
        LOG_ERR("Failed to initialize DS3231");
        return;
    }

    /* Check if oscillator has stopped (power loss) */
    ret = ds3231_check_oscillator(rtc, &osc_stopped);
    if (ret < 0) {
        LOG_ERR("Failed to check oscillator: %d", ret);
        return;
    }

    if (osc_stopped) {
        LOG_WRN("Oscillator was stopped - time may be invalid!");
    }

    /* Read current date/time */
    ret = ds3231_get_datetime(rtc, &datetime);
    if (ret < 0) {
        LOG_ERR("Failed to get datetime: %d", ret);
        return;
    }

    /* Display the date/time */
    LOG_INF("Current Date/Time: %04d-%02d-%02d %02d:%02d:%02d",
            datetime.tm_year + 1900,
            datetime.tm_mon + 1,
            datetime.tm_mday,
            datetime.tm_hour,
            datetime.tm_min,
            datetime.tm_sec);
}

/* Example 3: Read temperature from DS3231 */
void example_get_temperature(void)
{
    struct ds3231_dev *rtc;
    float temperature;
    int ret;

    /* Initialize DS3231 */
    rtc = ds3231_init("I2C_0");
    if (!rtc) {
        LOG_ERR("Failed to initialize DS3231");
        return;
    }

    /* Read temperature */
    ret = ds3231_get_temperature(rtc, &temperature);
    if (ret < 0) {
        LOG_ERR("Failed to get temperature: %d", ret);
        return;
    }

    LOG_INF("Temperature: %.2f°C", (double)temperature);
}

/* Example 4: Complete usage in main loop */
void example_main_loop(void)
{
    struct ds3231_dev *rtc;
    struct tm datetime;
    float temperature;
    int ret;

    /* Initialize DS3231 once at startup */
    rtc = ds3231_init("I2C_0");
    if (!rtc) {
        LOG_ERR("Failed to initialize DS3231");
        return;
    }

    /* Set initial time (do this once) */
    datetime.tm_year = 2026 - 1900;
    datetime.tm_mon = 0;
    datetime.tm_mday = 18;
    datetime.tm_hour = 14;
    datetime.tm_min = 30;
    datetime.tm_sec = 0;
    datetime.tm_wday = 6;

    ret = ds3231_set_datetime(rtc, &datetime);
    if (ret < 0) {
        LOG_ERR("Failed to set datetime");
        return;
    }

    /* Main loop - read time and temperature periodically */
    while (1) {
        /* Read current time */
        ret = ds3231_get_datetime(rtc, &datetime);
        if (ret == 0) {
            LOG_INF("Time: %02d:%02d:%02d",
                    datetime.tm_hour,
                    datetime.tm_min,
                    datetime.tm_sec);
        }

        /* Read temperature */
        ret = ds3231_get_temperature(rtc, &temperature);
        if (ret == 0) {
            LOG_INF("Temp: %.2f°C", (double)temperature);
        }

        /* Wait 10 seconds */
        k_msleep(10000);
    }
}
