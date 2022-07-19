//
// Created by Michel Lesoinne on 6/17/22.
//
#include "Async/events.h"
#include "Async/task.h"

#include "devices/neopixel/NeoPixel.h"
#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/pwm.h"

using namespace std::chrono_literals;
using namespace async;
using events::operator co_await;
const uint    led_pin = 6;

const uint led_pins[] = {0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 26, 27, 28};

int freq[] = { 659, 659, 0, 659, 0, 523, 659, 0, 784 };
float durations[] = { 0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.2 };
const uint piezo_pin = 22;

class Piezo {
public:
	Piezo(uint pin) : pin(pin) {
		gpio_init(pin);
		gpio_set_function(pin, GPIO_FUNC_PWM);
		slice_num = pwm_gpio_to_slice_num(pin);
	}

	task<> tone(uint frequency, float duration)
	{
		if (frequency != 0) {
			// Set a divider to bring the frequency range between about 20Hz and 10kHz
			pwm_set_clkdiv_int_frac(slice_num, 125, 0);
			auto wrap = static_cast<uint16_t>(1.0e6f / frequency);
			pwm_set_wrap(slice_num, wrap);

			pwm_set_gpio_level(pin, wrap/2);
			// If not reset, the counter continues from where it was and if it is
			// already beyond wrap/2, the next pulse change may be delayed by
			// up to 65536 - current_counter
			pwm_set_counter(slice_num, 0);
			// Set the PWM running
			pwm_set_enabled(slice_num, true);
		}
		co_await  (static_cast<int>(duration*1e6) * 1us);
		pwm_set_enabled(slice_num, false);
	}

private:
	uint pin;
	uint slice_num;
};

bool tones_are_playing = true;

task<> play_tones()
{
	tones_are_playing = true;
	Piezo piezo(piezo_pin);
	for (int i = 0; i < 9; ++i) {
		co_await piezo.tone(freq[i], durations[i]);
	}
	tones_are_playing = false;
}

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
	while(true) {
		np.clear();
		switch (color % 3) {
		case 0:
			np.push_back({128,0,0});
			np.push_back({0,128,0});
			break;
		case 1:
			np.push_back({0,128,0});
			np.push_back({128,64,0});
			break;
		case 2:
			np.push_back({128,64,0});
			np.push_back({128,0,0});
			break;
		default:
			np.push_back({128,0,0});
			np.push_back({0,128,0});
			break;
		}
		co_await np.show();
		co_await 1s;
		++color;
	}
}

task<>
buttons() {
	const uint buttonA = 20;
	const uint buttonB = 21;
	co_await 5s;
	std::cout << "Starting pin listener." << std::endl;
	events::pin_listener pinListener(buttonA, buttonB);
	while(true) {
		auto [pin, event] = co_await pinListener.next();
		std::cout << "Pin " << pin << " got event " << event << std::endl;

		if (pin == 21 && event == 8)
			if (!tones_are_playing) {
				tones_are_playing = true;
				core_loop().start_task(play_tones());
			}
	}
}

task<>
servo(uint pin)
{
	gpio_set_function(pin, GPIO_FUNC_PWM);
	// Find out which PWM slice is connected to GPIO pin
	uint slice_num = pwm_gpio_to_slice_num(pin);
	uint channel_num = pwm_gpio_to_channel(pin);
	// Producing 50Hz
	pwm_set_clkdiv_int_frac (slice_num,  38,3);
	pwm_set_wrap(slice_num,65535);

	pwm_set_gpio_level(pin, 65536/20);
	// Set the PWM running
	pwm_set_enabled(slice_num, true);
	uint16_t level = 0;
	while (true) {
		co_await 5ms;
		level += 50;
		// Servos want a duty cycle between 5 and 10%
		pwm_set_gpio_level(pin, 3270+level/20);
	}
}

int
main()
{
	stdio_init_all();
	// Start the main loop with two tasks.
	loop_control.loop(
	    blink(led_pins, 250),
	    neoPixel(18),
	    servo(15),
	    play_tones(),  buttons());
}