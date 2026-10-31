#include "aem13921_task.hpp"

#include <iterator>
#include <nrf_general/lib_msgq_typed.hpp>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor/aem13921.hpp>

LOG_MODULE_REGISTER(aem13921, LOG_LEVEL_INF);

namespace aem13921
{
    static const struct gpio_dt_spec aem13921_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(i2c_irq), gpios);
    static const struct device *const aem13921_dev = DEVICE_DT_GET(DT_NODELABEL(aem13921));

    constexpr size_t AEM13921_THREAD_STACK_SIZE = 2*1024;
    constexpr size_t AEM13921_THREAD_PRIORITY=3;

    void aem_service_thread(void *, void *, void *);
    K_THREAD_DEFINE(aem13921_sense_thread, AEM13921_THREAD_STACK_SIZE,
	    aem_service_thread, NULL, NULL, NULL,
	    AEM13921_THREAD_PRIORITY, 0, -1);

    void aem_service_thread(void *, void *, void *)
    {
	while(true)
	{
	}
    }

    int setup_task()
    {
	k_thread_start(aem13921_sense_thread);
	return 0;
    }
}
