#include "Async/events.h"
#include "Async/task.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include <iostream>

using namespace std::chrono_literals;
const uint led_pin = 25;
const uint button_pin = 16;
/// Task blinking the LED twice per second.
task<>
blink(uint pin, uint delay)
{
	while (true) {
		co_await events::sleep(delay * 1ms);
		gpio_put(pin, 1);
		co_await events::sleep(delay * 1ms);
		gpio_put(pin, 0);
	}
}
task<>
say_something()
{
	std::cout << "I am saying something" << std::endl;
	co_return;
}

/// Task printing every 3 seconds.
task<>
report()
{
	auto t0 = co_await events::sleep(0s);
	for (int i = 0; true; ++i) {
		if (i >= 10) {
			std::cout << "Now!" << std::endl;
			core_loop().start_task(say_something());
		}
		else
			std::cout << "Not yet!" << std::endl;
		auto t = co_await events::sleep(3s);
		std::cout << "Time " << 1e-6 * (t - t0) << std::endl;
	}
}

task<>
pin(uint bp, auto... button_pins)
{
	events::pin_listener pinListener(bp, static_cast<uint>(button_pins)...);
	for (int i = 0; true; ++i) {
		auto event = co_await pinListener.next();
		std::cout << "On core " << get_core_num() << ", event is " << event.second << " for pin "
		          << event.first << std::endl;
	}
}

int
main()
{

	gpio_init(led_pin);
	gpio_set_dir(led_pin, GPIO_OUT);

	stdio_init_all();

	multicore_launch_core1([]() {
		std::cout << "I am on core " << get_core_num() << std::endl;
		core_loop().loop(pin(15));
	});
	// Start the main loop with two tasks.
	loop_control.loop(blink(led_pin, 250), report(), pin(button_pin));
}
