#ifndef PUMP_TASK_HPP_
#define PUMP_TASK_HPP_
#include <cstddef>
#include <cstdint>
#include <variant>

namespace pump
{
    struct start_t
    {
        uint32_t duration = 1000;
        float power_percentage = 100.f;//0..100%
    };
    struct extend_duration_t
    {
        uint32_t additional_duration = 1000;
    };
    struct set_duration_t
    {
        uint32_t duration = 1000;
    };
    struct stop_t
    {
    };
    struct pressure_calibrate_t//get the current voltage of pressure sensor and consider it 100% water level
    {
    };
    struct pressure_reset_calibration_t//get the current voltage of pressure sensor and consider it 100% water level
    {
    };

    using param_t = std::variant<start_t, extend_duration_t, set_duration_t, stop_t, pressure_calibrate_t, pressure_reset_calibration_t>;
    using error_t = int;
    using cb_t = void(*)(error_t err, param_t const& p, void *pUserArg);
    using water_level_cb_t = void(*)(float level_percentage);

    error_t setup_task(water_level_cb_t water_level_cb = nullptr);
    void command(param_t p, cb_t cb = nullptr, void *user = nullptr);
}
#endif
