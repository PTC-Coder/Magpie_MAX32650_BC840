/*
 * Generic NOR Flash Driver with LittleFS Support
 * Header File
 * 
 * Supports dual flash configuration with compile-time size selection:
 * - Flash1 (SPIFX): SPI1 interface in QSPI mode - configurable size (default: 16MB)
 * - Flash2 (QSPI): Hardware QSPI interface - configurable size (default: 64MB)
 * 
 * Configure flash sizes in CMakeLists.txt:
 * - FLASH1_SIZE_MB: 64, 32, or 16 (default: 16)
 * - FLASH2_SIZE_MB: 64, 32, or 16 (default: 64)
 */

#ifndef NOR_FLASH_H
#define NOR_FLASH_H

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flash size configuration - set via CMakeLists.txt */
#ifndef FLASH1_SIZE_MB
#define FLASH1_SIZE_MB 16  /* Default: 16MB for Flash1 (SPIFX/SPI1) */
#endif

#ifndef FLASH2_SIZE_MB
#define FLASH2_SIZE_MB 64  /* Default: 64MB for Flash2 (QSPI hardware) */
#endif

/* Calculate flash parameters based on size */
#if FLASH1_SIZE_MB == 64
#define FLASH1_CHIP_NAME         "MX25L51245GZ2I-08G"
#define FLASH1_CHIP_SIZE_BYTES   67108864UL
#define FLASH1_CHIP_JEDEC_ID     0xC2201A
#elif FLASH1_SIZE_MB == 32
#define FLASH1_CHIP_NAME         "MX25L25645GZ2I-08G"
#define FLASH1_CHIP_SIZE_BYTES   33554432UL
#define FLASH1_CHIP_JEDEC_ID     0xC22019
#elif FLASH1_SIZE_MB == 16
#define FLASH1_CHIP_NAME         "MX25L12845GZ2I-08G"
#define FLASH1_CHIP_SIZE_BYTES   16777216UL
#define FLASH1_CHIP_JEDEC_ID     0xC22018
#else
#error "FLASH1_SIZE_MB must be 64, 32, or 16"
#endif

#if FLASH2_SIZE_MB == 64
#define FLASH2_CHIP_NAME         "MX25L51245GZ2I-08G"
#define FLASH2_CHIP_SIZE_BYTES   67108864UL
#define FLASH2_CHIP_JEDEC_ID     0xC2201A
#elif FLASH2_SIZE_MB == 32
#define FLASH2_CHIP_NAME         "MX25L25645GZ2I-08G"
#define FLASH2_CHIP_SIZE_BYTES   33554432UL
#define FLASH2_CHIP_JEDEC_ID     0xC22019
#elif FLASH2_SIZE_MB == 16
#define FLASH2_CHIP_NAME         "MX25L12845GZ2I-08G"
#define FLASH2_CHIP_SIZE_BYTES   16777216UL
#define FLASH2_CHIP_JEDEC_ID     0xC22018
#else
#error "FLASH2_SIZE_MB must be 64, 32, or 16"
#endif

/* Common flash parameters */
#define FLASH_PAGE_SIZE          256
#define FLASH_SECTOR_SIZE        4096
#define FLASH_BLOCK_SIZE_32K     32768
#define FLASH_BLOCK_SIZE_64K     65536

/* Flash selection for operations */
typedef enum {
    FLASH1 = 0,    /* SPIFX interface (SPI1 in QSPI mode) - default 16MB */
    FLASH2 = 1     /* QSPI interface (hardware QSPI) - default 64MB */
} flash_device_t;

/* Public API Functions */
int nor_flash_system_init(void);
int nor_flash_basic_init(void);
int nor_flash_basic_test(void);

/* File operations - specify which flash device to use */
int nor_flash_write_file(flash_device_t device, const char *filename, const void *data, size_t len);
int nor_flash_read_file(flash_device_t device, const char *filename, void *buffer, size_t len);
int nor_flash_write_struct(flash_device_t device, const char *filename, const void *data, size_t size);
int nor_flash_read_struct(flash_device_t device, const char *filename, void *buffer, size_t size);

/* Get flash device info */
const char* nor_flash_get_device_name(flash_device_t device);
uint32_t nor_flash_get_device_size(flash_device_t device);

#ifdef __cplusplus
}
#endif

#endif /* NOR_FLASH_H */