/*
 * Generic NOR Flash Driver with LittleFS Support - Dual Flash Implementation
 * Flash1: SPIFX (SPI1 in QSPI mode) - Default 16MB
 * Flash2: QSPI (Hardware QSPI) - Default 64MB
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <errno.h>
#include "../LittleFS/lfs.h"
#include "nor_flash.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

LOG_MODULE_REGISTER(nor_flash, LOG_LEVEL_INF);

/* MX25L Commands */
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

/* Device Structure */
struct flash_dev {
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
static struct flash_dev flash1;  /* SPIFX */
static struct flash_dev flash2;  /* QSPI */

/* LittleFS instances */
static lfs_t lfs1, lfs2;
static uint8_t lfs1_read_buf[FLASH_PAGE_SIZE], lfs1_prog_buf[FLASH_PAGE_SIZE], lfs1_look_buf[256];
static uint8_t lfs2_read_buf[FLASH_PAGE_SIZE], lfs2_prog_buf[FLASH_PAGE_SIZE], lfs2_look_buf[256];

/* Forward declarations */
static int flash_transceive(struct flash_dev *dev, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len);
static int flash_wait_ready(struct flash_dev *dev);
static int flash_read_status(struct flash_dev *dev, uint8_t *status);
static int flash_write_enable(struct flash_dev *dev);
static int flash_read_id(struct flash_dev *dev, uint8_t *id);

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

/* SPI Transaction */
static int flash_transceive(struct flash_dev *dev, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
    struct spi_buf tx_buf = {.buf = (void *)tx, .len = tx_len};
    struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1};
    struct spi_buf rx_buf = {.buf = rx, .len = rx_len};
    struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1};
    
    gpio_pin_set(dev->gpio_dev, dev->cs_pin, 0);
    k_usleep(1);
    int ret = spi_transceive(dev->spi_dev, &dev->spi_cfg, &tx_set, rx_len ? &rx_set : NULL);
    k_usleep(1);
    gpio_pin_set(dev->gpio_dev, dev->cs_pin, 1);
    return ret;
}

static int flash_wait_ready(struct flash_dev *dev)
{
    uint8_t status;
    for (int i = 0; i < 1000; i++) {
        if (flash_read_status(dev, &status) != 0) return -EIO;
        if (!(status & SR_BUSY)) return 0;
        k_msleep(1);
    }
    return -ETIMEDOUT;
}

static int flash_read_status(struct flash_dev *dev, uint8_t *status)
{
    uint8_t tx[2] = {CMD_READ_STATUS, 0}, rx[2];
    int ret = flash_transceive(dev, tx, 2, rx, 2);
    if (ret == 0) *status = rx[1];
    return ret;
}

static int flash_write_enable(struct flash_dev *dev)
{
    uint8_t tx[1] = {CMD_WRITE_ENABLE};
    return flash_transceive(dev, tx, 1, NULL, 0);
}

static int flash_read_id(struct flash_dev *dev, uint8_t *id)
{
    uint8_t tx[4] = {CMD_JEDEC_ID, 0, 0, 0}, rx[4];
    int ret = flash_transceive(dev, tx, 4, rx, 4);
    if (ret == 0) { id[0] = rx[1]; id[1] = rx[2]; id[2] = rx[3]; }
    return ret;
}

/* Generic flash read/prog/erase helpers */
static int flash_read_data(struct flash_dev *dev, uint32_t addr, void *buf, size_t size)
{
    uint8_t cmd[4 + 512];
    if (size > 512) return -EINVAL;
    cmd[0] = CMD_READ_DATA;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = addr & 0xFF;
    memset(cmd + 4, 0xFF, size);
    uint8_t rx[4 + 512];
    int ret = flash_transceive(dev, cmd, 4 + size, rx, 4 + size);
    if (ret == 0) memcpy(buf, rx + 4, size);
    return ret;
}

static int flash_prog_data(struct flash_dev *dev, uint32_t addr, const void *buf, size_t size)
{
    const uint8_t *data = buf;
    while (size > 0) {
        uint32_t page_off = addr % FLASH_PAGE_SIZE;
        size_t write_size = MIN(size, FLASH_PAGE_SIZE - page_off);
        
        if (flash_write_enable(dev) != 0) return -EIO;
        
        uint8_t cmd[4 + 256];
        cmd[0] = CMD_PAGE_PROGRAM;
        cmd[1] = (addr >> 16) & 0xFF;
        cmd[2] = (addr >> 8) & 0xFF;
        cmd[3] = addr & 0xFF;
        memcpy(cmd + 4, data, write_size);
        
        if (flash_transceive(dev, cmd, 4 + write_size, NULL, 0) != 0) return -EIO;
        if (flash_wait_ready(dev) != 0) return -EIO;
        
        addr += write_size;
        data += write_size;
        size -= write_size;
    }
    return 0;
}

static int flash_erase_sector(struct flash_dev *dev, uint32_t addr)
{
    if (flash_write_enable(dev) != 0) return -EIO;
    uint8_t cmd[4] = {CMD_SECTOR_ERASE, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF};
    if (flash_transceive(dev, cmd, 4, NULL, 0) != 0) return -EIO;
    return flash_wait_ready(dev);
}

/* LittleFS callbacks for FLASH1 */
static int lfs1_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buf, lfs_size_t size)
{
    uint32_t addr = (block * FLASH_SECTOR_SIZE) + off;
    return flash_read_data(&flash1, addr, buf, size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs1_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buf, lfs_size_t size)
{
    uint32_t addr = (block * FLASH_SECTOR_SIZE) + off;
    return flash_prog_data(&flash1, addr, buf, size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs1_erase(const struct lfs_config *c, lfs_block_t block)
{
    uint32_t addr = block * FLASH_SECTOR_SIZE;
    return flash_erase_sector(&flash1, addr) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs1_sync(const struct lfs_config *c) { return LFS_ERR_OK; }

/* LittleFS callbacks for FLASH2 */
static int lfs2_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buf, lfs_size_t size)
{
    uint32_t addr = (block * FLASH_SECTOR_SIZE) + off;
    return flash_read_data(&flash2, addr, buf, size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs2_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buf, lfs_size_t size)
{
    uint32_t addr = (block * FLASH_SECTOR_SIZE) + off;
    return flash_prog_data(&flash2, addr, buf, size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs2_erase(const struct lfs_config *c, lfs_block_t block)
{
    uint32_t addr = block * FLASH_SECTOR_SIZE;
    return flash_erase_sector(&flash2, addr) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs2_sync(const struct lfs_config *c) { return LFS_ERR_OK; }

/* Initialize flash device */
static int flash_init_dev(struct flash_dev *dev, const char *spi_label, gpio_pin_t cs_pin,
                          const char *name, uint32_t size, uint32_t jedec)
{
    dev->name = name;
    dev->size_bytes = size;
    dev->jedec_id = jedec;
    dev->cs_pin = cs_pin;
    
    dev->spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi1));
    if (!device_is_ready(dev->spi_dev)) {
        LOG_ERR("%s: SPI device not ready", name);
        return -ENODEV;
    }
    
    dev->spi_cfg.frequency = 8000000;
    dev->spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB;
    dev->spi_cfg.slave = 0;
    
    dev->gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(dev->gpio_dev)) {
        LOG_ERR("%s: GPIO device not ready", name);
        return -ENODEV;
    }
    
    gpio_pin_configure(dev->gpio_dev, dev->cs_pin, GPIO_OUTPUT);
    gpio_pin_set(dev->gpio_dev, dev->cs_pin, 1);
    k_msleep(10);
    
    uint8_t wakeup[2] = {CMD_RELEASE_PD, 0};
    flash_transceive(dev, wakeup, 2, NULL, 0);
    k_msleep(10);
    
    uint8_t id[3];
    if (flash_read_id(dev, id) != 0) {
        LOG_ERR("%s: Failed to read ID", name);
        return -EIO;
    }
    
    LOG_INF("%s: ID=%02X %02X %02X", name, id[0], id[1], id[2]);
    
    uint32_t read_jedec = (id[0] << 16) | (id[1] << 8) | id[2];
    if (read_jedec != jedec) {
        LOG_WRN("%s: ID mismatch! Expected %06X, got %06X", name, jedec, read_jedec);
        if (id[0] != 0xC2) {
            LOG_ERR("%s: Not a Macronix chip", name);
            return -ENODEV;
        }
    }
    
    dev->initialized = true;
    LOG_INF("%s: Initialized (%d MB)", name, size / (1024*1024));
    return 0;
}

/* Initialize LittleFS on a flash */
static int lfs_init(lfs_t *lfs, struct lfs_config *cfg, const char *name)
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

/* Public API */
int nor_flash_system_init(void)
{
    LOG_INF("Initializing dual NOR flash system...");
    LOG_INF("FLASH1 (SPIFX): %s (%d MB)", FLASH1_CHIP_NAME, FLASH1_SIZE_MB);
    LOG_INF("FLASH2 (QSPI): %s (%d MB)", FLASH2_CHIP_NAME, FLASH2_SIZE_MB);
    
    /* Init FLASH1 (SPIFX - SPI1) */
    if (flash_init_dev(&flash1, "spi1", 4, FLASH1_CHIP_NAME, 
                       FLASH1_CHIP_SIZE_BYTES, FLASH1_CHIP_JEDEC_ID) != 0) {
        LOG_ERR("FLASH1 init failed");
        return -EIO;
    }
    
    /* Init FLASH2 (QSPI - will use SPI1 for now until QSPI driver ready) */
    if (flash_init_dev(&flash2, "spi1", 15, FLASH2_CHIP_NAME,
                       FLASH2_CHIP_SIZE_BYTES, FLASH2_CHIP_JEDEC_ID) != 0) {
        LOG_ERR("FLASH2 init failed");
        return -EIO;
    }
    
    /* Setup LittleFS configs */
    lfs_cfg1.read_buffer = lfs1_read_buf;
    lfs_cfg1.prog_buffer = lfs1_prog_buf;
    lfs_cfg1.lookahead_buffer = lfs1_look_buf;
    
    lfs_cfg2.read_buffer = lfs2_read_buf;
    lfs_cfg2.prog_buffer = lfs2_prog_buf;
    lfs_cfg2.lookahead_buffer = lfs2_look_buf;
    
    /* Init filesystems */
    if (lfs_init(&lfs1, &lfs_cfg1, "FLASH1") != 0) return -EIO;
    if (lfs_init(&lfs2, &lfs_cfg2, "FLASH2") != 0) return -EIO;
    
    LOG_INF("Dual flash system ready - Total: %d MB", FLASH1_SIZE_MB + FLASH2_SIZE_MB);
    return 0;
}

/* File operations */
int nor_flash_write_file(flash_device_t device, const char *filename, const void *data, size_t len)
{
    lfs_t *lfs = (device == FLASH1) ? &lfs1 : &lfs2;
    lfs_file_t file;
    
    int ret = lfs_file_open(lfs, &file, filename, LFS_O_WRONLY | LFS_O_CREAT);
    if (ret < 0) return ret;
    
    ret = lfs_file_write(lfs, &file, data, len);
    if (ret < 0) {
        lfs_file_close(lfs, &file);
        return ret;
    }
    
    ret = lfs_file_close(lfs, &file);
    LOG_INF("FLASH%d: Wrote %s (%zu bytes)", device + 1, filename, len);
    return ret < 0 ? ret : 0;
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
    return ret;
}

const char* nor_flash_get_device_name(flash_device_t device)
{
    return (device == FLASH1) ? flash1.name : flash2.name;
}

uint32_t nor_flash_get_device_size(flash_device_t device)
{
    return (device == FLASH1) ? flash1.size_bytes : flash2.size_bytes;
}

int nor_flash_basic_init(void) { return nor_flash_system_init(); }
int nor_flash_basic_test(void) { return 0; }
