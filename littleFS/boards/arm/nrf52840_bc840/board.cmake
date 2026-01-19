# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

board_runner_args(jlink "--device=nrf52840_xxaa" "--speed=4000")
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
