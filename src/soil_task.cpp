#include "soil_task.hpp"
#include "soil_sensor.hpp"

#include <iterator>
#include <zephyr/drivers/adc.h>

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

    int setup_task()
    {
	for(size_t i = 0; i < kSoilCount; ++i)
	{
	    device_is_ready(soil_sensors[i]);
	}
	return -ENOTSUP;
    }

    void run_measurements(/*TODO: add parameters*/)
    {
    }
}
