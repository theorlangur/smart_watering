#ifndef SOIL_SENSOR_HPP_
#define SOIL_SENSOR_HPP_

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <expected>

namespace soil{
    class Sensor
    {
    public:
        using error_t = int;

        int setup(const device *power, const adc_dt_spec *sensor);
        std::expected<int, error_t> measure();
    private:
        const device *m_pPower = nullptr;
        const adc_dt_spec *m_pSensor = nullptr;
    };
};
#endif
