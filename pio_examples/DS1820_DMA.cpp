//
// Created by Michel Lesoinne on 6/1/22.
//
#include <array>
#include <device/usbd.h>
#include <span>
#include <stdio.h>

#include "Async/events.h"
#include "Async/task.h"

#include "DS1820_DMA.pio.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "hardware/structs/systick.h"

#include "Async/hw/DMA.h"

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
sendHeader(PIO pio, uint sm, uint16_t first, uint16_t num_bytes)
{
	pio_sm_put_blocking(pio, sm, first);
	pio_sm_put_blocking(pio, sm, 8*num_bytes-1);
}

void
writeBytes(PIO pio, uint sm, span<const uint8_t> bytes)
{
	auto t0 = getTicks();
	auto len = bytes.size();
	sendHeader(pio, sm, 250, len);
	for (int i = 0; i < len; i += 2) {
		uint16_t v = bytes[i] | (bytes[i + 1] << 8u);
		pio_sm_put_blocking(pio, sm, v);
	}
	writeTicks = getTicks() - t0;
}

uint32_t readTicks;

namespace {
static array<uint32_t, 9> buffer;

}

task<float>
getTemperature_DMA(PIO pio, uint sm)
{
	static bool               first = true;

	static async::hw::DMAChannel dmaChannel({
	    .transferSize = DMA_SIZE_32,
	    .dreq =  pio_get_dreq(pio, sm, false),
	    .write_addr = buffer.data(),
	    .read_addr = &pio->rxf[sm],
	    .transfer_count = buffer.size()
	});
	auto t0 = getTicks();
	writeBytes(pio, sm, ((uint8_t[]){0xCC, 0x44}));
	auto t1 = getTicks();
	co_await events::sleep(1000ms);
	auto t2 = getTicks();

	writeBytes(pio, sm, ((uint8_t[]){0xCC, 0xBE}));
	auto awaitable = dmaChannel.receive(std::span<uint32_t>{buffer});
	// Tell the PIO code we want to read and how much data we want
	sendHeader(pio, sm, 0, buffer.size());
	auto t3 = getTicks();
	readTicks = t1-t0 +t3 - t2;
	auto b = co_await awaitable;
	float temperature = - 1000.0f;
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
	co_return temperature;
}

uint
DS18Initalize(PIO pio, int gpio)
{
	uint offset = pio_add_program(pio, &DS1820_program);
	uint sm = pio_claim_unused_sm(pio, true);
	pio_gpio_init(pio, gpio);
	pio_sm_config c = DS1820_program_get_default_config(offset);
	sm_config_set_clkdiv_int_frac(&c, 511, 0);
	sm_config_set_set_pins(&c, gpio, 1);
	sm_config_set_out_pins(&c, gpio, 1);
	sm_config_set_out_shift(&c, true, true, 16);
	sm_config_set_in_pins(&c, gpio);
	// Set the shifting to the right as we receive LSB to HSB order.
	// Automatic shift of data to the output FIFO after 8 bits have been shifted in.
	sm_config_set_in_shift(&c, true, true, 8);
	pio_sm_init(pio0, sm, offset, &c);
	pio_sm_set_enabled(pio0, sm, true);
	return sm;
}

// To be able to co_await a duration. E.g. co_await 3ms;
using events::operator co_await;

/// \brief Continuously print the temperature.
task<>
print_temp()
{
	uint sm = DS18Initalize(pio0, 2);

	co_await 2s;
	std::cout << "Starting the temperature loop." << std::endl;
	while(true) {
		float t;
		do {
			co_await events::sleep(1s);
			t = co_await getTemperature_DMA(pio0, sm);
		} while (t < -999);
		std::cout << "Temperature: " << t << std::endl;
	};
}
/// \brief Create a task blinking the LED twice per second.
task<>
blink()
{
	gpio_init(led_pin);
	gpio_set_dir(led_pin, GPIO_OUT);

	for (int i = 0; true; ++i) {
		co_await 250ms;
		gpio_put(led_pin, true);
		co_await 250ms;
		gpio_put(led_pin, false);
	}
}

/// \brief Create a task printing time and timings every 3 seconds.
task<>
report()
{
	// Systick counts down and is only 24 bits. wrapTick() counts the wrapping
	// around of the counter.
	systick_hw->csr = 0x5;
	systick_hw->rvr = 0x00FFFFFF;
	auto t0 = co_await 0s;
	auto st0 = getTicks();
	for (int i = 0; true; ++i) {
		auto t = co_await 3s;
		auto st1 = getTicks();
		std::cout << "Time " << 1e-6 * (t - t0) << " systick " << (st1 - st0) << " write ticks "
		          << writeTicks << " read ticks " << readTicks << std::endl;
		st0 = st1;
	}
}

/// \brief Task checking for USB input character, printing each one on std::cout.
task<>
input() {
	while(true) {
		auto c = co_await events::one_char();
		std::cout << "Input: " << c << std::endl;
	}
}

int
main()
{
	stdio_init_all();
	// Handle the USB input.
	loop_control.add_constant_processor(tud_task);
	// Start the main loop with 5 tasks.
	loop_control.loop(print_temp(), blink(), report(), input(), wrapTick());
	return 0;
}