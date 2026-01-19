# Flash Naming Reference

## Flash Device Naming

| Name | Interface | Hardware | Default Size | Pins |
|------|-----------|----------|--------------|------|
| **FLASH1** | SPIFX | SPI1 in QSPI mode | 16MB | P0.04, P0.07, P0.06, P0.12, P0.08, P1.09 |
| **FLASH2** | QSPI | Hardware QSPI | 64MB | P0.15, P1.01, P0.17, P0.19, P0.22, P0.25 |

## Pin Mappings

### FLASH1 (SPIFX - SPI1 in QSPI mode)
| Pin | Function | Description |
|-----|----------|-------------|
| P0.04 | CS | Chip Select |
| P0.07 | SCK | Clock |
| P0.06 | IO1 | Data line 1 (MOSI) |
| P0.12 | IO0 | Data line 0 (MISO) |
| P0.08 | IO2 | Data line 2 |
| P1.09 | IO3 | Data line 3 |

### FLASH2 (QSPI - Hardware QSPI)
| Pin | Function | Description |
|-----|----------|-------------|
| P0.15 | CS | Chip Select |
| P0.19 | SCK | Clock |
| P1.01 | IO0 | Data line 0 |
| P0.17 | IO1 | Data line 1 |
| P0.25 | IO2 | Data line 2 |
| P0.22 | IO3 | Data line 3 |

## Supported Flash Chips

All three sizes are supported on either interface:

| Size | Chip Model | JEDEC ID |
|------|------------|----------|
| 64MB | MX25L51245GZ2I-08G | 0xC2201A |
| 32MB | MX25L25645GZ2I-08G | 0xC22019 |
| 16MB | MX25L12845GZ2I-08G | 0xC22018 |

## Configuration

### Default Configuration
```cmake
# In CMakeLists.txt - uses defaults from nor_flash.h
# FLASH1 (SPIFX) = 16MB
# FLASH2 (QSPI) = 64MB
# Total: 80MB
```

### Custom Configuration Examples

#### Both 64MB (160MB total)
```cmake
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=64
    FLASH2_SIZE_MB=64
)
```

#### Both 32MB (64MB total)
```cmake
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=32
    FLASH2_SIZE_MB=32
)
```

#### Both 16MB (32MB total)
```cmake
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=16
    FLASH2_SIZE_MB=16
)
```

#### Mixed Sizes
```cmake
# FLASH1 = 32MB, FLASH2 = 64MB (96MB total)
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=32
    FLASH2_SIZE_MB=64
)
```

## API Usage

### Using FLASH1 (SPIFX - 16MB default)
```c
// Write to FLASH1
nor_flash_write_file(FLASH1, "config.txt", data, len);

// Read from FLASH1
nor_flash_read_file(FLASH1, "config.txt", buffer, len);

// Struct operations
nor_flash_write_struct(FLASH1, "settings.bin", &settings, sizeof(settings));
nor_flash_read_struct(FLASH1, "settings.bin", &settings, sizeof(settings));
```

### Using FLASH2 (QSPI - 64MB default)
```c
// Write to FLASH2
nor_flash_write_file(FLASH2, "large_data.bin", data, size);

// Read from FLASH2
nor_flash_read_file(FLASH2, "large_data.bin", buffer, size);

// Struct operations
nor_flash_write_struct(FLASH2, "archive.bin", &archive, sizeof(archive));
nor_flash_read_struct(FLASH2, "archive.bin", &archive, sizeof(archive));
```

### Get Device Information
```c
// Get device names
const char* name1 = nor_flash_get_device_name(FLASH1);  // "MX25L12845GZ2I-08G"
const char* name2 = nor_flash_get_device_name(FLASH2);  // "MX25L51245GZ2I-08G"

// Get device sizes
uint32_t size1 = nor_flash_get_device_size(FLASH1);  // 16777216 bytes (16MB)
uint32_t size2 = nor_flash_get_device_size(FLASH2);  // 67108864 bytes (64MB)
```

## Use Case Recommendations

### FLASH1 (SPIFX - 16MB)
**Best for:**
- Configuration files
- System settings
- Calibration data
- Small logs
- Frequently accessed data

**Characteristics:**
- Software QSPI mode (slower than hardware)
- Lower capacity
- Good for small files

### FLASH2 (QSPI - 64MB)
**Best for:**
- Large data files
- Media content (audio, images)
- Data logging
- Firmware updates
- Archives

**Characteristics:**
- Hardware QSPI (faster)
- Higher capacity
- Optimal for large files

## Summary

- **FLASH1** = SPIFX (SPI1 in QSPI mode) - Default 16MB
- **FLASH2** = QSPI (Hardware QSPI) - Default 64MB
- **Total Default Storage** = 80MB
- **Configurable** = Any size (64/32/16 MB) on either flash
- **Independent** = Each flash has its own filesystem

---

**Quick Reference:**
- Small files, config → Use FLASH1
- Large files, data → Use FLASH2
- Configure sizes in CMakeLists.txt
- Both flashes available simultaneously
