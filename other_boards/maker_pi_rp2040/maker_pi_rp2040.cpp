//
// Created by Michel Lesoinne on 6/17/22.
//
#include "Async/events.h"
#include "Async/task.h"

#include "devices/neopixel/NeoPixel.h"
#include "pico/stdlib.h"

#include "hardware/pio.h"

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

task<>
neoPixel(uint pin)
{
	const int numLEDs = 2;
	const int stateMachine = 0;
	NeoPixel np(pin, numLEDs, pio0, stateMachine);
	int color = 1;
	for (int i = 0; i < 10; ++i) {
		co_await 1s;
		std::cout << "Wait for it! " << i << std::endl;
	}
	while(true) {
		co_await 1s;
		np.clear();
		std::cout << "Color " << color << std::endl;
		switch (color % 3) {
		case 0:
			np.push_back({128,0,0});
			np.push_back({0,128,0});
			break;
		case 1:
			np.push_back({0,128,0});
			np.push_back({0,0,128});
			break;
		case 2:
			np.push_back({0,0,128});
			np.push_back({128,0,0});
			break;
		default:
			np.push_back({128,0,0});
			np.push_back({0,128,0});
			break;
		}
		std::cout << "Ready to show " << np.size() << " pixels." << std::endl;
		co_await np.show();
		++color;
		std::cout <<"New color is " << color << std::endl;
	}
}

int
main()
{
	stdio_init_all();
	// Start the main loop with two tasks.
	loop_control.loop(blink(led_pins, 250), neoPixel(18));
}