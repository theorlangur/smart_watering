#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/device.h>
#include <zephyr/logging/log.h>

#include "usb_uart.hpp"

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_console)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

static const struct gpio_dt_spec usb_sense = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(sense), gpios);

LOG_MODULE_REGISTER(usb_uart_mgr, LOG_LEVEL_INF);

namespace usb_uart
{
    constinit static bool g_LastUSBState = false;
    constinit static usb_sense_callback_t g_SenseCB = nullptr;

    static struct gpio_callback sense_cb_data;
    static struct k_work usb_sense_work;

    static void usb_sense_work_handler(struct k_work *work)
    {
	int connected = gpio_pin_get_dt(&usb_sense);
	bool connectedBool = connected > 0;

	if (connectedBool) {
	    LOG_INF("VBUS detected: Resuming UART00 (default pinctrl)");
	    /* Re-enables UART clocks, EasyDMA, and applies uart00_default */
	    pm_device_action_run(uart_dev, PM_DEVICE_ACTION_RESUME);
	} else {
	    LOG_INF("VBUS lost: Suspending UART00 (sleep pinctrl / High-Z)");
	    /* Disables peripheral and applies uart00_sleep with low-power-enable */
	    pm_device_action_run(uart_dev, PM_DEVICE_ACTION_SUSPEND);
	}

	if (connectedBool != g_LastUSBState)
	{
	    g_LastUSBState = connectedBool;
	    if (g_SenseCB)
		g_SenseCB(g_LastUSBState);
	}
    }

    static void usb_sense_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
    {
	/* Defer PM state change out of interrupt context */
	k_work_submit(&usb_sense_work);
    }

    void set_callback(usb_sense_callback_t cb)
    {
	g_SenseCB = cb;
    }

    int setup()
    {
	if (!device_is_ready(uart_dev))
	{
	    LOG_ERR("USB sense GPIO not ready");
	    return -ENODEV;
	}
	if (!gpio_is_ready_dt(&usb_sense)) {
	    LOG_ERR("USB sense GPIO not ready");
	    return -ENODEV;
	}

	int err;
	k_work_init(&usb_sense_work, usb_sense_work_handler);

	err = gpio_pin_configure_dt(&usb_sense, GPIO_INPUT);
	if (err < 0) return err;
	err = gpio_pin_interrupt_configure_dt(&usb_sense, GPIO_INT_EDGE_BOTH | GPIO_INT_LEVELS_LOGICAL);
	if (err < 0) return err;
	gpio_init_callback(&sense_cb_data, usb_sense_isr, BIT(usb_sense.pin));
	err = gpio_add_callback(usb_sense.port, &sense_cb_data);
	if (err < 0) return err;

	err = k_work_submit(&usb_sense_work);
	return err;
    }

    bool usb_state()
    {
	return g_LastUSBState;
    }

    SYS_INIT(setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
}

