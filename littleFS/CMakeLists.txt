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

target_include_directories(app PRIVATE LittleFS src)

target_compile_definitions(app PRIVATE
    LFS_NO_DEBUG
    LFS_NO_WARN
    LFS_NO_ERROR
)
