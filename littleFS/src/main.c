/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 * Note:
 * Tested on nRF Connect SDK Version : 2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <hal/nrf_gpio.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include "nor_flash.h"
#include <SEGGER_RTT.h>

/* SLEEP_TIME */
#define SLEEP_TIME_MS 500

/* LED definitions */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
static const struct gpio_dt_spec blu_led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

/* MCU status pin - P1.14 output to indicate nRF52840 is active */
#define MCU_ACTIVE_PIN      14
#define MCU_ACTIVE_PORT     1

/* Wake/sleep pin - P1.13 input from external MCU (HIGH=awake, LOW=sleep) */
#define WAKEUP_PIN          13
#define WAKEUP_PORT         1

static const struct device *gpio1_dev;
static struct gpio_callback wakeup_cb_data;
static volatile bool sleep_requested = false;

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* Macro to log and flush RTT buffer with direct RTT write */
#define LOG_INF_FLUSH(fmt, ...) do { \
    char _log_buf[256]; \
    snprintf(_log_buf, sizeof(_log_buf), fmt "\r\n", ##__VA_ARGS__); \
    SEGGER_RTT_WriteString(0, _log_buf); \
    k_msleep(100); \
} while(0)

/* Data structure for setup.bin file */
typedef struct {
    int id;
    char name[20];
    float temperature_c;
    struct tm setDateTime;
} MyData;

/* List of pins to keep configured during init (not set to Hi-Z) */
/* Format: {port, pin} - P1.14 (MCU_ACTIVE output), P1.13 (WAKEUP input), LEDs */
static const struct {
    uint8_t port;
    uint8_t pin;
} init_keep_pins[] = {
    {1, 14},  /* MCU_ACTIVE_PIN - output to external MCU */
    {1, 13},  /* WAKEUP_PIN - input from external MCU */
    {1, 7},   /* LED0 (blue) */
    {1, 4},   /* LED1 (red) */
};

/* List of pins to keep configured during sleep (not set to Hi-Z) */
/* LEDs are set to Hi-Z during sleep to save power */
static const struct {
    uint8_t port;
    uint8_t pin;
} sleep_keep_pins[] = {
    {1, 14},  /* MCU_ACTIVE_PIN - output to external MCU */
    {1, 13},  /* WAKEUP_PIN - input from external MCU */
};

/* Check if a pin should be kept configured during init */
static bool is_init_keep_pin(uint8_t port, uint8_t pin)
{
    for (size_t i = 0; i < ARRAY_SIZE(init_keep_pins); i++) {
        if (init_keep_pins[i].port == port && init_keep_pins[i].pin == pin) {
            return true;
        }
    }
    return false;
}

/* Check if a pin should be kept configured during sleep */
static bool is_sleep_keep_pin(uint8_t port, uint8_t pin)
{
    for (size_t i = 0; i < ARRAY_SIZE(sleep_keep_pins); i++) {
        if (sleep_keep_pins[i].port == port && sleep_keep_pins[i].pin == pin) {
            return true;
        }
    }
    return false;
}

/* Set all GPIO pins to Hi-Z (disconnected) except specified pins for init */
static void disconnect_pins_for_init(void)
{
    /* Disconnect all P0 pins */
    for (uint8_t pin = 0; pin < 32; pin++) {
        if (!is_init_keep_pin(0, pin)) {
            nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(0, pin));
        }
    }
    
    /* Disconnect all P1 pins */
    for (uint8_t pin = 0; pin < 16; pin++) {
        if (!is_init_keep_pin(1, pin)) {
            nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(1, pin));
        }
    }
}

/* Set all GPIO pins to Hi-Z (disconnected) except specified pins for sleep */
static void disconnect_pins_for_sleep(void)
{
    /* Disconnect all P0 pins */
    for (uint8_t pin = 0; pin < 32; pin++) {
        if (!is_sleep_keep_pin(0, pin)) {
            nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(0, pin));
        }
    }
    
    /* Disconnect all P1 pins */
    for (uint8_t pin = 0; pin < 16; pin++) {
        if (!is_sleep_keep_pin(1, pin)) {
            nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(1, pin));
        }
    }
}

/* Enter deep sleep with GPIO wake-up on P1.13 rising edge */
void enter_deep_sleep(void)
{
    LOG_INF_FLUSH("Preparing for deep sleep...");
    
    /* 5 second countdown with alternating red/blue LED blink each second */
    for (int i = 3; i > 0; i--) {
        LOG_INF_FLUSH("Entering deep sleep in %d...", i);
        
        /* Alternate between red and blue LED each second */
        if (i % 2 == 1) {
            gpio_pin_set_dt(&red_led, 1);
            gpio_pin_set_dt(&blu_led, 0);
        } else {
            gpio_pin_set_dt(&red_led, 0);
            gpio_pin_set_dt(&blu_led, 1);
        }
        k_msleep(1000);
    }
    
    LOG_INF_FLUSH("Disconnecting pins and entering System OFF...");
    
    /* Turn off all LEDs before going to Hi-Z */
    gpio_pin_set_dt(&red_led, 0);
    gpio_pin_set_dt(&blu_led, 0);
    k_msleep(10);  /* Brief delay to ensure LEDs are off */
    
    /* Set MCU_ACTIVE pin low to indicate sleep */
    gpio_pin_set(gpio1_dev, MCU_ACTIVE_PIN, 0);
    
    /* Disconnect all pins except sleep_keep_pins (LEDs will be Hi-Z) */
    disconnect_pins_for_sleep();
    
    /* Configure P1.13 as wake-up source (sense high level, no pull - external MCU drives it) */
    nrf_gpio_cfg_sense_input(NRF_GPIO_PIN_MAP(WAKEUP_PORT, WAKEUP_PIN),
                             NRF_GPIO_PIN_NOPULL,
                             NRF_GPIO_PIN_SENSE_HIGH);
    
    LOG_INF_FLUSH("System in Deep Sleep");
    k_msleep(100);  /* Allow log to flush */
    
    /* Enter System OFF mode - lowest power state
     * Only GPIO DETECT or reset can wake from this state
     * On wake, the system will reset and start from main() */
    NRF_POWER->SYSTEMOFF = 1;
    __DSB();
    
    /* Should never reach here */
    while (1) {
        __WFE();
    }
}

/* Enter deep sleep immediately (no countdown) - used at startup if P1.13 is LOW */
static void enter_deep_sleep_immediate(void)
{
    LOG_INF_FLUSH("P1.13 is LOW at startup - entering deep sleep immediately");
    k_msleep(100);  /* Allow log to flush */
    
    /* Set MCU_ACTIVE pin (P1.14) low to indicate sleep */
    nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(MCU_ACTIVE_PORT, MCU_ACTIVE_PIN));
    
    /* Configure P1.13 as wake-up source (sense high level) */
    nrf_gpio_cfg_sense_input(NRF_GPIO_PIN_MAP(WAKEUP_PORT, WAKEUP_PIN),
                             NRF_GPIO_PIN_NOPULL,
                             NRF_GPIO_PIN_SENSE_HIGH);
    
    /* Enter System OFF mode */
    NRF_POWER->SYSTEMOFF = 1;
    __DSB();
    
    while (1) {
        __WFE();
    }
}

/* GPIO callback when P1.13 goes LOW - sets flag for main loop to handle */
static void wakeup_pin_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    /* Disable further interrupts to prevent re-entry */
    gpio_pin_interrupt_configure(gpio1_dev, WAKEUP_PIN, GPIO_INT_DISABLE);
    
    /* Set flag - actual sleep will be handled in main loop context */
    sleep_requested = true;
}

int main(void)
{
	int ret;

	/*========================================================================
	 * EARLY INITIALIZATION - Before any peripherals
	 * Set all pins to Hi-Z except P1.13, P1.14, and LEDs
	 *========================================================================*/
	
	/* 2 second delay at startup to allow flashing even if P1.13 is LOW */
	k_msleep(2000);
	
	/* Configure P1.14 as output HIGH (MCU active indicator) using direct nrf_gpio */
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(MCU_ACTIVE_PORT, MCU_ACTIVE_PIN));
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(MCU_ACTIVE_PORT, MCU_ACTIVE_PIN));
	
	/* Configure P1.13 as input (wake/sleep signal from external MCU) */
	nrf_gpio_cfg_input(NRF_GPIO_PIN_MAP(WAKEUP_PORT, WAKEUP_PIN), NRF_GPIO_PIN_NOPULL);
	
	/* Set all other pins to Hi-Z except P1.13, P1.14, and LED pins */
	disconnect_pins_for_init();
	
	/* Quick LED blink to show system is alive (Red then Blue) */
	/* LEDs are active low on this board, so we use nrf_gpio directly */
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1, 4));  /* Red LED P1.04 */
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1, 7));  /* Blue LED P1.07 */
	
	nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(1, 4));   /* Red ON */
	k_msleep(150);
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1, 4));     /* Red OFF */
	nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(1, 7));   /* Blue ON */
	k_msleep(150);
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1, 7));     /* Blue OFF */
	
	/* Check if P1.13 is HIGH - if LOW, go to deep sleep immediately */
	LOG_INF_FLUSH("Checking P1.13 wake signal...");
	
	/* Wait 1 second and verify P1.13 stays HIGH */
	for (int i = 0; i < 10; i++) {
		k_msleep(100);
		if (nrf_gpio_pin_read(NRF_GPIO_PIN_MAP(WAKEUP_PORT, WAKEUP_PIN)) == 0) {
			/* P1.13 is LOW - external MCU wants us to sleep */
			enter_deep_sleep_immediate();
			/* Never returns */
		}
	}
	
	LOG_INF_FLUSH("P1.13 is HIGH - continuing startup");

	/*========================================================================
	 * NORMAL INITIALIZATION
	 *========================================================================*/

	/* Initialize GPIO1 for MCU_ACTIVE and WAKEUP pins */
	gpio1_dev = DEVICE_DT_GET(DT_NODELABEL(gpio1));
	if (!device_is_ready(gpio1_dev)) {
		LOG_ERR("GPIO1 device not ready");
		return -1;
	}

	/* Initialize LEDs */
	if (!gpio_is_ready_dt(&red_led)) {
		LOG_ERR("Red LED device not ready");
		return -1;
	}
	if (!gpio_is_ready_dt(&blu_led)) {
		LOG_ERR("Blue LED device not ready");
		return -1;
	}

	ret = gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure red LED");
		return -1;
	}
	ret = gpio_pin_configure_dt(&blu_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure blue LED");
		return -1;
	}

	/* Reconfigure P1.14 using Zephyr GPIO API (already set HIGH via nrf_gpio) */
	ret = gpio_pin_configure(gpio1_dev, MCU_ACTIVE_PIN, GPIO_OUTPUT_HIGH);
	if (ret < 0) {
		LOG_ERR("Failed to configure MCU_ACTIVE pin");
		return -1;
	}

	/* Configure P1.13 as input using Zephyr GPIO API */
	ret = gpio_pin_configure(gpio1_dev, WAKEUP_PIN, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Failed to configure WAKEUP pin");
		return -1;
	}

	/* Set up interrupt callback for P1.13 falling edge (HIGH to LOW triggers sleep) */
	ret = gpio_pin_interrupt_configure(gpio1_dev, WAKEUP_PIN, GPIO_INT_EDGE_TO_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure WAKEUP interrupt");
		return -1;
	}

	gpio_init_callback(&wakeup_cb_data, wakeup_pin_callback, BIT(WAKEUP_PIN));
	gpio_add_callback(gpio1_dev, &wakeup_cb_data);

	/* Blink LEDs to show system is starting */
	gpio_pin_set_dt(&red_led, 1);
	k_msleep(200);
	gpio_pin_set_dt(&red_led, 0);
	gpio_pin_set_dt(&blu_led, 1);
	k_msleep(200);
	gpio_pin_set_dt(&blu_led, 0);

	// ******************** Little FS test **************

	char write_data[] = "Hello, Dual NOR Flash with LittleFS!";

	LOG_INF_FLUSH("Starting Dual NOR Flash Demo");

	// Initialize the full system with LittleFS
	ret = nor_flash_system_init();
	if (ret != 0) {
		LOG_ERR("System initialization failed");
		return ret;
	}

	// Read test file from FLASH1 - get file size first
	int file_size = nor_flash_get_file_size(FLASH1, "max_test.txt");
	if (file_size > 0) {
		/* Allocate buffer based on file size (+1 for null terminator) */
		char *read_buffer = k_malloc(file_size + 1);
		if (read_buffer != NULL) {
			memset(read_buffer, 0, file_size + 1);
			ret = nor_flash_read_file(FLASH1, "max_test.txt", read_buffer, file_size);
			if (ret >= 0) {
				read_buffer[ret] = '\0';  /* Ensure null termination */
				LOG_INF_FLUSH("Read max_test.txt (%d bytes): %s", ret, read_buffer);
			} else {
				LOG_ERR("Read max_test.txt failed: %d", ret);
			}
			k_free(read_buffer);
		} else {
			LOG_ERR("Failed to allocate %d bytes for max_test.txt", file_size + 1);
		}
	} else if (file_size == 0) {
		LOG_INF_FLUSH("max_test.txt is empty");
	} else {
		LOG_INF_FLUSH("max_test.txt not found or error: %d", file_size);
	}

	// Write nrf_test file to FLASH1 (SPIFX - 16MB)
	ret = nor_flash_write_file(FLASH1, "nrf_test.txt", write_data, strlen(write_data));
	if (ret != 0) {
		LOG_ERR("Write nrf_test.txt failed");
		return ret;
	}

	// Read test file from FLASH1 - dynamic buffer
	file_size = nor_flash_get_file_size(FLASH1, "nrf_test.txt");
	if (file_size > 0) {
		char *read_buffer = k_malloc(file_size + 1);
		if (read_buffer != NULL) {
			memset(read_buffer, 0, file_size + 1);
			ret = nor_flash_read_file(FLASH1, "nrf_test.txt", read_buffer, file_size);
			if (ret >= 0) {
				read_buffer[ret] = '\0';
				LOG_INF_FLUSH("Read nrf_test.txt: %s", read_buffer);

				// Verify data
				if (strcmp(write_data, read_buffer) == 0) {
					LOG_INF_FLUSH("Data verification successful!");
				} else {
					LOG_ERR("Data verification failed!");
				}
			}
			k_free(read_buffer);
		}
	}

	// ******************** Setup.bin struct test **************

	LOG_INF_FLUSH("Reading max_test_data.bin file from FLASH2 (QSPI - 64MB)...");

	MyData readData;
	static char dateTimeStr[128];

	// Direct read of max_test_data.bin file from FLASH2
	ret = nor_flash_read_struct(FLASH2, "max_test_data.bin", &readData, sizeof(MyData));

	if (ret == -ENOENT) {
		LOG_INF_FLUSH("max_test_data.bin file does not exist");
	} else if (ret < 0) {
		LOG_ERR("Failed to read max_test_data.bin file: error %d", ret);
	} else {
		snprintf(dateTimeStr, sizeof(dateTimeStr), "%04d-%02d-%02d %02d:%02d:%02dZ", 
			 readData.setDateTime.tm_year + 1900,
			 readData.setDateTime.tm_mon + 1,
			 readData.setDateTime.tm_mday,
			 readData.setDateTime.tm_hour,
			 readData.setDateTime.tm_min,
			 readData.setDateTime.tm_sec);

		LOG_INF_FLUSH("MAX Setup Data Read Successfully:");
		LOG_INF_FLUSH("  ID: %d", readData.id);
		LOG_INF_FLUSH("  Name: %s", readData.name);
		LOG_INF_FLUSH("  Temperature: %.1fC", (double)readData.temperature_c);
		LOG_INF_FLUSH("  Date/Time: %s", dateTimeStr);
		k_msleep(50);
	}

	LOG_INF_FLUSH("Writing nrf_test_data.bin file to FLASH2 (QSPI - 64MB)...");

	MyData writeData = {
		.id = 42,
		.name = "NRF Write",
		.temperature_c = 23.5,
	};
	time_t now = time(NULL);
	gmtime_r(&now, &writeData.setDateTime);

	ret = nor_flash_write_struct(FLASH2, "nrf_test_data.bin", &writeData, sizeof(MyData));
	if (ret != 0) {
		LOG_ERR("Failed to write nrf_test_data.bin file: error %d", ret);
	} else {
		LOG_INF_FLUSH("nrf_test_data.bin file written successfully to FLASH2");
	}

	LOG_INF_FLUSH("Reading nrf_test_data.bin file from FLASH2...");

	ret = nor_flash_read_struct(FLASH2, "nrf_test_data.bin", &readData, sizeof(MyData));

	if (ret == -ENOENT) {
		LOG_INF_FLUSH("nrf_test_data.bin file does not exist");
	} else if (ret < 0) {
		LOG_ERR("Failed to read nrf_test_data.bin file: error %d", ret);
	} else {
		snprintf(dateTimeStr, sizeof(dateTimeStr), "%04d-%02d-%02d %02d:%02d:%02dZ", 
			 readData.setDateTime.tm_year + 1900,
			 readData.setDateTime.tm_mon + 1,
			 readData.setDateTime.tm_mday,
			 readData.setDateTime.tm_hour,
			 readData.setDateTime.tm_min,
			 readData.setDateTime.tm_sec);

		LOG_INF_FLUSH("NRF Setup Data Read Successfully:");
		LOG_INF_FLUSH("  ID: %d", readData.id);
		LOG_INF_FLUSH("  Name: %s", readData.name);
		LOG_INF_FLUSH("  Temperature: %.1f C", (double)readData.temperature_c);
		LOG_INF_FLUSH("  Date/Time: %s", dateTimeStr);
		k_msleep(50);
	}

	// ******************** End of Little FS test **************

	LOG_INF_FLUSH("All tests completed successfully!");
	LOG_INF_FLUSH("System running - P1.13 LOW will trigger deep sleep");
	k_msleep(200);

	/* Main loop - blink blue LED periodically */
	while (1) {
		/* Check if sleep was requested via P1.13 interrupt */
		if (sleep_requested) {
			LOG_INF_FLUSH("Sleep signal received (P1.13 went LOW)");
			enter_deep_sleep();
			/* Never returns */
		}
		
		gpio_pin_toggle_dt(&blu_led);
		k_msleep(SLEEP_TIME_MS);
	}
}
