#ifndef SOIL_TASK_HPP_
#define SOIL_TASK_HPP_
#include <cstddef>
#include <cstdint>
#include <expected>

namespace soil
{
    using soil_result_t = std::expected<float, int>;
    using cb_t = void(*)(soil_result_t *pResults, size_t count, uint32_t mask, void *pUserArg);
    struct request_t
    {
        uint32_t soil_sensors_mask = 0xffffffff;
        cb_t callback = nullptr;
        void *user = nullptr;
    };

    uint32_t setup_task();
    size_t sensors_count();
    void run_measurements(request_t r);
}
#endif
