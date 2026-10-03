/*
 * Copyright (c) 2023, Meta
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define FORCE_FMT
#define PRINTF_FUNC(...) printk(__VA_ARGS__)

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/settings/settings.h>
//#include <dk_buttons_and_leds.h>
//#include <nrf_general/led.h>

//#include <zephyr/drivers/sensor.h>
//#include <zephyr/drivers/sensor/ens160.h>

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/storage/flash_map.h>
#include "usb_uart.hpp"
#include "threads_snapshot.hpp"
#include "partition_basic.hpp"
#include "soil_task.hpp"
#include "zigbee.hpp"

#include <atomic>

//extern "C"{
//#include <zephyr/debug/coredump.h>
//
//void nrf_flash_skip_sync(bool skip);
//}

constexpr bool kDebug = false;

#define BREADCRUMBS_PARTITION		breadcrumbs_partition
#define BREADCRUMBS_PARTITION_ID	FIXED_PARTITION_ID(BREADCRUMBS_PARTITION)

//#if !FIXED_PARTITION_EXISTS(BREADCRUMBS_PARTITION)
//#error "Need a fixed partition named 'breadcrumbs-partition'!"
//#endif
//
///* Note that currently external memories are not supported */
//#define FLASH_CONTROLLER	\
//	DT_PARENT(DT_PARENT(DT_NODELABEL(BREADCRUMBS_PARTITION)))
//
//#define FLASH_WRITE_SIZE	DT_PROP(FLASH_CONTROLLER, write_block_size)
//#define FLASH_BUF_SIZE \
//	MAX(FLASH_WRITE_SIZE, ROUND_UP(CONFIG_DEBUG_COREDUMP_FLASH_CHUNK_SIZE, FLASH_WRITE_SIZE))
//#if DT_NODE_HAS_PROP(FLASH_CONTROLLER, erase_block_size)
//#define DEVICE_ERASE_BLOCK_SIZE DT_PROP(FLASH_CONTROLLER, erase_block_size)
//#else
///* Device has no erase block size */
//#define DEVICE_ERASE_BLOCK_SIZE 1
//#endif


int main(void)
{
    //static_assert(atomic_state_t::is_always_lock_free);
    //disable_hw_wdt();

    //usb_uart::setup();
    soil::setup_task();
    zb::setup();

    int err = settings_subsys_init();

    printk("main\r\n");

    while (1) {
	printk("Main: sleep forever\r\n");
	k_msleep(1000);
    }

    return 0;
}

