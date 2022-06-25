//
// Created by Michel Lesoinne on 6/24/22.
//
#include "Async/events.h"
#include "Async/task.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
// You also need i2c library to communicate with the display
#include "hardware/i2c.h"

#include <Async/devices/ssd1306/SSD1306.h>
#include <iostream>

using namespace std::chrono_literals;
// Make it possible to await a duration
using events::operator co_await;

const uint led_pin = 25;
const uint i2c_base_pin = 0;

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

/// Task printing every 3 seconds.
task<>
report()
{
	auto t0 = co_await events::sleep(0s);
	while(true) {
		auto t = co_await events::sleep(3s);
		std::cout << "Time " << 1e-6 * (t - t0) << std::endl;
	}
}

/// \brief Drive a 128x32 SSD1306-based OLED display
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

	// Create a new display object
	// To drive a 128x64 display, change the 0x3C into 0x3D and the last argument.
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

	gpio_init(led_pin);
	gpio_set_dir(led_pin, GPIO_OUT);

	stdio_init_all();
	// Start the main loop with three tasks.
	loop_control.loop(blink(led_pin, 250), ssd1306(i2c_base_pin), report());
}