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
#include "mx25l51245g_flash.h"
#include <SEGGER_RTT.h>


/* STEP 9 - Increase the sleep time from 100ms to 10 minutes  */
#define SLEEP_TIME_MS 10 * 60 * 1000

/* SW0_NODE is the devicetree node identifier for the node with alias "sw0" */
#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define SW3_NODE DT_ALIAS(sw3)
static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(SW1_NODE, gpios);
static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET(SW2_NODE, gpios);
static const struct gpio_dt_spec button3 = GPIO_DT_SPEC_GET(SW3_NODE, gpios);

/* LED0_NODE is the devicetree node identifier for the node with alias "led0". */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

/* STEP 4 - Define the callback function */
void button0_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	gpio_pin_toggle_dt(&led0);
}
void button1_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	gpio_pin_toggle_dt(&led1);
}
void button2_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	gpio_pin_toggle_dt(&led2);
}
void button3_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	gpio_pin_toggle_dt(&led3);
}
/* STEP 5 - Define a variable of type static struct gpio_callback */
static struct gpio_callback button0_cb_data;
static struct gpio_callback button1_cb_data;
static struct gpio_callback button2_cb_data;
static struct gpio_callback button3_cb_data;


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

	if (!device_is_ready(led0.port)) {
		return -1;
	}
	if (!device_is_ready(led1.port)) {
		return -1;
	}
	if (!device_is_ready(led2.port)) {
		return -1;
	}
	if (!device_is_ready(led3.port)) {
		return -1;
	}

	if (!device_is_ready(button0.port)) {
		return -1;
	}
	if (!device_is_ready(button1.port)) {
		return -1;
	}
	if (!device_is_ready(button2.port)) {
		return -1;
	}
	if (!device_is_ready(button3.port)) {
		return -1;
	}

	ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return -1;
	}


	ret = gpio_pin_configure_dt(&button0, GPIO_INPUT);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&button1, GPIO_INPUT);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&button2, GPIO_INPUT);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&button3, GPIO_INPUT);
	if (ret < 0) {
		return -1;
	}
	/* STEP 3 - Configure the interrupt on the button's pin */
	ret = gpio_pin_interrupt_configure_dt(&button0, GPIO_INT_EDGE_TO_ACTIVE);
	ret = gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_EDGE_TO_ACTIVE);
	ret = gpio_pin_interrupt_configure_dt(&button2, GPIO_INT_EDGE_TO_ACTIVE);
	ret = gpio_pin_interrupt_configure_dt(&button3, GPIO_INT_EDGE_TO_ACTIVE);

	/* STEP 6 - Initialize the static struct gpio_callback variable   */
	gpio_init_callback(&button0_cb_data, button0_pressed, BIT(button0.pin));
	gpio_init_callback(&button1_cb_data, button1_pressed, BIT(button1.pin));
	gpio_init_callback(&button2_cb_data, button2_pressed, BIT(button2.pin));
	gpio_init_callback(&button3_cb_data, button3_pressed, BIT(button3.pin));

	/* STEP 7 - Add the callback function by calling gpio_add_callback()   */
	gpio_add_callback(button0.port, &button0_cb_data);
	gpio_add_callback(button1.port, &button1_cb_data);
	gpio_add_callback(button2.port, &button2_cb_data);
	gpio_add_callback(button3.port, &button3_cb_data);


	// ******************** Little FS test **************



	char write_data[] = "Hello, MX25L51245G NOR Flash with LittleFS!";
    	char read_buffer[100];
    
    	LOG_INF_FLUSH("Starting MX25L51245G NOR Flash Demo");
    
    // Initialize the system
    // Initialize the full system with LittleFS
    ret = mx25l51245g_system_init();
    if (ret != 0) {
        LOG_ERR("System initialization failed");
        return ret;
    }
    
    // Write test file
    ret = mx25l51245g_write_file("test.txt", write_data, strlen(write_data));
    if (ret != 0) {
        LOG_ERR("Write test failed");
        return ret;
    }
    
    // Read test file
    memset(read_buffer, 0, sizeof(read_buffer));
    ret = mx25l51245g_read_file("test.txt", read_buffer, sizeof(read_buffer));
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
    
    LOG_INF_FLUSH("Reading setup.bin file from MAX...");
    
    MyData readData;
    static char dateTimeStr[128];
    
    // Direct read of setup.bin file
    ret = mx25l51245g_read_struct("setup.bin", &readData, sizeof(MyData));
    
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

	LOG_INF_FLUSH("Writing NRFsetup.bin file to flash ...");

	MyData writeData = {
		.id = 42,
		.name = "NRF Write",
		.temperature_c = 23.5,
	};
	// Set current time as example
	time_t now = time(NULL);
	gmtime_r(&now, &writeData.setDateTime);
	
	// Write the struct to setup.bin file
	ret = mx25l51245g_write_struct("nRFsetup.bin", &writeData, sizeof(MyData));
	if (ret != 0) {
		LOG_ERR("Failed to write nRFsetup.bin file: error %d", ret);
	} else {
		LOG_INF_FLUSH("nRFsetup.bin file written successfully");
	}

	LOG_INF_FLUSH("Reading nRFsetup.bin file from flash ...");

	  // Direct read of nRFsetup.bin file
    ret = mx25l51245g_read_struct("nRFsetup.bin", &readData, sizeof(MyData));
    
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

	while (1) {
		/* STEP 8 - Remove the polling code */

		k_msleep(SLEEP_TIME_MS);
	}
    



}