#include "soil_task.hpp"

#include <iterator>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/sensor.h>
#include <nrf_general/lib_msgq_typed.hpp>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(soil, LOG_LEVEL_INF);

namespace soil
{
#define SOIL_PARENT_NODE DT_NODELABEL(soil_sensors)

    /* Callback macro invoked for every child node */
#define SOIL_DEVICE_ENTRY(child_node_id) DEVICE_DT_GET(child_node_id),

    /* Compile-time array of pointers to the sensor devices */
    constinit static const device *const soil_sensors[] = {
	DT_FOREACH_CHILD_STATUS_OKAY(SOIL_PARENT_NODE, SOIL_DEVICE_ENTRY)
    };

    constexpr auto kSoilCount = std::size(soil_sensors);

    using Queue = msgq::Queue<request_t,4>;
    K_MSGQ_DEFINE_TYPED(Queue, q);

    uint32_t setup_task()
    {
	uint32_t res = 0;
	int ready = 0;
	for(size_t i = 0; i < kSoilCount; ++i)
	{
	    if (device_is_ready(soil_sensors[i]))
	    {
		res |= uint32_t(1) << i;
		++ready;
	    }
	    else
	    {
		LOG_WRN("Soil sensor %d not ready", i);
	    }
	}

	LOG_INF("(setup) Total soil sensors declared: %d; Ready: %d", (int)kSoilCount, ready);
	return res;
    }

    size_t sensors_count()
    {
	return kSoilCount;
    }

    void run_measurements(request_t r)
    {
	q << r;
    }


    void measurements_thread(void *, void *, void *);

    constexpr size_t SOIL_SENSE_THREAD_STACK_SIZE = 1024;
    constexpr size_t SOIL_SENSE_THREAD_PRIORITY=7;

    K_THREAD_DEFINE(soil_sense_thread, SOIL_SENSE_THREAD_STACK_SIZE,
	    measurements_thread, NULL, NULL, NULL,
	    SOIL_SENSE_THREAD_PRIORITY, 0, -1);

    void measurements_thread(void *, void *, void *)
    {
	while(true)
	{
	    request_t r;
	    soil_result_t results[kSoilCount];
	    q >> r;
	    for(size_t i = 0; i < kSoilCount; ++i)
	    {
		if (r.soil_sensors_mask & (uint32_t(1) << i))
		{
		    auto *pDev = soil_sensors[i];
		    if (device_is_ready(pDev))
		    {
			int err = sensor_sample_fetch(pDev);
			if (err < 0)
			{
			    results[i] = std::unexpected(err);
			    LOG_ERR("(request) Soil sensor %d: failed fetching sample with err %d (%s)", (int)i, err, strerror(err));
			}else
			{
			    sensor_value v;
			    err = sensor_channel_get(pDev, sensor_channel::SENSOR_CHAN_VOLTAGE, &v);
			    if (err < 0)
			    {
				results[i] = std::unexpected(err);
				LOG_ERR("(request) Soil sensor %d: failed getting channels value with err %d (%s)", (int)i, err, strerror(err));
			    }else
			    {
				float soil_humidity = v.val1 + float(v.val2) / 1'000'000.f;
				results[i] = soil_humidity;
				LOG_INF("(request) Soil sensor %d: measured %.2f %% humidity", (int)i, (double)soil_humidity);
			    }
			}
		    }else
		    {
			results[i] = std::unexpected(-ENODEV);
			LOG_WRN("(request) Soil sensor %d not ready", (int)i);
		    }
		}
	    }
	    if (r.callback)
	    {
		r.callback(results, kSoilCount, r.soil_sensors_mask, r.user);
	    }
	}
    }
}
