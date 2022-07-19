#include "Async/events.h"
#include "Async/task.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
// You also need i2c library to communicate with the display
#include "hardware/i2c.h"

#include <Async/devices/ssd1306/SSD1306.h>
#include <iostream>

using namespace std::chrono_literals;
using namespace async;
// Make it possible to await a duration
using events::operator co_await;

const uint led_pin = 25;
const uint button_pin = 16;
/// Task blinking the LED twice per second.
task<>
blink(uint pin, uint delay)
{

	gpio_init(pin);
	gpio_set_dir(pin, GPIO_OUT);

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

task<>
ssd1306(uint basePin)
{
	auto I2C_PIN_SDA = basePin;
	auto I2C_PIN_SCL = basePin+1;
	auto I2C_PORT = i2c0;
	i2c_init(I2C_PORT, 1000000); //Use i2c port with baud rate of 1Mhz
	//Set pins for I2C operation
	gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
	gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_PIN_SDA);
	gpio_pull_up(I2C_PIN_SCL);

	//Create a new display object
	pico_ssd1306::SSD1306 display = pico_ssd1306::SSD1306(I2C_PORT, 0x3C, pico_ssd1306::Size::W128xH32);


	for (int offset = 0; offset < 1024; ++offset) {
		if (offset == 128) {
			offset = 0;
			std::cout << "Starting again" << std::endl;
		}
		display.clear();
		// create a vertical line moving from left to right.
		for (int16_t y = 0; y < 64; y++) {
			display.setPixel(offset, y);
		}
		// create three vertical lines moving from right to left.
		for (int16_t x = 0; x < 16; ++x) {
			auto xx = 128-offset-x ;
			if (xx < 0)
				xx += 128;
			display.setPixel(xx, 0);
			display.setPixel(xx, 16);
			display.setPixel(xx, 31);
		}
		co_await display.sendBuffer(); // Send buffer to device and show on screen
		co_await 50ms;
	}
}

int
main()
{

	stdio_init_all();

	multicore_launch_core1([]() {
		std::cout << "I am on core " << get_core_num() << std::endl;
		core_loop().loop(ssd1306(0));
	});
	// Start the main loop with two tasks.
	loop_control.loop(blink(led_pin, 250), report(), pin(button_pin));
}
