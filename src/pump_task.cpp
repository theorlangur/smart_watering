#include "pump_task.hpp"
#include "lib_misc_helpers.hpp"

#include <nrf_general/lib_msgq_typed.hpp>
#include <zephyr/drivers/regulator.h>
#include <zephyr/logging/log.h>
#include <hal/nrf_timer.h>
#include <hal/nrf_gpio.h>

LOG_MODULE_REGISTER(pump, LOG_LEVEL_INF);

namespace pump
{
    template<size_t timerBase, int TimerIrqNum, int Pin, int Freq, bool ActiveHigh>
    struct soft_pwm_t
    {
	static inline constexpr auto kPWM_Pin_Mask = BIT(Pin);
	static inline constexpr auto kPWM_TimerPrio = 1;
	static inline constexpr auto kPWM_TimerPrescaler = NRF_TIMER_FREQ_16MHz;
	static inline constexpr auto kPWM_TicksPerSec = 16'000'000;
	static inline constexpr auto kPWM_PeriodTicks = kPWM_TicksPerSec / Freq;
	static inline constexpr auto kPWM_MinDutyTickSize = kPWM_PeriodTicks / 200;//0.5%
										   //
	constexpr soft_pwm_t() = default;

	inline static NRF_TIMER_Type *TimerBase() { return (NRF_TIMER_Type *)timerBase;}

	inline static void set_active()
	{
	    if constexpr (ActiveHigh)
		NRF_P2->OUTSET = kPWM_Pin_Mask;
	    else
		NRF_P2->OUTCLR = kPWM_Pin_Mask;
	}

	inline static void set_inactive()
	{
	    if constexpr (!ActiveHigh)
		NRF_P2->OUTSET = kPWM_Pin_Mask;
	    else
		NRF_P2->OUTCLR = kPWM_Pin_Mask;
	}

	void set_duty(float duty_percent)
	{
	    if (duty_percent < 0.0f) duty_percent = 0.0f;
	    if (duty_percent > 100.0f) duty_percent = 100.0f;

	    /* Boundary 0%: Disable compare events, force pin LOW */
	    if (duty_percent <= 0.0f) {
		nrf_timer_int_disable(TimerBase(), NRF_TIMER_INT_COMPARE0_MASK | NRF_TIMER_INT_COMPARE1_MASK);
		set_inactive();
		m_DutyCycleTicks = 0;
		return;
	    }

	    /* Boundary 100%: Disable compare events, force pin HIGH */
	    if (duty_percent >= 100.0f) {
		nrf_timer_int_disable(TimerBase(), NRF_TIMER_INT_COMPARE0_MASK | NRF_TIMER_INT_COMPARE1_MASK);
		set_active();
		m_DutyCycleTicks = kPWM_PeriodTicks;
		return;
	    }

	    /* Active PWM: Clamp narrow pulses (< 1.5 us) to avoid ISR re-entrancy collision */
	    uint32_t ticks = (uint32_t)((duty_percent / 100.0f) * (float)kPWM_PeriodTicks);
	    if (ticks < kPWM_MinDutyTickSize) ticks = kPWM_MinDutyTickSize;
	    if (ticks > (kPWM_PeriodTicks - kPWM_MinDutyTickSize)) ticks = kPWM_PeriodTicks - kPWM_MinDutyTickSize;

	    m_DutyCycleTicks = ticks;
	    nrf_timer_cc_set(TimerBase(), NRF_TIMER_CC_CHANNEL0, m_DutyCycleTicks);
	    nrf_timer_int_enable(TimerBase(), NRF_TIMER_INT_COMPARE0_MASK | NRF_TIMER_INT_COMPARE1_MASK);
	}

	void stop()
	{
	    nrf_timer_int_disable(TimerBase(), NRF_TIMER_INT_COMPARE0_MASK | NRF_TIMER_INT_COMPARE1_MASK);
	    nrf_timer_task_trigger(TimerBase(), NRF_TIMER_TASK_STOP);
	    m_DutyCycleTicks = 0;
	    set_inactive();
	}

	void setup()
	{
	    /* 1. Configure Port 2 pin as output */
	    NRF_P2->PIN_CNF[Pin] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
		(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
		(GPIO_PIN_CNF_DRIVE0_H0 << GPIO_PIN_CNF_DRIVE0_Pos); /* High Drive */
	    NRF_P2->OUTCLR = kPWM_Pin_Mask;

	    /* 2. Configure 32-bit Hardware Timer */
	    nrf_timer_mode_set(TimerBase(), NRF_TIMER_MODE_TIMER);
	    nrf_timer_bit_width_set(TimerBase(), NRF_TIMER_BIT_WIDTH_32);
	    nrf_timer_prescaler_set(TimerBase(), kPWM_TimerPrescaler);

	    /* CC[1] defines the period; hardware shortcut resets timer on match */
	    nrf_timer_cc_set(TimerBase(), NRF_TIMER_CC_CHANNEL1, kPWM_PeriodTicks);
	    nrf_timer_shorts_enable(TimerBase(), NRF_TIMER_SHORT_COMPARE1_CLEAR_MASK);

	    /* 3. Connect IRQ */
	    IRQ_CONNECT(TimerIrqNum, kPWM_TimerPrio, timer_pwm_isr, NULL, 0);
	    irq_enable(TimerIrqNum);

	    /* 4. Start timer */
	    nrf_timer_task_trigger(TimerBase(), NRF_TIMER_TASK_CLEAR);
	    nrf_timer_task_trigger(TimerBase(), NRF_TIMER_TASK_START);

	    set_duty(0.0f);
	}
    private:
	static void timer_pwm_isr(const void *arg)
	{
	    if (nrf_timer_event_check(TimerBase(), NRF_TIMER_EVENT_COMPARE0)) {
		nrf_timer_event_clear(TimerBase(), NRF_TIMER_EVENT_COMPARE0);
		set_inactive();
	    }

	    /* End of period -> Set pin HIGH */
	    if (nrf_timer_event_check(TimerBase(), NRF_TIMER_EVENT_COMPARE1)) {
		nrf_timer_event_clear(TimerBase(), NRF_TIMER_EVENT_COMPARE1);
		set_active();
	    }
	}
	uint32_t m_DutyCycleTicks = 0;
    };

    constinit const struct device *pump_power = DEVICE_DT_GET(DT_NODELABEL(pump_power_ctrl));
    //constinit const struct device *pump_ctrl = DEVICE_DT_GET(DT_NODELABEL(pump_pwm_ctrl));
    constinit soft_pwm_t<NRF_TIMER00_S_BASE, TIMER00_IRQn, 3, 2000, false> pump_ctrl;

    struct request_t
    {
	param_t p;
	cb_t cb;
	void *user;
    };

    using Queue = msgq::Queue<request_t,4>;
    K_MSGQ_DEFINE_TYPED(Queue, q);

    void command(param_t p, cb_t cb, void *user)
    {
	q << request_t{std::move(p), cb, user};
    }


    void pump_thread(void *, void *, void *);

    constexpr size_t PUMP_THREAD_STACK_SIZE = 4*1024;
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
	    if (q.Recv(r, timeout) == -EAGAIN)
	    {
		LOG_INF("timed out. scheduling stop");
		r = {stop_t{}, nullptr, nullptr};
	    }
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
		    LOG_INF("received entry, but also timed out. scheduling stop");
		    pump_timeout = 0;
		    command(stop_t{}, nullptr, nullptr);
		}
	    }

	    error_t e = std::visit(
		overloaded{
		    [&](start_t const &p)->error_t{
			error_t err;
			pump_ctrl.stop();
			k_msleep(1);
			if ((err = regulator_enable(pump_power)) < 0)
			{
			    LOG_ERR("power regulator failed: %d (%s)", err, strerror(err));
			    return err;
			}
			pump_ctrl.setup();
			//ramp up, instead of immediate
			for(float i = 0; i < p.power_percentage; i += p.power_percentage / 10)
			{
			    pump_ctrl.set_duty(i);
			    k_msleep(100);
			}
			pump_ctrl.set_duty(p.power_percentage);

			pump_active = true;
			pump_timeout = p.duration;
			timeout = Z_TIMEOUT_MS(p.duration);
			last_timestamp = k_uptime_get_32();
			pump_start_timestamp = last_timestamp;
			LOG_INF("started. for %dms", pump_timeout);
			return err;
		    },
		    [&](extend_duration_t const &p)->error_t{
			if (pump_active)
			{
			    LOG_INF("extending duration for %dms", p.additional_duration);
			    uint32_t now = k_uptime_get_32();
			    pump_timeout = now - last_timestamp + p.additional_duration;
			    timeout = Z_TIMEOUT_MS(pump_timeout);
			    last_timestamp = now;
			}
			return 0;
		    },
		    [&](set_duration_t const &p)->error_t{
			if (pump_active)
			{
			    uint32_t now = k_uptime_get_32();
			    if ((now - pump_start_timestamp) >= p.duration)
			    {
				LOG_INF("set duration: new duration (%dms) less than ellapsed. stopping", p.duration);
				//stop immediately
				command(stop_t{}, nullptr, nullptr);
			    }else
			    {
				LOG_INF("set duration: new duration (%dms) is bigger than ellapsed. extendnig", p.duration);
				//extend the timeout
				uint32_t delta = p.duration - (now - pump_start_timestamp);
				pump_timeout = now - last_timestamp + delta;
				timeout = Z_TIMEOUT_MS(pump_timeout);
				last_timestamp = now;
			    }
			}
			return 0;
		    },
		    [&](stop_t const &p)->error_t{
			pump_ctrl.stop();
			k_msleep(1);
			regulator_disable(pump_power);
			pump_ctrl.set_active();//put to GND
			timeout = K_FOREVER;
			pump_active = false;
			LOG_INF("stopped");
			return 0;
		    },
		},
		r.p
	    );
	    if (r.cb)
		r.cb(e, r.p, r.user);
	}
    }

    error_t setup_task()
    {
	if (!device_is_ready(pump_power))
	{
	    LOG_ERR("power regulator device not ready");
	    return -ENODEV;
	}
	pump_ctrl.stop();
	pump_ctrl.set_active();//put to GND

	k_thread_start(pump_thread_obj);
	return 0;
    }
}
