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
#include <dk_buttons_and_leds.h>
#include <nrf_general/led.h>

#include <zephyr/drivers/sensor.h>
//#include <zephyr/drivers/sensor/ens160.h>

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/storage/flash_map.h>
#include "threads_snapshot.hpp"
#include "partition_basic.hpp"
/**********************************************************************/
/* Zigbee                                                             */
/**********************************************************************/
#include <nrfzbmcpp/zbm.hpp>
#include "zb/zb_mem_cfg.hpp"
#include <nrfzbmcpp/zcl/zbm_zcl_basic.hpp>
#include <nrfzbmcpp/zcl/zbm_zcl_on_off.hpp>
#include <nrfzbmcpp/zcl/zbm_zcl_occupancy.hpp>
#include <nrfzbmcpp/zcl/zbm_zcl_rel_humidity.hpp>
#include <nrfzbmcpp/zcl/zbm_zcl_temperature.hpp>
#include <nrfzbmcpp/zcl/zbm_zcl_co2.hpp>
#include <nrfzbmcpp/misc_zc/zbm_misc_zc_air_q.hpp>
#include <nrfzbmcpp/misc_zc/zbm_misc_zc_status.hpp>
#include "zb/zbm_ld2412.hpp"
#include "zb/zbm_dev_ctrl.hpp"

#include <atomic>

extern "C"{
#include <zephyr/debug/coredump.h>

void nrf_flash_skip_sync(bool skip);
}

constexpr bool kDebug = false;

#define BREADCRUMBS_PARTITION		breadcrumbs_partition
#define BREADCRUMBS_PARTITION_ID	FIXED_PARTITION_ID(BREADCRUMBS_PARTITION)

#if !FIXED_PARTITION_EXISTS(BREADCRUMBS_PARTITION)
#error "Need a fixed partition named 'breadcrumbs-partition'!"
#endif

/* Note that currently external memories are not supported */
#define FLASH_CONTROLLER	\
	DT_PARENT(DT_PARENT(DT_NODELABEL(BREADCRUMBS_PARTITION)))

#define FLASH_WRITE_SIZE	DT_PROP(FLASH_CONTROLLER, write_block_size)
#define FLASH_BUF_SIZE \
	MAX(FLASH_WRITE_SIZE, ROUND_UP(CONFIG_DEBUG_COREDUMP_FLASH_CHUNK_SIZE, FLASH_WRITE_SIZE))
#if DT_NODE_HAS_PROP(FLASH_CONTROLLER, erase_block_size)
#define DEVICE_ERASE_BLOCK_SIZE DT_PROP(FLASH_CONTROLLER, erase_block_size)
#else
/* Device has no erase block size */
#define DEVICE_ERASE_BLOCK_SIZE 1
#endif

/**********************************************************************/
/* Zigbee Declarations and Definitions                                */
/**********************************************************************/
static bool g_ZigbeeReady = false;

/* Manufacturer name (32 bytes). */
#define INIT_BASIC_MANUF_NAME      "SFINAE"

/* Model number assigned by manufacturer (32-bytes long string). */
#define INIT_BASIC_MODEL_ID        "SWATER-NG"


/* Button used to enter the Bulb into the Identify mode. */
#define IDENTIFY_MODE_BUTTON            DK_BTN2_MSK

/* Button to start Factory Reset */
#define FACTORY_RESET_BUTTON IDENTIFY_MODE_BUTTON

/* Device endpoint, used to receive light controlling commands. */
constexpr int8_t kTX_POWER = 7;
constexpr uint8_t kMMW_EP = 1;
constexpr uint8_t kMMW_AUX_EP = 2;
constexpr uint16_t kDEV_ID = 0xBAAD;


//struct ep1_t
//{
//    zbm::zcl::basic_names_t basic_attr;
//    zbm::misc_zc::status_t status_attr;
//    zbm::misc_zc::dev_ctrl_t dev_attr;
//    zbm::zcl::occupancy_pir_and_ultrasonic_t occupancy;
//    zbm::zcl::on_off_client_t on_off_client;
//    zbm::misc_zc::ld2412_t ld2412_main;
//    zbm::zcl::rel_humid_basic_t humidity;
//    zbm::zcl::temp_basic_t temperature;
//    zbm::zcl::co2_basic_t co2;
//    zbm::misc_zc::air_q_t airq;
//};

//struct ep2_t
//{
//    zbm::misc_zc::ld2412_t ld2412_aux;
//};

//struct device_ctx_t
//{
//    [[=zbm::ep_a{.ep = kMMW_EP, .dev_id=kDEV_ID, .dev_ver=1}]]
//    ep1_t ep1;
//    [[=zbm::ep_a{.ep = kMMW_AUX_EP, .dev_id=kDEV_ID, .dev_ver=1}]]
//    ep2_t ep2;
//};
//
////attribute shortcuts for template arguments
//
///**********************************************************************/
///* Status attribute shortcuts                                         */
///**********************************************************************/
//constexpr auto kAttrStatus1 = ^^zbm::misc_zc::status_t::status1;
//constexpr auto kAttrStatus2 = ^^zbm::misc_zc::status_t::status2;
//constexpr auto kAttrStatus3 = ^^zbm::misc_zc::status_t::status3;


/* Zigbee device application context storage. */
static constinit device_ctx_t dev_ctx = zbm::const_init_device<device_ctx_t
{
	//   .ep1={
	//.basic_attr = {
	//    {
	//	.zcl_version = ZB_ZCL_VERSION,
	//	.power_source = zbm::zcl::basic_min_t::PowerSource::DC,
	//    },
	//    /*.manufacturer =*/ INIT_BASIC_MANUF_NAME,
	//    /*.model =*/ INIT_BASIC_MODEL_ID,
	//},
	//.status_attr={
	//    .cmd1 = on_cmd_stop_wd_feeding
	//    ,.cmd2 = on_cmd_clear_coredump
	//},
	//.dev_attr={
	//	.cmd_start_analysis_for_presence     = on_cmd_start_analysis_for_presence
	//	,.cmd_start_analysis_for_absense = on_cmd_start_analysis_for_absence
	//	,.cmd_stop_analysis              = on_cmd_stop_analysis
	//},
	//.ld2412_main={
	//    .still_energy_thresholds = zbm::misc_zc::ld2412_t::gate_array_t{100, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15}
	//    ,.move_energy_thresholds = zbm::misc_zc::ld2412_t::gate_array_t{100, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15}
	//    ,.cmd_restart = on_cmd_restart<ld2412_1>
	//    ,.cmd_factory_reset = on_cmd_factory_reset<ld2412_1>
	//    ,.cmd_run_background_analysis = on_cmd_run_back_analysis<ld2412_1>
	//    ,.cmd_take_statistic_snapshot = on_cmd_do_stat_snapshot<ld2412_1>
	//},
	//   },
	//   .ep2={
	//.ld2412_aux={
	//    .still_energy_thresholds = zbm::misc_zc::ld2412_t::gate_array_t{100, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15}
	//    ,.move_energy_thresholds = zbm::misc_zc::ld2412_t::gate_array_t{100, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15}
	//
	//    ,.cmd_restart = on_cmd_restart<ld2412_2>
	//    ,.cmd_factory_reset = on_cmd_factory_reset<ld2412_2>
	//    ,.cmd_run_background_analysis = on_cmd_run_back_analysis<ld2412_2>
	//    ,.cmd_take_statistic_snapshot = on_cmd_do_stat_snapshot<ld2412_2>
	//},
	//   }
}
>{}
;

using zbm_dev_t = zbm::device_full_t<^^dev_ctx>;
constinit static zbm_dev_t zb_ctx{};

//a shortcut for a convenient access
//constinit static auto &zb_ep = zb_ctx.ep<kMMW_EP>();
//constinit static auto &zb_ep_aux = zb_ctx.ep<kMMW_AUX_EP>();

void on_zigbee_start()
{
    printk("on_zigbee_start\r\n");
    g_ZigbeeReady = true;
}

void zb_on_leave()
{
    printk("left the network\r\n");
}

void zb_on_error()
{
    //led::show_pattern(led::kPATTERN_3_BLIPS_NORMED, 1000);
    //printk("error happened\r\n");
}

/**@brief Zigbee stack event handler.
 *
 * @param[in]   bufid   Reference to the Zigbee stack buffer
 *                      used to pass signal.
 */
void zboss_signal_handler(zb_bufid_t bufid)
{
        zb_zdo_app_signal_hdr_t *pHdr;
        auto signalId = zb_get_app_signal(bufid, &pHdr);

	auto ret = zbm::tpl_signal_handler<^^dev_ctx, zb_ctx,
	    zbm::sig_handlers_t{ZB_ZDO_SIGNAL_LEAVE, ^^zb_on_leave},
	    zbm::sig_handlers_t{ZB_ZDO_SIGNAL_ERROR, ^^zb_on_error},
	    zbm::sig_handlers_t{ZB_BDB_SIGNAL_DEVICE_REBOOT, ^^on_zigbee_start},
	    zbm::sig_handlers_t{ZB_BDB_SIGNAL_STEERING, ^^on_zigbee_start},
	    zbm::sig_handlers_t{ZB_COMMON_SIGNAL_CAN_SLEEP, ^^zb_sleep_now}
	   >(bufid);
    const uint32_t LOCAL_ERR_CODE = (uint32_t) (-ret);	
    if (LOCAL_ERR_CODE != RET_OK) {				
	zb_osif_abort();				
    }							
}

int main(void)
{
    static_assert(atomic_state_t::is_always_lock_free);
    disable_hw_wdt();

    int err = settings_subsys_init();

    printk("main\r\n");

    zigbee_enable();
    while (1) {
	printk("Main: sleep forever\r\n");
	k_sleep(K_FOREVER);
    }

    return 0;
}

