//
// Created by Michel Lesoinne on 6/1/22.
//
#include <array>
#include <span>
#include <stdio.h>

#include "Async/events.h"
#include "Async/task.h"

#include "DS1820_DMA.pio.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "hardware/structs/systick.h"

#include "DMA.h"

using namespace std::chrono_literals;

using namespace std;

const uint led_pin = 25;
const uint button_pin = 16;

uint tickWrap = 0;

uint wpCount{0x1000000u};

task<>
wrapTick()
{
	auto tick = systick_hw->cvr;
	while (true) {
		co_await events::sleep(3ms);
		auto tick2 = systick_hw->cvr;
		if (tick2 > tick)
			tickWrap += 0x1000000u;
		tick = tick2;
		++wpCount;
	}
}

uint32_t
getTicks()
{
	return tickWrap - systick_hw->cvr;
}

uint8_t
crc8(span<uint8_t> data)
{
	uint8_t crc = 0;
	for (int i = 0; i < data.size(); i++) {
		auto databyte = data[i];
		for (int j = 0; j < 8; j++) {
			auto temp = (crc ^ databyte) & 0x01;
			crc >>= 1;
			if (temp)
				crc ^= 0x8C;
			databyte >>= 1;
		}
	}
	return crc;
}
uint32_t writeTicks;


void
sendHeader(PIO pio, uint sm, uint16_t first, uint16_t second)
{
	uint32_t header = static_cast<uint32_t>(first) |
	                  (static_cast<uint32_t>(second) << 16u);
	pio_sm_put_blocking(pio, sm, header);
}

void
writeBytes(PIO pio, uint sm, span<const uint8_t> bytes)
{
	auto t0 = getTicks();
	auto len = bytes.size();
	sendHeader(pio, sm, 250, len-1);
	for (int i = 0; i < len; i++) {
		pio_sm_put_blocking(pio, sm, bytes[i]);
	}
	writeTicks = getTicks() - t0;
}

uint32_t readTicks;

task<float>
getTemperature_DMA(PIO pio, uint sm)
{
	static bool               first = true;
	static array<uint32_t, 9> buffer;

	if (first) {
		initInputDMA(pio, sm, buffer);
		first = false;
	}
	float temperature = -1.0;
	if (is_triggered) {
		array<uint8_t, 9> bytes;

		for (int i = 0; i < bytes.size(); i++) {
			bytes[i] = buffer[i] >> 24;
		}
		uint8_t crc = crc8(bytes);
		if (crc == 0) {
			int     t1 = bytes[0];
			int     t2 = bytes[1];
			int16_t temp1 = (t2 << 8 | t1);
			temperature = (float)temp1 / 16;
		}
	}
	auto t0 = getTicks();
	writeBytes(pio, sm, ((uint8_t[]){0xCC, 0x44}));
	auto t1 = getTicks();
	co_await events::sleep(1000ms);
	auto t2 = getTicks();
	triggerDMAReceive(buffer);

	writeBytes(pio, sm, ((uint8_t[]){0xCC, 0xBE}));
	// Tell the PIO code we want to read and how much data we want
	sendHeader(pio, sm, 0, buffer.size() - 1);
	auto t3 = getTicks();
	readTicks = t1-t0 +t3 - t2;
	co_return temperature;
}

uint
DS18Initalize(PIO pio, int gpio)
{
	uint offset = pio_add_program(pio, &DS1820_program);
	uint sm = pio_claim_unused_sm(pio, true);
	pio_gpio_init(pio, gpio);
	pio_sm_config c = DS1820_program_get_default_config(offset);
	sm_config_set_clkdiv_int_frac(&c, 255, 0);
	sm_config_set_set_pins(&c, gpio, 1);
	sm_config_set_out_pins(&c, gpio, 1);
	sm_config_set_in_pins(&c, gpio);
	// Set the shifting to the right as we receive LSB to HSB order.
	// Automatic shift of data to the output FIFO after 8 bits have been shifted in.
	sm_config_set_in_shift(&c, true, true, 8);
	pio_sm_init(pio0, sm, offset, &c);
	pio_sm_set_enabled(pio0, sm, true);
	return sm;
}

task<>
print_temp()
{
	uint sm = DS18Initalize(pio0, 2);

	for (bool normal = true;; normal = !normal) {
		float t;
		do {
			co_await events::sleep(1s);
			t = co_await getTemperature_DMA(pio0, sm);
		} while (t < -999);
		printf("temperature %f\r\n", t);
		co_await events::sleep(500ms);
	};
}
/// Task blinking the LED twice per second.
task<>
blink()
{
	gpio_init(led_pin);
	gpio_set_dir(led_pin, GPIO_OUT);

	for (int i = 0; true; ++i) {
		co_await events::sleep(250ms);
		gpio_put(led_pin, 1);
		co_await events::sleep(250ms);
		gpio_put(led_pin, 0);
	}
}

/// Task printing every 3 seconds.
task<>
report()
{
	// Systick counts down and is only 24 bits. wrapTick() counts the wrapping
	// around of the counter.
	systick_hw->csr = 0x5;
	systick_hw->rvr = 0x00FFFFFF;
	auto t0 = co_await events::sleep(0s);
	auto st0 = tickWrap - systick_hw->cvr;
	for (int i = 0; true; ++i) {
		auto t = co_await events::sleep(3s);
		auto stA = systick_hw->cvr;
		auto st1 = tickWrap - systick_hw->cvr;
		auto stB = systick_hw->cvr;
		std::cout << "Time " << 1e-6 * (t - t0) << " systick " << (st1 - st0) << " write ticks "
		          << writeTicks << " read ticks " << readTicks << std::endl;
		st0 = st1;
	}
}

int
main()
{
	stdio_init_all();
	// Start the main loop with two tasks.
	loop_control.loop(print_temp(), blink(), report(), wrapTick());
	return 0;
}