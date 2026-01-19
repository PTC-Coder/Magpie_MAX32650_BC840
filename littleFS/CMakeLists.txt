# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(littlefs)

target_sources(app PRIVATE 
    src/main.c
    src/nor_flash.c
    src/ds3231.c
    LittleFS/lfs.c
    LittleFS/lfs_util.c
)

# Add include directories
target_include_directories(app PRIVATE
    LittleFS
    src
)

# Dual flash configuration
# Flash1: SPIFX interface (SPI1 in QSPI mode) - default 16MB
# Flash2: QSPI interface (hardware QSPI peripheral) - default 64MB
#
# Configure flash sizes (options: 64, 32, or 16 MB)
# Uncomment and modify to change from defaults:
#
# target_compile_definitions(app PRIVATE FLASH1_SIZE_MB=16)  # Default: 16MB
# target_compile_definitions(app PRIVATE FLASH2_SIZE_MB=64)  # Default: 64MB
#
# Examples:
# - Both 64MB: FLASH1_SIZE_MB=64 FLASH2_SIZE_MB=64
# - Both 32MB: FLASH1_SIZE_MB=32 FLASH2_SIZE_MB=32
# - Mixed:     FLASH1_SIZE_MB=16 FLASH2_SIZE_MB=64 (default)

# Default configuration (16MB + 64MB = 80MB total)
# No definitions needed - uses defaults from nor_flash.h

# Configure LittleFS with basic settings
target_compile_definitions(app PRIVATE
    LFS_NO_DEBUG
    LFS_NO_WARN
    LFS_NO_ERROR
)
