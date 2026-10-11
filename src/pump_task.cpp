#include "pump_task.hpp"
#include "lib_misc_helpers.hpp"

#include <nrf_general/lib_msgq_typed.hpp>
#include <zephyr/drivers/regulator.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pump, LOG_LEVEL_INF);

namespace pump
{
    constinit const struct device *pump_power = DEVICE_DT_GET(DT_NODELABEL(pump_power_ctrl));
    constinit const struct device *pump_ctrl = DEVICE_DT_GET(DT_NODELABEL(pump_pwm_ctrl));

    struct request_t
    {
	param_t p;
	cb_t cb;
	void *user;
    };

    using Queue = msgq::Queue<request_t,4>;
    K_MSGQ_DEFINE_TYPED(Queue, q);

    error_t setup_task()
    {
	if (!device_is_ready(pump_power))
	    return -ENODEV;
	if (!device_is_ready(pump_ctrl))
	    return -ENODEV;
	return 0;
    }

    void command(param_t p, cb_t cb, void *user)
    {
	q << request_t{std::move(p), cb, user};
    }


    void pump_thread(void *, void *, void *);

    constexpr size_t PUMP_THREAD_STACK_SIZE = 1024;
    constexpr size_t PUMP_THREAD_PRIORITY=2;

    K_THREAD_DEFINE(pump_thread_obj, PUMP_THREAD_STACK_SIZE,
	    pump_thread, NULL, NULL, NULL,
	    PUMP_THREAD_PRIORITY, 0, -1);

    void pump_thread(void *, void *, void *)
    {
	k_timeout_t timeout = K_FOREVER;
	uint32_t last_timestamp = k_uptime_get_32();
	uint32_t pump_start_timestamp = 0;
	uint32_t pump_timeout = 0;
	bool pump_active = false;
	while(true)
	{
	    request_t r;
	    uint32_t pre_queue_wait = k_uptime_get_32();
	    if (q.Recv(r, timeout) == -ETIMEDOUT)
		r = {stop_t{}, nullptr, nullptr};
	    else if (pump_active)
	    {
		uint32_t now = k_uptime_get_32();
		//re-configure timeout for the rest of time
		if ((now - pre_queue_wait) <= pump_timeout)
		{
		    pump_timeout -= now - pre_queue_wait;
		    timeout = Z_TIMEOUT_MS(pump_timeout);
		}
		else
		{
		    pump_timeout = 0;
		    command(stop_t{}, nullptr, nullptr);
		}
	    }

	    error_t e = std::visit(
		overloaded{
		    [&](start_t const &p)->error_t{
			error_t err;
			if ((err = regulator_enable(pump_power)) < 0)
			    return err;
			if ((err = regulator_enable(pump_ctrl)) < 0)
			{
			    regulator_disable(pump_power);
			    return err;
			}
			pump_active = true;
			pump_timeout = p.duration;
			timeout = Z_TIMEOUT_MS(p.duration);
			last_timestamp = k_uptime_get_32();
			pump_start_timestamp = last_timestamp;
		    },
		    [&](extend_duration_t const &p)->error_t{
			if (pump_active)
			{
			    uint32_t now = k_uptime_get_32();
			    pump_timeout = now - last_timestamp + p.additional_duration;
			    timeout = Z_TIMEOUT_MS(pump_timeout);
			    last_timestamp = now;
			}
		    },
		    [&](set_duration_t const &p)->error_t{
			if (pump_active)
			{
			    uint32_t now = k_uptime_get_32();
			    if ((now - pump_start_timestamp) >= p.duration)
			    {
				//stop immediately
				command(stop_t{}, nullptr, nullptr);
			    }else
			    {
				//extend the timeout
				uint32_t delta = p.duration - (now - pump_start_timestamp);
				pump_timeout = now - last_timestamp + delta;
				timeout = Z_TIMEOUT_MS(pump_timeout);
				last_timestamp = now;
			    }
			}
		    },
		    [&](stop_t const &p)->error_t{
			regulator_disable(pump_ctrl);
			regulator_disable(pump_power);
			timeout = K_FOREVER;
			pump_active = false;
		    },
		},
		r.p
	    );
	    if (r.cb)
		r.cb(e, r.p, r.user);
	}
    }
}
