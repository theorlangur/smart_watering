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


#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor/aem13921.h>

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

LOG_MODULE_REGISTER(main_log, LOG_LEVEL_INF);


/**********************************************************************/
/* For testing purposes                                               */
/**********************************************************************/

static const struct gpio_dt_spec btn0 = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
static const struct gpio_dt_spec btn1 = GPIO_DT_SPEC_GET(DT_NODELABEL(button1), gpios);
static const struct gpio_dt_spec btn2 = GPIO_DT_SPEC_GET(DT_NODELABEL(button2), gpios);

static struct gpio_callback btn0_cb_data;
static struct k_work test_measure_soil_work;

void measure_soil_handler(soil::soil_result_t *pResults, size_t count, uint32_t mask, void *pUserArg)
{
    for(size_t i = 0; i < count; ++i)
    {
	if (mask & (1 << i))
	{
	    auto &r = pResults[i];
	    if (r)
	    {
		LOG_INF("soil%d: %.2f%%", (int)i, double(*r));
	    }else
	    {
		LOG_INF("soil%d: %d (%s)", (int)i, r.error(), strerror(r.error()));
	    }
	}
    }
}

static void btn0_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    /* Defer PM state change out of interrupt context */
    soil::run_measurements({.soil_sensors_mask = 0, .callback = measure_soil_handler});
}

int main(void)
{
    //static_assert(atomic_state_t::is_always_lock_free);
    //disable_hw_wdt();

    //usb_uart::setup();
    LOG_INF("aem13921 test: %d", aem13921_hello());
    soil::setup_task();
    zb::setup();

    int err = settings_subsys_init();

    err = gpio_pin_configure_dt(&btn0, GPIO_INPUT);
    if (err < 0) 
	LOG_INF("btn0 cfg: %d (%s)", err, strerror(err));
    err = gpio_pin_interrupt_configure_dt(&btn0, GPIO_INT_LEVEL_INACTIVE);
    if (err < 0)
	LOG_INF("btn0 int cfg: %d (%s)", err, strerror(err));
    gpio_init_callback(&btn0_cb_data, btn0_isr, BIT(btn0.pin));
    err = gpio_add_callback(btn0.port, &btn0_cb_data);
    if (err < 0)
	LOG_INF("btn0 cb: %d (%s)", err, strerror(err));

    printk("main\r\n");

    while (1) {
	printk("Main: sleep forever\r\n");
	k_msleep(1000);
    }

    return 0;
}

