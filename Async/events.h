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

struct time_awaitable {
  bool await_ready() { return false; }
  void await_suspend(std::coroutine_handle<> h) {
    // Schedule resuming the caller after the given duration.
    loop_control.schedule(h, resume_time);
  }
  auto await_resume() { return resume_time; }
  uint64_t resume_time;
};

}

auto sleep(std::chrono::duration<uint64_t, std::micro> duration) {
  return details::time_awaitable{time_us_64() + duration.count()};
}

/// \brief Enables to co_await a duration.
template <typename T, typename D>
auto operator co_await(std::chrono::duration<T,D> duration)
{
	return sleep(duration);
}

auto wait_until(uint64_t when) {
  return details::time_awaitable{when};
}

template <uint pin_index>
class pin_listener {
public:
  pin_listener() {
    gpio_init(pin_index);
    gpio_set_dir(pin_index, GPIO_IN);
    gpio_pull_up(pin_index);

    gpio_set_irq_enabled_with_callback(pin_index, GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL, true, pinChange);

    listener = this;
  }
  auto next() {
    struct pin_awaitable {
      bool await_ready() { return false; }
      void await_suspend(std::coroutine_handle<> h) {
        // Schedule resuming the caller after the given duration.
        std::cout << "Setting up to wait" << std::endl;
        listener->h = h;
      }
      auto await_resume() { return listener->event; }
      pin_listener *listener;
    };
    return pin_awaitable{this};
  }
private:
  static pin_listener *listener;
  std::coroutine_handle<> h;
  uint32_t event;
  // This is currently having a race condition with the loop.
  static void pinChange(uint pin, uint32_t event) {
    std::cout << "Pin change " << listener->h.address() << std::endl;
    if (listener->h) {
      listener->event = event;
      std::cout << "Scheduling the thing" << std::endl;
      loop_control.schedule(listener->h, 0);
    }
    listener->h = nullptr;
  }
};

template <uint pin>
inline
pin_listener<pin> *pin_listener<pin>::listener = nullptr;

/// \brief Task to get one character from the USB.
/// \details For it to work, the loop controller has to have a constant call to tud_task.
/// Otherwise most characters will be missed.
task<char> one_char() {
	using namespace std::chrono_literals;
	while(true) {
		auto c = getchar_timeout_us(0);
		if (c >= 0)
			co_return static_cast<char>(c);
		co_await sleep(10us);
	}
}
} // namespace events