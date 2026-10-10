/*
 * Copyright (c) 2023, Meta
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lib_misc_helpers.hpp"
#define FORCE_FMT
#define PRINTF_FUNC(...) printk(__VA_ARGS__)

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
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
#include <zephyr/drivers/sensor/aem13921.hpp>

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

static const struct device *const aem13921_dev = DEVICE_DT_GET(DT_NODELABEL(aem13921));

void dbg_aem13921()
{
    LOG_INF("<================AEM13921=================>");
    {
	auto irq = epeas::aem1392x_get_irq_state(aem13921_dev);
	if (irq)
	{
	    LOG_INF("irq: 0x%X 0x%X", (*irq).bytes[0], (*irq).bytes[1]);
	}else
	{
	    LOG_ERR("irq: failed with %d %s", irq.error(), strerror(irq.error()));
	}
    }
    {
	auto status = epeas::aem1392x_get_status(aem13921_dev);
	if (status)
	{
	    LOG_INF("status: 0x%X 0x%X", (*status).bytes[0], (*status).bytes[1]);
	}else
	{
	    LOG_ERR("status: failed with %d %s", status.error(), strerror(status.error()));
	}
    }

    {
	auto apm_err = epeas::aem1392x_get_apm_error(aem13921_dev);
	if (apm_err)
	{
	    LOG_INF("apm error: 0x%X", apm_err->byte);
	}
	else
	{
	    LOG_ERR("apm error: failed with %d %s", apm_err.error(), strerror(apm_err.error()));
	}
    }

    {
	auto apm_5v = epeas::aem1392x_get_apm_ch5v(aem13921_dev);
	if (apm_5v)
	{
	    LOG_INF("apm 5v: %d", apm_5v->value);
	}
	else
	{
	    LOG_ERR("apm 5v: failed with %d %s", apm_5v.error(), strerror(apm_5v.error()));
	}
    }
    {
	auto apm_src1 = epeas::aem1392x_get_apm_src1(aem13921_dev);
	if (apm_src1)
	{
	    std::visit(
		overloaded{
		    [](const epeas::apm_counter_no_acc_t &v)
		    {
			LOG_INF("apm src1 cnt no acc: %d", v.value.counter);
		    },
		    [](const epeas::apm_counter_acc_t &v)
		    {
			LOG_INF("apm src1 cnt acc: %d:%d", v.value.offset, v.value.counter);
		    },
		    [](const epeas::apm_power_meter_acc_t &v)
		    {
			LOG_INF("apm src1 power: %d:%d", v.value.offset, v.value.power);
		    }
		},
		*apm_src1
	    );
	}
	else
	{
	    LOG_ERR("apm src1: failed with %d %s", apm_src1.error(), strerror(apm_src1.error()));
	}
    }
    {
	auto apm_src2 = epeas::aem1392x_get_apm_src2(aem13921_dev);
	if (apm_src2)
	{
	    std::visit(
		overloaded{
		    [](const epeas::apm_counter_no_acc_t &v)
		    {
			LOG_INF("apm src2 cnt no acc: %d", v.value.counter);
		    },
		    [](const epeas::apm_counter_acc_t &v)
		    {
			LOG_INF("apm src2 cnt acc: %d:%d", v.value.offset, v.value.counter);
		    },
		    [](const epeas::apm_power_meter_acc_t &v)
		    {
			LOG_INF("apm src2 power: %d:%d", v.value.offset, v.value.power);
		    }
		},
		*apm_src2
	    );
	}
	else
	{
	    LOG_ERR("apm src2: failed with %d %s", apm_src2.error(), strerror(apm_src2.error()));
	}
    }
    {
	auto apm_load = epeas::aem1392x_get_apm_load(aem13921_dev);
	if (apm_load)
	{
	    std::visit(
		overloaded{
		    [](const epeas::apm_counter_no_acc_t &v)
		    {
			LOG_INF("apm load cnt no acc: %d", v.value.counter);
		    },
		    [](const epeas::apm_counter_acc_t &v)
		    {
			LOG_INF("apm load cnt acc: %d:%d", v.value.offset, v.value.counter);
		    },
		    [](const epeas::apm_power_meter_acc_t &v)
		    {
			LOG_INF("apm load power: %d:%d", v.value.offset, v.value.power);
		    }
		},
		*apm_load
	    );
	}
	else
	{
	    LOG_ERR("apm load: failed with %d %s", apm_load.error(), strerror(apm_load.error()));
	}
    }
    {
	auto storage_v = epeas::aem1392x_get_storage_voltage(aem13921_dev);
	if (storage_v)
	{
	    LOG_INF("storage v: %.3f", *storage_v);
	}
	else
	{
	    LOG_ERR("storage v: failed with %d %s", storage_v.error(), strerror(storage_v.error()));
	}
    }
    {
	auto src1_v = epeas::aem1392x_get_src_voltage(aem13921_dev, epeas::src_t::_1);
	if (src1_v)
	{
	    LOG_INF("src1 v: %.3f", *src1_v);
	}
	else
	{
	    LOG_ERR("src1 v: failed with %d %s", src1_v.error(), strerror(src1_v.error()));
	}
    }
    {
	auto src2_v = epeas::aem1392x_get_src_voltage(aem13921_dev, epeas::src_t::_2);
	if (src2_v)
	{
	    LOG_INF("src2 v: %.3f", *src2_v);
	}
	else
	{
	    LOG_ERR("src2 v: failed with %d %s", src2_v.error(), strerror(src2_v.error()));
	}
    }
    LOG_INF("<================AEM13921=================>");
}

int main(void)
{
    //static_assert(atomic_state_t::is_always_lock_free);
    //disable_hw_wdt();

    //usb_uart::setup();
    //LOG_INF("aem13921 test: %d", aem13921_hello());
    soil::setup_task();
    zb::setup();

    int err = settings_subsys_init();
    uint8_t v;
    err = epeas::aem1392x_get_version(aem13921_dev, &v);
    LOG_INF("aem13921 test: %d, %d", err, v);

    //epeas::aem1392x_configure_src_regulation_mppt(aem13921_dev, epeas::src_t::_0, {.ratio = epeas::mppt_ratio_t::_75, .wait = epeas::mppt_wait_t::_116, .period = epeas::mppt_period_t::_14895});
    //epeas::aem1392x_configure_src_regulation_voltage(aem13921_dev, epeas::src_t::_1, 0.5);
    auto res = epeas::aem1392x_get_src_regulation(aem13921_dev, epeas::src_t::_1);
    if (res)
    {
	auto const& v = *res;
	//auto *pmppt = std::get_if<epeas::src_mppt_cfg_t>(&v);
	//v.visit();
    }else
    {
	LOG_INF("aem13921 test@: %d", res.error());
    }


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
	dbg_aem13921();
	k_msleep(1000);
    }

    return 0;
}

