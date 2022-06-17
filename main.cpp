#include "Async/events.h"
#include "Async/task.h"
#include "pico/stdlib.h"
#include <iostream>

using namespace std::chrono_literals;
const uint led_pin = 25;
const uint button_pin = 16;
/// Task blinking the LED twice per second.
task<> blink() {
  for (int i = 0; true; ++i) {
    co_await events::sleep(250ms);
    gpio_put(led_pin, 1);
    co_await events::sleep(250ms);
    gpio_put(led_pin, 0);
  }
}

/// Task printing every 3 seconds.
task<> report() {
  auto t0 = co_await events::sleep(0s);
  for (int i = 0; true; ++i) {
    auto t = co_await events::sleep(3s);
    std::cout << "Time " << 1e-6*(t-t0) << std::endl;
  }
}

task<>
pin(uint bp, auto... button_pins)
{
	events::pin_listener pinListener(bp, static_cast<uint>(button_pins)...);
	for (int i = 0; true; ++i) {
		auto event = co_await pinListener.next();
		std::cout << "Event is " << event.second << " for pin " << event.first << std::endl;
	}
}

int main() {

  gpio_init(led_pin);
  gpio_set_dir(led_pin, GPIO_OUT);

  stdio_init_all();

  // Start the main loop with two tasks.
  loop_control.loop(blink(), report(), pin(button_pin, 15));
}
