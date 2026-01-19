# DS3231 RTC Driver - Quick Integration Guide

## What's Been Done

I've implemented a complete DS3231 Real-Time Clock driver for your Zephyr project. Here's what was created:

### Files Created:
1. **src/ds3231.h** - Driver header with API declarations
2. **src/ds3231.c** - Complete driver implementation
3. **src/ds3231_example.c** - Usage examples
4. **DS3231_DRIVER_README.md** - Complete documentation

### Files Modified:
1. **CMakeLists.txt** - Added ds3231.c to build
2. **prj.conf** - Enabled I2C support
3. **boards/arm/nrf52832_ev_bc832/nrf52832_ev_bc832.overlay** - Added I2C configuration

## Hardware Setup

Connect your DS3231 module to the nRF52832:

```
DS3231          nRF52832
------          --------
VCC     <--->   3.3V
GND     <--->   GND
SDA     <--->   P0.26 (with 4.7kΩ pullup to 3.3V)
SCL     <--->   P0.27 (with 4.7kΩ pullup to 3.3V)
```

**Note:** If your hardware uses different pins, edit the overlay file:
- File: `boards/arm/nrf52832_ev_bc832/nrf52832_ev_bc832.overlay`
- Change pin numbers in the `i2c0_default` and `i2c0_sleep` sections

## How to Use in main.c

### Step 1: Include the header

```c
#include "ds3231.h"
```

### Step 2: Add to your main() function

```c
int main(void)
{
    struct ds3231_dev *rtc;
    struct tm datetime;
    int ret;

    /* Your existing initialization code... */

    /* Initialize DS3231 */
    rtc = ds3231_init("I2C_0");
    if (!rtc) {
        LOG_ERR("Failed to initialize DS3231");
        return -1;
    }

    /* Set the current date/time (do this once) */
    datetime.tm_year = 2026 - 1900;  /* Year since 1900 */
    datetime.tm_mon = 0;              /* January (0-11) */
    datetime.tm_mday = 18;            /* Day 18 */
    datetime.tm_hour = 14;            /* 14:30:00 */
    datetime.tm_min = 30;
    datetime.tm_sec = 0;
    datetime.tm_wday = 6;             /* Saturday */

    ret = ds3231_set_datetime(rtc, &datetime);
    if (ret < 0) {
        LOG_ERR("Failed to set datetime");
    }

    /* Your existing code... */

    while (1) {
        /* Read current time */
        ret = ds3231_get_datetime(rtc, &datetime);
        if (ret == 0) {
            LOG_INF("Current time: %02d:%02d:%02d",
                    datetime.tm_hour,
                    datetime.tm_min,
                    datetime.tm_sec);
        }

        k_msleep(10000);  /* Wait 10 seconds */
    }

    return 0;
}
```

## API Quick Reference

### Initialize
```c
struct ds3231_dev *rtc = ds3231_init("I2C_0");
```

### Set Date/Time
```c
struct tm datetime;
datetime.tm_year = 2026 - 1900;  /* Years since 1900 */
datetime.tm_mon = 0;              /* Month (0-11) */
datetime.tm_mday = 18;            /* Day (1-31) */
datetime.tm_hour = 14;            /* Hour (0-23) */
datetime.tm_min = 30;             /* Minute (0-59) */
datetime.tm_sec = 0;              /* Second (0-59) */
datetime.tm_wday = 6;             /* Day of week (0=Sun, 6=Sat) */

ds3231_set_datetime(rtc, &datetime);
```

### Get Date/Time
```c
struct tm datetime;
ds3231_get_datetime(rtc, &datetime);
LOG_INF("Time: %02d:%02d:%02d", datetime.tm_hour, datetime.tm_min, datetime.tm_sec);
```

### Get Temperature
```c
float temperature;
ds3231_get_temperature(rtc, &temperature);
LOG_INF("Temperature: %.2f°C", (double)temperature);
```

### Check Oscillator (Power Loss Detection)
```c
bool osc_stopped;
ds3231_check_oscillator(rtc, &osc_stopped);
if (osc_stopped) {
    LOG_WRN("Power was lost - time may be invalid!");
}
```

## Build and Flash

```bash
# Build the project
west build -b nrf52832_ev_bc832

# Flash to device
west flash
```

## Testing

After flashing:
1. The DS3231 will be initialized automatically
2. Check RTT console for log messages
3. You should see time being read every 10 seconds (if you added the example code)

## Troubleshooting

**"Failed to initialize DS3231"**
- Check I2C wiring (SDA, SCL, VCC, GND)
- Verify pullup resistors are present (4.7kΩ on SDA and SCL)
- Confirm DS3231 is powered (3.3V on VCC)

**"Failed to get datetime"**
- Check I2C communication is working
- Try reading temperature first (simpler operation)
- Use a logic analyzer to verify I2C signals

**Wrong time displayed**
- Make sure you set the time first with `ds3231_set_datetime()`
- Check if oscillator stopped flag is set
- Verify battery is connected if using battery backup

## Next Steps

1. **Integrate with your existing code**: Add DS3231 calls where you need timestamps
2. **Use with LittleFS**: Store timestamps with your file operations
3. **Add battery backup**: Connect a CR2032 battery to VBAT pin for timekeeping during power loss
4. **Implement alarms**: Extend the driver to use DS3231's alarm features (see datasheet)

## Example: Timestamping File Operations

```c
/* Write file with timestamp */
struct tm datetime;
ds3231_get_datetime(rtc, &datetime);

MyData writeData = {
    .id = 42,
    .name = "Sensor Data",
    .temperature_c = 23.5,
    .setDateTime = datetime  /* Add timestamp from RTC */
};

nor_flash_write_struct(FLASH2, "data.bin", &writeData, sizeof(MyData));
```

## Support

For more details, see:
- **DS3231_DRIVER_README.md** - Complete API documentation
- **src/ds3231_example.c** - More usage examples
- **DS3231_datasheet.txt** - Hardware datasheet

The driver is ready to use! Just add the initialization and API calls to your main.c file.
