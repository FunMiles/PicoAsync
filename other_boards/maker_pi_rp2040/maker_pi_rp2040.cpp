//
// Created by Michel Lesoinne on 6/17/22.
//
#include "Async/events.h"
#include "Async/task.h"
#include "pico/stdlib.h"

using namespace std::chrono_literals;
using events::operator co_await;
const uint    led_pin = 6;

const uint led_pins[] = {0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 26, 27, 28};

/// Task blinking All blue LEDs in sequence.
template <size_t N>
task<>
blink(const uint (&pins)[N], uint delay)
{
	for (auto pin : pins) {
		gpio_init(pin);
		gpio_set_dir(pin, GPIO_OUT);
	}

	while (true) {
		for (auto pin : pins) {
			gpio_put(pin, 1);
			co_await (delay * 1ms);
			gpio_put(pin, 0);
		}
	}
}

int
main()
{
	// Start the main loop with two tasks.
	loop_control.loop(blink(led_pins, 250));
}