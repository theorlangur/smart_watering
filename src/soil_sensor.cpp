#include "soil_sensor.hpp"

namespace soil
{
    int Sensor::setup(const device *power, const adc_dt_spec *sensor)
    {
	m_pPower = power;
	m_pSensor = sensor;
	return -ENOTSUP;
    }

    std::expected<int, Sensor::error_t> Sensor::measure()
    {
	return std::unexpected(-ENOTSUP);
    }
}
