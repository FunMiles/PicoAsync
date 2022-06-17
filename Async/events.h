//
// Created by Michel Lesoinne on 5/22/22.
//

#pragma once

#include "pico/stdlib.h"
#include <chrono>
#include <coroutine>
#include <iostream>

#include "loop_control.h"
#include "task.h"

/** The goal of objects in this file is to provide awaitable objects
 * for hardware, or inter-task communication events.
 * For example:
 * ```cpp
 *      co_await 100ms; // sleep for 100ms
 *      co_await core_1; // Switch execution to core 1
 *      co_await core_0; // Switch execution to core 0
 * ```
 */

namespace events {

namespace details {
/** \brief Awaitable that schedule a coroutine to resume at a later time. */
struct time_awaitable {
	bool await_ready() { return false; }
	void await_suspend(std::coroutine_handle<> h)
	{
		// Schedule resuming the caller after the given duration.
		core_loop().schedule(h, resume_time);
	}
	auto     await_resume() { return resume_time; }
	uint64_t resume_time;
};

} // namespace details

/** \brief Get an awaitable for later continuation of the calling coroutine.
 *
 * @param duration Time to sleep before continuation.
 * @return An awaitable that schedules continuation after the given duration.
 */
auto
sleep(std::chrono::duration<uint64_t, std::micro> duration)
{
	return details::time_awaitable{time_us_64() + duration.count()};
}

/// \brief Enables to co_await a duration.
template <typename T, typename D>
auto operator co_await(std::chrono::duration<T, D> duration)
{
	return sleep(duration);
}

/** Get an awaitable for continuation at a given Pico time.
 *
 * @param when The time in the Pico's time system.
 * @return
 */
auto
wait_until(uint64_t when)
{
	return details::time_awaitable{when};
}


namespace details {
template <typename T>
concept Integral = std::is_integral_v<T>;
}

class pin_listener {
public:

	template <details::Integral...T>
	pin_listener(T ... pinList)
	{
		uint pl[] = { static_cast<uint>(pinList) ... };
		for (auto pin_index : pl) {
			gpio_init(pin_index);
			gpio_set_dir(pin_index, GPIO_IN);
			gpio_pull_up(pin_index);

			gpio_set_irq_enabled_with_callback(pin_index, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true,
			                                   pinChange);

			perCoreListeners[get_core_num()][pin_index] = this;
		}
	}
	auto next()
	{
		struct pin_awaitable {
			bool await_ready() { return false; }
			void await_suspend(std::coroutine_handle<> h)
			{
				// Schedule resuming the caller after the given duration.
				std::cout << "Setting up to wait" << std::endl;
				listener->h = h;
			}
			auto          await_resume() { return std::pair{listener->pin, listener->event}; }
			pin_listener *listener;
		};
		return pin_awaitable{this};
	}

private:
	static constexpr uint numPins = 32;
	static inline pin_listener    *perCoreListeners[2][numPins];
	std::coroutine_handle<> h;
	uint                    pin;
	uint32_t                event;
	/** \brief Interrupt handler routine
	 * \note This may have a race condition if other interrupts wish to touch the same
	 * list of actions on the core_loop().
	 *
	 * @param pin
	 * @param event
	 */
	static void __not_in_flash_func(pinChange)(uint pin, uint32_t event)
	{
		auto &listener = perCoreListeners[get_core_num()];
		if (listener[pin]->h) {
			listener[pin]->pin = pin;
			listener[pin]->event = event;
			uint32_t save = save_and_disable_interrupts();
			core_loop().scheduleInterruptAction(listener[pin]->h);
			__mem_fence_release();
			restore_interrupts(save);
		}
		listener[pin]->h = nullptr;
	}
};

/// \brief Task to get one character from the USB.
/// \details For it to work, the loop controller has to have a constant call to tud_task.
/// Otherwise most characters will be missed.
task<char>
one_char()
{
	using namespace std::chrono_literals;
	while (true) {
		auto c = getchar_timeout_us(0);
		if (c >= 0)
			co_return static_cast<char>(c);
		co_await sleep(10us);
	}
}
} // namespace events