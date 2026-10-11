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

    using param_t = std::variant<start_t, extend_duration_t, set_duration_t, stop_t>;
    using error_t = int;
    using cb_t = void(*)(error_t err, param_t const& p, void *pUserArg);

    error_t setup_task();
    void command(param_t p, cb_t cb = nullptr, void *user = nullptr);
}
#endif
