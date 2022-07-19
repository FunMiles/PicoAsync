//
// Created by Michel Lesoinne on 6/3/22.
//
#include <stdio.h>


#include <device/usbd.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"

#include "Async/events.h"
#include "Async/task.h"

using namespace async;
using namespace std::chrono_literals;
// To be able to co_await a duration. E.g. co_await 3ms;
using events::operator co_await;

/// \brief Create a task printing time and timings every 3 seconds.
task<>
report(int numSeconds)
{
	auto t0 = co_await 0s;
	for (int i = 0; true; ++i) {
		auto t = co_await (numSeconds * 1s);
		std::cout << "Time " << 1e-6 * (t - t0) << " on core " << get_core_num() << std::endl;
	}
}

task<> blink(uint pin)
{
	for (int i = 0; true; ++i) {
		co_await 250ms;
		gpio_put(pin, true);
		co_await 250ms;
		gpio_put(pin, false);
	}

}

int
main()
{
	const uint led_pin = 25;

	stdio_init_all();

	gpio_init(led_pin);
	gpio_set_dir(led_pin, GPIO_OUT);

	multicore_launch_core1( []() {
		std::cout << "I am on core " << get_core_num() << std::endl;
		core_loop().loop(report(7));
	});
	// Start the main loop with 5 tasks.
	loop_control.loop( blink(led_pin), report(5) );
	return 0;
}