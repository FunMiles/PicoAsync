//
// Created by Michel Lesoinne on 5/22/22.
//

#pragma once

#include <chrono>
#include <coroutine>
#include "pico/stdlib.h"

#include "loop_control.h"

/** The goal of objects in this file is to provide awaitable objects
 * for hardware, or inter-task communication events.
 * For example:
 * ```cpp
 *      co_await sleep(100_ms);
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
  void await_resume() {}
  uint64_t resume_time;
};

}

auto sleep(std::chrono::duration<uint64_t, std::micro> duration) {
  return details::time_awaitable{time_us_64() + duration.count()};
}

} // namespace events