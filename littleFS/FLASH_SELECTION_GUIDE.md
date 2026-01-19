# Flash Selection Guide

## Overview

The dual NOR flash system supports **compile-time configuration** of flash sizes for both interfaces. You can choose any combination of 64MB, 32MB, or 16MB flash chips for either FLASH1 or FLASH2.

## Quick Reference

| Flash | Interface | Default Size | Configurable Sizes | Pins |
|-------|-----------|--------------|-------------------|------|
| **FLASH1** | SPIFX (SPI1) | 16MB | 64MB, 32MB, 16MB | P0.04, P0.07, P0.06, P0.12 |
| **FLASH2** | QSPI (SPI1*) | 64MB | 64MB, 32MB, 16MB | P0.15, P0.19, P1.01, P0.17, P0.25, P0.22 |

*Currently uses SPI1, will use hardware QSPI when driver is ready

## Supported Flash Chips

All three Macronix NOR flash chip sizes are supported on either interface:

| Size | Part Number | JEDEC ID | Capacity |
|------|-------------|----------|----------|
| 64MB | MX25L51245GZ2I-08G | 0xC2201A | 67,108,864 bytes |
| 32MB | MX25L25645GZ2I-08G | 0xC22019 | 33,554,432 bytes |
| 16MB | MX25L12845GZ2I-08G | 0xC22018 | 16,777,216 bytes |

## How to Configure Flash Sizes

### Step 1: Edit CMakeLists.txt

Open `littleFS/CMakeLists.txt` and add compile definitions:

```cmake
# Add these lines after the target_sources section
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=16  # Choose: 64, 32, or 16
    FLASH2_SIZE_MB=64  # Choose: 64, 32, or 16
)
```

### Step 2: Build and Flash

```bash
cd littleFS
west build -b nrf52840_bc840 -p
west flash
```

The system will automatically:
- Configure the correct chip parameters
- Verify the JEDEC ID matches your selection
- Set up LittleFS with the correct block count
- Initialize both flash devices

## Configuration Examples

### Example 1: Default Configuration (80MB Total)
**Use Case:** Standard setup with small config flash and large data flash

```cmake
# No configuration needed - uses defaults from nor_flash.h
# FLASH1 = 16MB (SPIFX)
# FLASH2 = 64MB (QSPI)
# Total = 80MB
```

**When to use:**
- Most common configuration
- FLASH1 for config, settings, logs
- FLASH2 for large data, media, archives

---

### Example 2: Maximum Storage (128MB Total)
**Use Case:** Need maximum storage capacity

```cmake
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=64
    FLASH2_SIZE_MB=64
)
# Total = 128MB
```

**When to use:**
- Large data logging applications
- Media storage
- Firmware update storage
- Maximum capacity needed

**Hardware Required:**
- Two MX25L51245GZ2I-08G (64MB) chips

---

### Example 3: Balanced Configuration (64MB Total)
**Use Case:** Balanced storage with moderate capacity

```cmake
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=32
    FLASH2_SIZE_MB=32
)
# Total = 64MB
```

**When to use:**
- Moderate data storage needs
- Cost-effective solution
- Balanced performance

**Hardware Required:**
- Two MX25L25645GZ2I-08G (32MB) chips

---

### Example 4: Minimal Configuration (32MB Total)
**Use Case:** Cost-sensitive applications with minimal storage needs

```cmake
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=16
    FLASH2_SIZE_MB=16
)
# Total = 32MB
```

**When to use:**
- Small data requirements
- Cost optimization
- Simple applications

**Hardware Required:**
- Two MX25L12845GZ2I-08G (16MB) chips

---

### Example 5: Mixed Configuration (96MB Total)
**Use Case:** Large primary storage with moderate secondary

```cmake
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=32
    FLASH2_SIZE_MB=64
)
# Total = 96MB
```

**When to use:**
- FLASH1 for moderate config/cache
- FLASH2 for large data storage
- Flexible allocation

**Hardware Required:**
- One MX25L25645GZ2I-08G (32MB)
- One MX25L51245GZ2I-08G (64MB)

---

### Example 6: Inverted Configuration (80MB Total)
**Use Case:** Large config/cache with moderate data storage

```cmake
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=64
    FLASH2_SIZE_MB=16
)
# Total = 80MB
```

**When to use:**
- Large configuration database
- Extensive caching needs
- Moderate data storage

**Hardware Required:**
- One MX25L51245GZ2I-08G (64MB)
- One MX25L12845GZ2I-08G (16MB)

## Configuration Matrix

| FLASH1 | FLASH2 | Total | Use Case |
|--------|--------|-------|----------|
| 16MB | 64MB | 80MB | **Default** - Config + Large Data |
| 64MB | 64MB | 128MB | Maximum Storage |
| 32MB | 32MB | 64MB | Balanced |
| 16MB | 16MB | 32MB | Minimal/Cost-Effective |
| 32MB | 64MB | 96MB | Moderate + Large |
| 64MB | 16MB | 80MB | Large Config + Moderate Data |
| 16MB | 32MB | 48MB | Small Config + Moderate Data |
| 64MB | 32MB | 96MB | Large Config + Moderate Data |
| 32MB | 16MB | 48MB | Moderate Config + Small Data |

## Verification

After building and flashing, check the log output:

```
[INF] Initializing dual NOR flash system...
[INF] FLASH1 (SPIFX): MX25L12845GZ2I-08G (16 MB)
[INF] FLASH2 (QSPI): MX25L51245GZ2I-08G (64 MB)
[INF] MX25L12845GZ2I-08G: ID=C2 20 18
[INF] MX25L12845GZ2I-08G: Initialized (16 MB)
[INF] MX25L51245GZ2I-08G: ID=C2 20 1A
[INF] MX25L51245GZ2I-08G: Initialized (64 MB)
[INF] FLASH1: LittleFS mounted
[INF] FLASH2: LittleFS mounted
[INF] Dual flash system ready - Total: 80 MB
```

### Verify JEDEC IDs Match

| Expected Size | Expected JEDEC ID | What to Check |
|---------------|-------------------|---------------|
| 64MB | C2 20 1A | Matches MX25L51245G |
| 32MB | C2 20 19 | Matches MX25L25645G |
| 16MB | C2 20 18 | Matches MX25L12845G |

If the JEDEC ID doesn't match:
- ⚠️ Wrong chip installed
- ⚠️ Wrong size configured in CMakeLists.txt
- ⚠️ Wiring issue

## Troubleshooting

### Issue: JEDEC ID Mismatch
```
[WRN] MX25L12845GZ2I-08G: ID mismatch! Expected C22018, got C2201A
```

**Solution:**
1. Check which physical chip is installed
2. Update CMakeLists.txt to match the physical chip
3. Rebuild and reflash

### Issue: Wrong Total Capacity
```
[INF] Dual flash system ready - Total: 32 MB
```
But you expected 80MB.

**Solution:**
1. Verify CMakeLists.txt has correct definitions
2. Clean build: `west build -b nrf52840_bc840 -p`
3. Check that definitions are not commented out

### Issue: Build Errors
```
error: FLASH1_SIZE_MB must be 64, 32, or 16
```

**Solution:**
- Only use values: 64, 32, or 16
- Check for typos in CMakeLists.txt

## Advanced Configuration

### Using Environment Variables

You can also set flash sizes via environment variables:

```bash
# Linux/Mac
export FLASH1_SIZE_MB=32
export FLASH2_SIZE_MB=64
west build -b nrf52840_bc840 -p

# Windows PowerShell
$env:FLASH1_SIZE_MB=32
$env:FLASH2_SIZE_MB=64
west build -b nrf52840_bc840 -p
```

### Using West Build Arguments

```bash
west build -b nrf52840_bc840 -p -- -DFLASH1_SIZE_MB=32 -DFLASH2_SIZE_MB=64
```

## Best Practices

### 1. Match Physical Hardware
Always configure the software to match your physical chips:
```cmake
# If you have 32MB + 64MB chips installed:
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=32
    FLASH2_SIZE_MB=64
)
```

### 2. Document Your Configuration
Add a comment in CMakeLists.txt:
```cmake
# Hardware: MX25L25645G (32MB) on FLASH1, MX25L51245G (64MB) on FLASH2
target_compile_definitions(app PRIVATE 
    FLASH1_SIZE_MB=32
    FLASH2_SIZE_MB=64
)
```

### 3. Verify After Changes
Always check the log output after changing configuration:
- JEDEC IDs should match expected values
- Total capacity should be correct
- Both filesystems should mount

### 4. Clean Build After Changes
When changing flash sizes, always do a clean build:
```bash
west build -b nrf52840_bc840 -p
```

## Storage Allocation Recommendations

### FLASH1 (SPIFX) - Best For:
- Configuration files
- System settings
- Calibration data
- Small logs
- Frequently accessed data
- Cache

### FLASH2 (QSPI) - Best For:
- Large data files
- Media content (audio, images)
- Data logging
- Firmware updates
- Archives
- Infrequently accessed data

## Summary

**To configure flash sizes:**

1. **Edit** `littleFS/CMakeLists.txt`
2. **Add** compile definitions:
   ```cmake
   target_compile_definitions(app PRIVATE 
       FLASH1_SIZE_MB=16  # or 32, 64
       FLASH2_SIZE_MB=64  # or 32, 16
   )
   ```
3. **Build** with clean: `west build -b nrf52840_bc840 -p`
4. **Flash**: `west flash`
5. **Verify** log output shows correct sizes and JEDEC IDs

**Default if not configured:**
- FLASH1 = 16MB
- FLASH2 = 64MB
- Total = 80MB

**Supported sizes:** 64MB, 32MB, 16MB (any combination)

---

**Quick Start:** For most applications, the default configuration (16MB + 64MB) is recommended. Only change if you have specific storage requirements or different physical chips installed.
