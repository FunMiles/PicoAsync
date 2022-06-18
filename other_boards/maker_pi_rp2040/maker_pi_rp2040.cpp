//
// Created by Michel Lesoinne on 6/17/22.
//
#include "Async/events.h"
#include "Async/task.h"
#include "pico/stdlib.h"

using namespace std::chrono_literals;
using events::operator co_await;
const uint led_pin = 6;

/// Task blinking the LED twice per second.
task<> blink(uint pin, uint delay) {
	gpio_init(pin);
	gpio_set_dir(pin, GPIO_OUT);

	for (int i = 0; true; ++i) {
		co_await (delay * 1ms);
		gpio_put(pin, 1);
		co_await (delay * 1ms);
		gpio_put(pin, 0);
	}
}

int main() {
	// Start the main loop with two tasks.
  loop_control.loop(blink(led_pin, 250));
}