/*
 * Generic NOR Flash Driver with LittleFS Support - Dual Flash Implementation
 * Flash1: SPIFX (SPI1) - Custom SPI driver - Default 16MB
 * Flash2: QSPI (Hardware QSPI) - Zephyr flash API - Default 64MB
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "../LittleFS/lfs.h"
#include "nor_flash.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

LOG_MODULE_REGISTER(nor_flash, LOG_LEVEL_INF);

/* MX25L Commands (for Flash1 SPI) */
#define CMD_READ_DATA        0x03
#define CMD_PAGE_PROGRAM     0x02
#define CMD_SECTOR_ERASE     0x20
#define CMD_WRITE_ENABLE     0x06
#define CMD_READ_STATUS      0x05
#define CMD_JEDEC_ID         0x9F
#define CMD_RELEASE_PD       0xAB

/* Status Register Bits */
#define SR_BUSY              BIT(0)
#define SR_WEL               BIT(1)

/* Flash1 Device Structure (SPI) */
struct flash1_dev {
    const struct device *spi_dev;
    struct spi_config spi_cfg;
    const struct device *gpio_dev;
    gpio_pin_t cs_pin;
    bool initialized;
    const char *name;
    uint32_t size_bytes;
    uint32_t jedec_id;
};

/* Global instances */
static struct flash1_dev flash1;                    /* SPIFX - custom SPI */
static const struct device *flash2_dev;             /* QSPI - Zephyr flash API */
static bool flash2_initialized = false;

/* LittleFS instances */
static lfs_t lfs1, lfs2;
static uint8_t lfs1_read_buf[FLASH_PAGE_SIZE], lfs1_prog_buf[FLASH_PAGE_SIZE];
static uint8_t lfs2_read_buf[FLASH_PAGE_SIZE], lfs2_prog_buf[FLASH_PAGE_SIZE];
static uint8_t __aligned(4) lfs1_look_buf[256];
static uint8_t __aligned(4) lfs2_look_buf[256];

/* Forward declarations - Flash1 SPI functions */
static int flash1_transceive(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len);
static int flash1_wait_ready(void);
static int flash1_read_status(uint8_t *status);
static int flash1_write_enable(void);
static int flash1_read_id(uint8_t *id);
static int flash1_read_data(uint32_t addr, void *buf, size_t size);
static int flash1_prog_data(uint32_t addr, const void *buf, size_t size);
static int flash1_erase_sector(uint32_t addr);

/* LittleFS callbacks */
static int lfs1_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buf, lfs_size_t size);
static int lfs1_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buf, lfs_size_t size);
static int lfs1_erase(const struct lfs_config *c, lfs_block_t block);
static int lfs1_sync(const struct lfs_config *c);
static int lfs2_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buf, lfs_size_t size);
static int lfs2_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buf, lfs_size_t size);
static int lfs2_erase(const struct lfs_config *c, lfs_block_t block);
static int lfs2_sync(const struct lfs_config *c);

/* LittleFS configs */
static struct lfs_config lfs_cfg1 = {
    .read = lfs1_read, .prog = lfs1_prog, .erase = lfs1_erase, .sync = lfs1_sync,
    .block_size = FLASH_SECTOR_SIZE, .block_count = FLASH1_CHIP_SIZE_BYTES / FLASH_SECTOR_SIZE,
    .cache_size = FLASH_PAGE_SIZE, .lookahead_size = 256, .block_cycles = 100000,
    .read_size = FLASH_PAGE_SIZE, .prog_size = FLASH_PAGE_SIZE,
};

static struct lfs_config lfs_cfg2 = {
    .read = lfs2_read, .prog = lfs2_prog, .erase = lfs2_erase, .sync = lfs2_sync,
    .block_size = FLASH_SECTOR_SIZE, .block_count = FLASH2_CHIP_SIZE_BYTES / FLASH_SECTOR_SIZE,
    .cache_size = FLASH_PAGE_SIZE, .lookahead_size = 256, .block_cycles = 100000,
    .read_size = FLASH_PAGE_SIZE, .prog_size = FLASH_PAGE_SIZE,
};

/*============================================================================
 * FLASH1 - Custom SPI Driver
 *============================================================================*/

static int flash1_transceive(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
    int ret;
    
    gpio_pin_set(flash1.gpio_dev, flash1.cs_pin, 0);
    k_busy_wait(10);
    
    if (tx_len > 0) {
        struct spi_buf tx_buf = {.buf = (void *)tx, .len = tx_len};
        struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1};
        ret = spi_write(flash1.spi_dev, &flash1.spi_cfg, &tx_set);
        if (ret != 0) {
            LOG_ERR("SPI write failed: %d", ret);
            gpio_pin_set(flash1.gpio_dev, flash1.cs_pin, 1);
            return ret;
        }
    }
    
    if (rx != NULL && rx_len > 0) {
        struct spi_buf rx_buf = {.buf = rx, .len = rx_len};
        struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1};
        ret = spi_read(flash1.spi_dev, &flash1.spi_cfg, &rx_set);
        if (ret != 0) {
            LOG_ERR("SPI read failed: %d", ret);
            gpio_pin_set(flash1.gpio_dev, flash1.cs_pin, 1);
            return ret;
        }
    }
    
    k_busy_wait(10);
    gpio_pin_set(flash1.gpio_dev, flash1.cs_pin, 1);
    return 0;
}

static int flash1_wait_ready(void)
{
    uint8_t status;
    for (int i = 0; i < 1000; i++) {
        if (flash1_read_status(&status) != 0) return -EIO;
        if (!(status & SR_BUSY)) return 0;
        k_msleep(1);
    }
    return -ETIMEDOUT;
}

static int flash1_read_status(uint8_t *status)
{
    uint8_t tx[1] = {CMD_READ_STATUS};
    return flash1_transceive(tx, 1, status, 1);
}

static int flash1_write_enable(void)
{
    uint8_t tx[1] = {CMD_WRITE_ENABLE};
    return flash1_transceive(tx, 1, NULL, 0);
}

static int flash1_read_id(uint8_t *id)
{
    uint8_t tx[1] = {CMD_JEDEC_ID};
    return flash1_transceive(tx, 1, id, 3);
}

static int flash1_read_data(uint32_t addr, void *buf, size_t size)
{
    if (size > 512) return -EINVAL;
    
    uint8_t cmd[4] = {
        CMD_READ_DATA,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    return flash1_transceive(cmd, 4, buf, size);
}

static int flash1_prog_data(uint32_t addr, const void *buf, size_t size)
{
    const uint8_t *data = buf;
    while (size > 0) {
        uint32_t page_off = addr % FLASH_PAGE_SIZE;
        size_t write_size = MIN(size, FLASH_PAGE_SIZE - page_off);
        
        if (flash1_write_enable() != 0) return -EIO;
        
        uint8_t cmd[4 + 256];
        cmd[0] = CMD_PAGE_PROGRAM;
        cmd[1] = (addr >> 16) & 0xFF;
        cmd[2] = (addr >> 8) & 0xFF;
        cmd[3] = addr & 0xFF;
        memcpy(cmd + 4, data, write_size);
        
        if (flash1_transceive(cmd, 4 + write_size, NULL, 0) != 0) return -EIO;
        if (flash1_wait_ready() != 0) return -EIO;
        
        addr += write_size;
        data += write_size;
        size -= write_size;
    }
    return 0;
}

static int flash1_erase_sector(uint32_t addr)
{
    if (flash1_write_enable() != 0) return -EIO;
    uint8_t cmd[4] = {CMD_SECTOR_ERASE, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF};
    if (flash1_transceive(cmd, 4, NULL, 0) != 0) return -EIO;
    return flash1_wait_ready();
}

/* Initialize Flash1 (SPI) */
static int flash1_init(void)
{
    flash1.name = FLASH1_CHIP_NAME;
    flash1.size_bytes = FLASH1_CHIP_SIZE_BYTES;
    flash1.jedec_id = FLASH1_CHIP_JEDEC_ID;
    flash1.cs_pin = 4;  /* P0.04 */
    
    flash1.spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi1));
    if (!device_is_ready(flash1.spi_dev)) {
        LOG_ERR("Flash1: SPI device not ready");
        return -ENODEV;
    }
    
    flash1.gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(flash1.gpio_dev)) {
        LOG_ERR("Flash1: GPIO device not ready");
        return -ENODEV;
    }
    
    flash1.spi_cfg.frequency = 8000000;  /* 8MHz */
    flash1.spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER;
    flash1.spi_cfg.slave = 0;
    
    gpio_pin_configure(flash1.gpio_dev, flash1.cs_pin, GPIO_OUTPUT_HIGH);
    k_msleep(10);
    
    /* Wake from deep power-down */
    uint8_t wakeup[1] = {CMD_RELEASE_PD};
    flash1_transceive(wakeup, 1, NULL, 0);
    k_msleep(1);
    
    uint8_t id[3];
    if (flash1_read_id(id) != 0) {
        LOG_ERR("Flash1: Failed to read ID");
        return -EIO;
    }
    
    LOG_INF("%s: ID=%02X %02X %02X", flash1.name, id[0], id[1], id[2]);
    
    uint32_t read_jedec = (id[0] << 16) | (id[1] << 8) | id[2];
    if (read_jedec != flash1.jedec_id) {
        LOG_WRN("%s: ID mismatch! Expected %06X, got %06X", flash1.name, flash1.jedec_id, read_jedec);
        if (id[0] != 0xC2) {
            LOG_ERR("%s: Not a Macronix chip", flash1.name);
            return -ENODEV;
        }
    }
    
    flash1.initialized = true;
    LOG_INF("%s: Initialized (%d MB)", flash1.name, flash1.size_bytes / (1024*1024));
    return 0;
}

/*============================================================================
 * FLASH2 - Zephyr QSPI Flash API
 *============================================================================*/

static int flash2_init(void)
{
    /* Get the QSPI flash device from devicetree */
    flash2_dev = DEVICE_DT_GET(DT_NODELABEL(mx25l51245g));
    
    if (!device_is_ready(flash2_dev)) {
        LOG_ERR("Flash2: QSPI device not ready");
        return -ENODEV;
    }
    
    /* Zephyr's nordic,qspi-nor driver validates JEDEC ID during init,
     * so if device_is_ready() passes, the chip is verified.
     * Do a simple read test to confirm communication. */
    uint8_t test_buf[4];
    int ret = flash_read(flash2_dev, 0, test_buf, sizeof(test_buf));
    if (ret != 0) {
        LOG_ERR("Flash2: Read test failed (%d)", ret);
        return -EIO;
    }
    
    flash2_initialized = true;
    LOG_INF("%s: Initialized (%d MB) via QSPI", FLASH2_CHIP_NAME, FLASH2_SIZE_MB);
    return 0;
}

/*============================================================================
 * LittleFS Callbacks - Flash1 (SPI)
 *============================================================================*/

static int lfs1_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buf, lfs_size_t size)
{
    uint32_t addr = (block * FLASH_SECTOR_SIZE) + off;
    return flash1_read_data(addr, buf, size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs1_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buf, lfs_size_t size)
{
    uint32_t addr = (block * FLASH_SECTOR_SIZE) + off;
    return flash1_prog_data(addr, buf, size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs1_erase(const struct lfs_config *c, lfs_block_t block)
{
    uint32_t addr = block * FLASH_SECTOR_SIZE;
    return flash1_erase_sector(addr) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs1_sync(const struct lfs_config *c) { return LFS_ERR_OK; }

/*============================================================================
 * LittleFS Callbacks - Flash2 (QSPI via Zephyr API)
 *============================================================================*/

static int lfs2_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buf, lfs_size_t size)
{
    uint32_t addr = (block * FLASH_SECTOR_SIZE) + off;
    int ret = flash_read(flash2_dev, addr, buf, size);
    return (ret == 0) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs2_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buf, lfs_size_t size)
{
    uint32_t addr = (block * FLASH_SECTOR_SIZE) + off;
    int ret = flash_write(flash2_dev, addr, buf, size);
    return (ret == 0) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs2_erase(const struct lfs_config *c, lfs_block_t block)
{
    uint32_t addr = block * FLASH_SECTOR_SIZE;
    int ret = flash_erase(flash2_dev, addr, FLASH_SECTOR_SIZE);
    return (ret == 0) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs2_sync(const struct lfs_config *c) { return LFS_ERR_OK; }

/*============================================================================
 * LittleFS Mount
 *============================================================================*/

static int lfs_init_mount(lfs_t *lfs, struct lfs_config *cfg, const char *name)
{
    int ret = lfs_mount(lfs, cfg);
    if (ret == LFS_ERR_OK) {
        LOG_INF("%s: LittleFS mounted", name);
        return 0;
    }
    
    LOG_WRN("%s: Mount failed (%d), formatting...", name, ret);
    ret = lfs_format(lfs, cfg);
    if (ret != LFS_ERR_OK) {
        LOG_ERR("%s: Format failed (%d)", name, ret);
        return -EIO;
    }
    
    ret = lfs_mount(lfs, cfg);
    if (ret != LFS_ERR_OK) {
        LOG_ERR("%s: Mount after format failed (%d)", name, ret);
        return -EIO;
    }
    
    LOG_INF("%s: LittleFS formatted and mounted", name);
    return 0;
}

/*============================================================================
 * Public API
 *============================================================================*/

int nor_flash_system_init(void)
{
    LOG_INF("Initializing dual NOR flash system...");
    LOG_INF("FLASH1 (SPI): %s (%d MB)", FLASH1_CHIP_NAME, FLASH1_SIZE_MB);
    LOG_INF("FLASH2 (QSPI): %s (%d MB)", FLASH2_CHIP_NAME, FLASH2_SIZE_MB);
    
    /* Init Flash1 (SPI) */
    if (flash1_init() != 0) {
        LOG_ERR("FLASH1 init failed");
        return -EIO;
    }
    
    /* Init Flash2 (QSPI) */
    if (flash2_init() != 0) {
        LOG_ERR("FLASH2 init failed");
        return -EIO;
    }
    
    /* Setup LittleFS buffers */
    lfs_cfg1.read_buffer = lfs1_read_buf;
    lfs_cfg1.prog_buffer = lfs1_prog_buf;
    lfs_cfg1.lookahead_buffer = lfs1_look_buf;
    
    lfs_cfg2.read_buffer = lfs2_read_buf;
    lfs_cfg2.prog_buffer = lfs2_prog_buf;
    lfs_cfg2.lookahead_buffer = lfs2_look_buf;
    
    /* Mount filesystems */
    if (lfs_init_mount(&lfs1, &lfs_cfg1, "FLASH1") != 0) return -EIO;
    if (lfs_init_mount(&lfs2, &lfs_cfg2, "FLASH2") != 0) return -EIO;
    
    LOG_INF("Dual flash system ready - Total: %d MB", FLASH1_SIZE_MB + FLASH2_SIZE_MB);
    return 0;
}

int nor_flash_write_file(flash_device_t device, const char *filename, const void *data, size_t len)
{
    lfs_t *lfs = (device == FLASH1) ? &lfs1 : &lfs2;
    lfs_file_t file;
    
    int ret = lfs_file_open(lfs, &file, filename, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (ret < 0) return ret;
    
    ret = lfs_file_write(lfs, &file, data, len);
    lfs_file_close(lfs, &file);
    
    if (ret >= 0) {
        LOG_INF("FLASH%d: Wrote %s (%zu bytes)", device + 1, filename, len);
    }
    return (ret >= 0) ? 0 : ret;
}

int nor_flash_read_file(flash_device_t device, const char *filename, void *buffer, size_t len)
{
    lfs_t *lfs = (device == FLASH1) ? &lfs1 : &lfs2;
    lfs_file_t file;
    
    int ret = lfs_file_open(lfs, &file, filename, LFS_O_RDONLY);
    if (ret < 0) return ret;
    
    ret = lfs_file_read(lfs, &file, buffer, len);
    lfs_file_close(lfs, &file);
    
    if (ret >= 0) {
        LOG_INF("FLASH%d: Read %s (%d bytes)", device + 1, filename, ret);
    }
    return ret;
}

int nor_flash_write_struct(flash_device_t device, const char *filename, const void *data, size_t size)
{
    return nor_flash_write_file(device, filename, data, size);
}

int nor_flash_read_struct(flash_device_t device, const char *filename, void *buffer, size_t size)
{
    int ret = nor_flash_read_file(device, filename, buffer, size);
    if (ret < 0) return ret;
    if ((size_t)ret != size) {
        LOG_ERR("Size mismatch: expected %zu, got %d", size, ret);
        return -EIO;
    }
    return 0;
}

const char* nor_flash_get_device_name(flash_device_t device)
{
    return (device == FLASH1) ? flash1.name : FLASH2_CHIP_NAME;
}

uint32_t nor_flash_get_device_size(flash_device_t device)
{
    return (device == FLASH1) ? flash1.size_bytes : FLASH2_CHIP_SIZE_BYTES;
}

int nor_flash_get_file_size(flash_device_t device, const char *filename)
{
    lfs_t *lfs = (device == FLASH1) ? &lfs1 : &lfs2;
    struct lfs_info info;
    
    int ret = lfs_stat(lfs, filename, &info);
    if (ret < 0) {
        return ret;  /* File not found or error */
    }
    
    if (info.type != LFS_TYPE_REG) {
        return -EISDIR;  /* Not a regular file */
    }
    
    return (int)info.size;
}

int nor_flash_basic_init(void) { return nor_flash_system_init(); }
int nor_flash_basic_test(void) { return 0; }
