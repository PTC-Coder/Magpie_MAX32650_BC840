# Hardware Configuration Summary

## Overview

The nRF52840_BC840 board configuration has been updated to reflect the actual hardware:
- Correct LED pin assignments
- Removed button support (no buttons on board)
- Added DS3231 RTC on I2C interface

## LED Configuration

### Pin Assignments
| LED | Pin | Color | Active |
|-----|-----|-------|--------|
| LED0 (Red) | P1.04 | Red | Active Low |
| LED1 (Blue) | P1.07 | Blue | Active Low |

### Device Tree Aliases
```dts
aliases {
    led0 = &led0;
    led1 = &led1;
    redled = &led0;
    blueled = &led1;
    ...
};
```

### Usage in Code
```c
#include <zephyr/drivers/gpio.h>

#define RED_LED_NODE DT_ALIAS(redled)
#define BLU_LED_NODE DT_ALIAS(blueled)

static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);
static const struct gpio_dt_spec blu_led = GPIO_DT_SPEC_GET(BLU_LED_NODE, gpios);

// Turn on red LED
gpio_pin_set_dt(&red_led, 1);

// Turn off blue LED
gpio_pin_set_dt(&blu_led, 0);

// Toggle blue LED
gpio_pin_toggle_dt(&blu_led);
```

## Buttons

**Status:** ❌ Removed

All button-related code has been removed from:
- Device tree definitions
- GPIO initialization
- Interrupt callbacks
- Main application code

The board has no physical buttons.

## RTC (DS3231)

### Hardware Connection
| nRF52840 Pin | Function | DS3231 Pin | Description |
|--------------|----------|------------|-------------|
| P0.26 | I2C SDA | SDA | Data line |
| P0.27 | I2C SCL | SCL | Clock line |
| 3.3V | Power | VCC | Power supply |
| GND | Ground | GND | Ground |

### I2C Configuration
- **Interface:** I2C0 (TWI0)
- **Address:** 0x68
- **Speed:** Standard (100kHz) or Fast (400kHz)
- **Pull-ups:** External pull-ups required (typically 4.7kΩ)

### Device Tree Configuration
```dts
&i2c0 {
    compatible = "nordic,nrf-twi";
    status = "okay";
    pinctrl-0 = <&i2c0_default>;
    pinctrl-1 = <&i2c0_sleep>;
    pinctrl-names = "default", "sleep";
    
    ds3231: ds3231@68 {
        compatible = "maxim,ds3231";
        reg = <0x68>;
        status = "okay";
    };
};
```

### Pin Control Configuration
```dts
i2c0_default: i2c0_default {
    group1 {
        psels = <NRF_PSEL(TWIM_SDA, 0, 26)>,
                <NRF_PSEL(TWIM_SCL, 0, 27)>;
    };
};

i2c0_sleep: i2c0_sleep {
    group1 {
        psels = <NRF_PSEL(TWIM_SDA, 0, 26)>,
                <NRF_PSEL(TWIM_SCL, 0, 27)>;
        low-power-enable;
    };
};
```

### RTC Alias
```dts
aliases {
    rtc = &ds3231;
};
```

### Using the RTC in Code

To use the DS3231 RTC, you'll need to add RTC support. Here's an example:

```c
#include <zephyr/drivers/rtc.h>

// Get RTC device
const struct device *rtc = DEVICE_DT_GET(DT_ALIAS(rtc));

if (!device_is_ready(rtc)) {
    LOG_ERR("RTC device not ready");
    return -ENODEV;
}

// Set time
struct rtc_time time = {
    .tm_year = 2026 - 1900,  // Years since 1900
    .tm_mon = 0,              // January (0-11)
    .tm_mday = 18,            // Day of month (1-31)
    .tm_hour = 12,            // Hour (0-23)
    .tm_min = 30,             // Minute (0-59)
    .tm_sec = 0               // Second (0-59)
};

ret = rtc_set_time(rtc, &time);

// Read time
struct rtc_time current_time;
ret = rtc_get_time(rtc, &current_time);

LOG_INF("Current time: %04d-%02d-%02d %02d:%02d:%02d",
        current_time.tm_year + 1900,
        current_time.tm_mon + 1,
        current_time.tm_mday,
        current_time.tm_hour,
        current_time.tm_min,
        current_time.tm_sec);
```

### DS3231 Features
- **Accuracy:** ±2ppm (±1 minute per year)
- **Temperature Compensated:** Built-in TCXO
- **Battery Backup:** Maintains time during power loss
- **Alarms:** Two programmable alarms
- **Square Wave Output:** Configurable frequency
- **Temperature Sensor:** Built-in (±3°C accuracy)

## Complete Pin Summary

### GPIO Pins Used

| Pin | Function | Device | Description |
|-----|----------|--------|-------------|
| P0.04 | SPI CS | FLASH1 | SPIFX Chip Select |
| P0.06 | SPI MOSI | FLASH1 | SPIFX Data Out |
| P0.07 | SPI SCK | FLASH1 | SPIFX Clock |
| P0.08 | GPIO | FLASH1 | SPIFX IO2 (future QSPI) |
| P0.10 | UART RX | UART0 | Debug UART |
| P0.11 | UART TX | UART0 | Debug UART |
| P0.12 | SPI MISO | FLASH1 | SPIFX Data In |
| P0.15 | SPI CS | FLASH2 | QSPI Chip Select |
| P0.17 | QSPI IO1 | FLASH2 | QSPI Data 1 |
| P0.19 | QSPI SCK | FLASH2 | QSPI Clock |
| P0.22 | QSPI IO3 | FLASH2 | QSPI Data 3 |
| P0.25 | QSPI IO2 | FLASH2 | QSPI Data 2 |
| P0.26 | I2C SDA | DS3231 | I2C Data |
| P0.27 | I2C SCL | DS3231 | I2C Clock |
| P1.01 | QSPI IO0 | FLASH2 | QSPI Data 0 |
| P1.04 | GPIO | LED | Red LED |
| P1.07 | GPIO | LED | Blue LED |
| P1.09 | GPIO | FLASH1 | SPIFX IO3 (future QSPI) |

### Available Pins

The following pins are available for other uses:
- P0.00 - P0.03
- P0.05
- P0.09
- P0.13 - P0.14
- P0.16
- P0.18
- P0.20 - P0.21
- P0.23 - P0.24
- P0.28 - P0.31
- P1.00
- P1.02 - P1.03
- P1.05 - P1.06
- P1.08
- P1.10 - P1.15

## Application Behavior

### Startup Sequence
1. Initialize LEDs
2. Blink red LED (200ms)
3. Blink blue LED (200ms)
4. Initialize dual flash system
5. Run flash tests
6. Enter main loop

### Main Loop
- Blue LED toggles every 10 minutes
- Indicates system is running
- Can be modified for application needs

## Configuration Files Updated

### Device Tree
- **File:** `boards/arm/nrf52840_bc840/nrf52840_bc840.dts`
- **Changes:**
  - Updated LED pin assignments (P1.04, P1.07)
  - Removed button definitions
  - Added DS3231 RTC on I2C0
  - Added RTC alias

### Main Application
- **File:** `src/main.c`
- **Changes:**
  - Removed all button code
  - Updated LED definitions
  - Added LED blink on startup
  - Added LED toggle in main loop
  - Simplified initialization

## Testing Checklist

### LEDs
- [ ] Red LED blinks on startup
- [ ] Blue LED blinks on startup
- [ ] Blue LED toggles in main loop
- [ ] LEDs are on correct pins (P1.04, P1.07)

### I2C / RTC
- [ ] I2C bus initializes correctly
- [ ] DS3231 detected at address 0x68
- [ ] Can read/write RTC time
- [ ] RTC maintains time with battery backup

### Flash
- [ ] FLASH1 initializes (SPIFX)
- [ ] FLASH2 initializes (QSPI)
- [ ] Both filesystems mount
- [ ] File operations work

## Troubleshooting

### LEDs Not Working
- Check pin connections (P1.04 for red, P1.07 for blue)
- Verify LEDs are active low
- Check for shorts or opens
- Verify GPIO configuration

### RTC Not Detected
- Check I2C wiring (P0.26 SDA, P0.27 SCL)
- Verify pull-up resistors (4.7kΩ typical)
- Check power supply (3.3V)
- Scan I2C bus: `i2c scan i2c@40003000`
- Verify address 0x68 responds

### I2C Communication Errors
- Check pull-up resistors
- Verify wire length (keep short)
- Check for noise/interference
- Try lower I2C speed
- Verify ground connection

## Summary

**Hardware Configuration:**
- ✅ 2 LEDs (Red: P1.04, Blue: P1.07)
- ✅ 0 Buttons (removed)
- ✅ DS3231 RTC on I2C0 (P0.26, P0.27)
- ✅ 2 Flash chips (FLASH1, FLASH2)
- ✅ UART debug (P0.10, P0.11)

**Status:** Configuration complete and ready for testing

---

**Date:** January 18, 2026  
**Board:** nRF52840_BC840  
**Configuration:** Production hardware
