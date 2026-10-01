#include "soil_task.hpp"
#include "soil_sensor.hpp"

#include <iterator>
#include <zephyr/drivers/adc.h>

namespace soil
{
#define SOIL_NODE DT_PATH(soil_sensors)
#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
    ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

    constinit static const ::adc_dt_spec soil_sensors_adc[] = {
	DT_FOREACH_PROP_ELEM(SOIL_NODE, io_channels,
		DT_SPEC_AND_COMMA)
    };

#define SOIL_POWER_DEVICE_GET(node_id, prop, idx) \
    DEVICE_DT_GET(DT_PROP_BY_IDX(node_id, prop, idx)),

    /* Compile-time generated array of regulator device pointers */
    constinit static const struct device *const soil_powers[] = {
	DT_FOREACH_PROP_ELEM(SOIL_NODE, power_regs, SOIL_POWER_DEVICE_GET)
    };

    static_assert(std::size(soil_sensors_adc) == std::size(soil_powers));
    constexpr auto kSoilCount = std::size(soil_sensors_adc);

    constinit static Sensor soilSensors[kSoilCount]{};

    int setup_task()
    {
	for(size_t i = 0; i < kSoilCount; ++i)
	{
	    soilSensors[i].setup(soil_powers[i], &soil_sensors_adc[i]);
	}
	return -ENOTSUP;
    }

    void run_measurements(/*TODO: add parameters*/)
    {
    }
}
