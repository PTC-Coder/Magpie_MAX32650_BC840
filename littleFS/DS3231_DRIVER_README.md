# DS3231 RTC Driver

A complete driver implementation for the DS3231 Real-Time Clock with Temperature Compensated Crystal Oscillator (TCXO) for Zephyr RTOS.

## Features

- Set and get date/time (seconds, minutes, hours, day, date, month, year)
- Read temperature from integrated sensor (±3°C accuracy)
- Check oscillator status (detect power loss)
- BCD conversion handled automatically
- 24-hour time format
- I2C communication at up to 400kHz

## Hardware Connection

The DS3231 connects via I2C:
- **SDA**: I2C Data line
- **SCL**: I2C Clock line
- **VCC**: 2.3V to 5.5V power supply
- **GND**: Ground
- **VBAT**: Battery backup (optional, 3V coin cell)

I2C Address: **0x68** (7-bit)

## Files

- `src/ds3231.h` - Driver header file
- `src/ds3231.c` - Driver implementation
- `src/ds3231_example.c` - Usage examples

## Configuration

### 1. Enable I2C in prj.conf

Add these lines to your `prj.conf`:

```
# Enable I2C
CONFIG_I2C=y
```

### 2. Configure I2C in Device Tree

Add I2C configuration to your board's `.dts` or `.overlay` file:

```dts
&i2c0 {
    status = "okay";
    compatible = "nordic,nrf-twi";
    sda-pin = <26>;  /* Adjust to your board */
    scl-pin = <27>;  /* Adjust to your board */
    clock-frequency = <I2C_BITRATE_FAST>;  /* 400kHz */
    
    ds3231@68 {
        compatible = "maxim,ds3231";
        reg = <0x68>;
        label = "DS3231";
    };
};
```

### 3. Update CMakeLists.txt

Add the DS3231 driver to your build:

```cmake
target_sources(app PRIVATE
    src/main.c
    src/ds3231.c
    # ... other source files
)
```

## API Reference

### Initialization

```c
struct ds3231_dev *ds3231_init(const char *i2c_label);
```

Initialize the DS3231 RTC. Returns pointer to device structure or NULL on failure.

**Parameters:**
- `i2c_label`: I2C device label (e.g., "I2C_0")

**Example:**
```c
struct ds3231_dev *rtc = ds3231_init("I2C_0");
if (!rtc) {
    LOG_ERR("Failed to initialize DS3231");
}
```

### Set Date/Time

```c
int ds3231_set_datetime(struct ds3231_dev *dev, const struct tm *tm);
```

Set the current date and time on the DS3231.

**Parameters:**
- `dev`: Pointer to DS3231 device structure
- `tm`: Pointer to tm structure containing date/time

**Returns:** 0 on success, negative error code on failure

**Example:**
```c
struct tm datetime;
datetime.tm_year = 2026 - 1900;  /* Years since 1900 */
datetime.tm_mon = 0;              /* January (0-11) */
datetime.tm_mday = 18;            /* Day of month (1-31) */
datetime.tm_hour = 14;            /* Hour (0-23) */
datetime.tm_min = 30;             /* Minute (0-59) */
datetime.tm_sec = 0;              /* Second (0-59) */
datetime.tm_wday = 6;             /* Day of week (0=Sun, 6=Sat) */

int ret = ds3231_set_datetime(rtc, &datetime);
```

### Get Date/Time

```c
int ds3231_get_datetime(struct ds3231_dev *dev, struct tm *tm);
```

Read the current date and time from the DS3231.

**Parameters:**
- `dev`: Pointer to DS3231 device structure
- `tm`: Pointer to tm structure to store date/time

**Returns:** 0 on success, negative error code on failure

**Example:**
```c
struct tm datetime;
int ret = ds3231_get_datetime(rtc, &datetime);
if (ret == 0) {
    LOG_INF("Time: %04d-%02d-%02d %02d:%02d:%02d",
            datetime.tm_year + 1900,
            datetime.tm_mon + 1,
            datetime.tm_mday,
            datetime.tm_hour,
            datetime.tm_min,
            datetime.tm_sec);
}
```

### Get Temperature

```c
int ds3231_get_temperature(struct ds3231_dev *dev, float *temp_c);
```

Read temperature from the DS3231's integrated sensor.

**Parameters:**
- `dev`: Pointer to DS3231 device structure
- `temp_c`: Pointer to store temperature in Celsius

**Returns:** 0 on success, negative error code on failure

**Example:**
```c
float temperature;
int ret = ds3231_get_temperature(rtc, &temperature);
if (ret == 0) {
    LOG_INF("Temperature: %.2f°C", (double)temperature);
}
```

### Check Oscillator Status

```c
int ds3231_check_oscillator(struct ds3231_dev *dev, bool *stopped);
```

Check if the oscillator has stopped (indicates power loss).

**Parameters:**
- `dev`: Pointer to DS3231 device structure
- `stopped`: Pointer to store result (true if oscillator stopped)

**Returns:** 0 on success, negative error code on failure

**Example:**
```c
bool osc_stopped;
int ret = ds3231_check_oscillator(rtc, &osc_stopped);
if (ret == 0 && osc_stopped) {
    LOG_WRN("Oscillator stopped - time may be invalid!");
}
```

## Usage in main.c

Here's a complete example of using the DS3231 driver in your main.c:

```c
#include "ds3231.h"

int main(void)
{
    struct ds3231_dev *rtc;
    struct tm datetime;
    float temperature;
    int ret;

    /* Initialize DS3231 */
    rtc = ds3231_init("I2C_0");
    if (!rtc) {
        LOG_ERR("Failed to initialize DS3231");
        return -1;
    }

    /* Set initial date/time */
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
    }

    /* Main loop */
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

        k_msleep(10000);  /* Wait 10 seconds */
    }

    return 0;
}
```

## Notes

- The DS3231 uses BCD (Binary Coded Decimal) format internally, but the driver handles conversion automatically
- Time is stored in 24-hour format
- The oscillator starts automatically when VCC is applied
- Battery backup (VBAT) maintains timekeeping during power loss
- Temperature is updated every 64 seconds automatically
- Temperature resolution: 0.25°C
- Timekeeping accuracy: ±2ppm (0°C to +40°C), ±3.5ppm (-40°C to +85°C)

## Troubleshooting

**I2C communication fails:**
- Check I2C pins are correctly configured in device tree
- Verify pullup resistors on SDA and SCL (typically 4.7kΩ)
- Confirm DS3231 is powered (VCC between 2.3V and 5.5V)
- Check I2C address is 0x68

**Time not keeping:**
- Check oscillator status with `ds3231_check_oscillator()`
- Verify battery is connected to VBAT if using battery backup
- Ensure VCC or VBAT is always present

**Temperature reading incorrect:**
- Temperature sensor has ±3°C accuracy
- Temperature updates every 64 seconds
- Reading is from internal die temperature, not ambient

## License

This driver is provided as-is for use with Zephyr RTOS projects.
