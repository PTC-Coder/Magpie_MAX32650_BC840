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
#include <string.h>
#include <time.h>
#include <errno.h>
#include "nor_flash.h"
#include <SEGGER_RTT.h>


/* SLEEP_TIME */
#define SLEEP_TIME_MS 10 * 60 * 1000

/* LED definitions */
#define RED_LED_NODE DT_ALIAS(redled)
#define BLU_LED_NODE DT_ALIAS(blueled)
static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);
static const struct gpio_dt_spec blu_led = GPIO_DT_SPEC_GET(BLU_LED_NODE, gpios);


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





int main(void)
{
	int ret;

	/* Initialize LEDs */
	if (!device_is_ready(red_led.port)) {
		LOG_ERR("Red LED device not ready");
		return -1;
	}
	if (!device_is_ready(blu_led.port)) {
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

	/* Blink LEDs to show system is starting */
	gpio_pin_set_dt(&red_led, 1);
	k_msleep(200);
	gpio_pin_set_dt(&red_led, 0);
	gpio_pin_set_dt(&blu_led, 1);
	k_msleep(200);
	gpio_pin_set_dt(&blu_led, 0);

	// ******************** Little FS test **************



	char write_data[] = "Hello, Dual NOR Flash with LittleFS!";
    	char read_buffer[100];
    
    	LOG_INF_FLUSH("Starting Dual NOR Flash Demo");
    
    // Initialize the system
    // Initialize the full system with LittleFS
    ret = nor_flash_system_init();
    if (ret != 0) {
        LOG_ERR("System initialization failed");
        return ret;
    }
    
    // Write test file to FLASH1 (SPIFX - 16MB)
    ret = nor_flash_write_file(FLASH1, "test.txt", write_data, strlen(write_data));
    if (ret != 0) {
        LOG_ERR("Write test failed");
        return ret;
    }
    
    // Read test file from FLASH1
    memset(read_buffer, 0, sizeof(read_buffer));
    ret = nor_flash_read_file(FLASH1, "test.txt", read_buffer, sizeof(read_buffer));
    if (ret < 0) {
        LOG_ERR("Read test failed");
        return ret;
    }
    
    LOG_INF_FLUSH("Read data: %s", read_buffer);
    
    // Verify data
    if (strcmp(write_data, read_buffer) == 0) {
        LOG_INF_FLUSH("Data verification successful!");
    } else {
        LOG_ERR("Data verification failed!");
    }
    
    // ******************** Setup.bin struct test **************
    
    LOG_INF_FLUSH("Reading setup.bin file from FLASH2 (QSPI - 64MB)...");
    
    MyData readData;
    static char dateTimeStr[128];
    
    // Direct read of setup.bin file from FLASH2
    ret = nor_flash_read_struct(FLASH2, "setup.bin", &readData, sizeof(MyData));
    
    if (ret == -ENOENT) {
        LOG_INF_FLUSH("setup.bin file does not exist");
    } else if (ret < 0) {
        LOG_ERR("Failed to read setup.bin file: error %d", ret);
    } else {
        // Successfully read the file - display the data
        snprintf(dateTimeStr, sizeof(dateTimeStr), "%04d-%02d-%02d %02d:%02d:%02dZ", 
                 readData.setDateTime.tm_year + 1900,
                 readData.setDateTime.tm_mon + 1,
                 readData.setDateTime.tm_mday,
                 readData.setDateTime.tm_hour,
                 readData.setDateTime.tm_min,
                 readData.setDateTime.tm_sec);
        
        LOG_INF_FLUSH("Setup Data Read Successfully:");
        LOG_INF_FLUSH("  ID: %d", readData.id);
        LOG_INF_FLUSH("  Name: %s", readData.name);
        LOG_INF_FLUSH("  Temperature: %.1fC", (double)readData.temperature_c);
        LOG_INF_FLUSH("  Date/Time: %s", dateTimeStr);
        k_msleep(50); // Allow RTT buffer to drain
    }

	LOG_INF_FLUSH("Writing nRFsetup.bin file to FLASH2 (SQPI - 64MB)...");

	MyData writeData = {
		.id = 42,
		.name = "NRF Write",
		.temperature_c = 23.5,
	};
	// Set current time as example
	time_t now = time(NULL);
	gmtime_r(&now, &writeData.setDateTime);
	
	// Write the struct to nRFsetup.bin file on FLASH2
	ret = nor_flash_write_struct(FLASH2, "nRFsetup.bin", &writeData, sizeof(MyData));
	if (ret != 0) {
		LOG_ERR("Failed to write nRFsetup.bin file: error %d", ret);
	} else {
		LOG_INF_FLUSH("nRFsetup.bin file written successfully to FLASH2");
	}

	LOG_INF_FLUSH("Reading nRFsetup.bin file from FLASH2...");

	  // Direct read of nRFsetup.bin file from FLASH1
    ret = nor_flash_read_struct(FLASH2, "nRFsetup.bin", &readData, sizeof(MyData));
    
    if (ret == -ENOENT) {
        LOG_INF_FLUSH("nRFsetup.bin file does not exist");
    } else if (ret < 0) {
        LOG_ERR("Failed to read nRFsetup.bin file: error %d", ret);
    } else {
        // Successfully read the file - display the data
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
        k_msleep(50); // Allow RTT buffer to drain
    }
	
	// ******************** End of Little FS test **************

	//***************************************************	
	
	// Final delay to ensure all RTT messages are transmitted
	LOG_INF_FLUSH("All tests completed successfully!");
	k_msleep(200);

	/* Main loop - blink blue LED periodically */
	while (1) {
		gpio_pin_toggle_dt(&blu_led);
		k_msleep(SLEEP_TIME_MS);
	}
}