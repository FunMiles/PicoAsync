//
// Created by Michel Lesoinne on 6/1/22.
//
#include <stdio.h>
#include <array>
#include <span>
#include "pico/stdlib.h"

#include "Async/events.h"
#include "Async/task.h"

#include "hardware/gpio.h"
#include "DS1820.pio.h"

#include "hardware/structs/systick.h"

using namespace std::chrono_literals;

using namespace std;

uint tickWrap=0;

uint wpCount{0x1000000u};

task<> wrapTick() {
  auto tick = systick_hw->cvr;
  while(true) {
    co_await events::sleep(3ms);
    auto tick2 = systick_hw->cvr;
    if (tick2 > tick)
      tickWrap += 0x1000000u;
    tick = tick2;
    ++wpCount;
  }
}

uint32_t getTicks() {
  return tickWrap - systick_hw->cvr;
}

uint8_t crc8(span<uint8_t> data)
{
	uint8_t crc = 0;
	for (int i = 0; i < data.size(); i++)
	{
		auto databyte = data[i];
		for (int j = 0; j < 8; j++)
		{
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
void writeBytes(PIO pio, uint sm, span<uint8_t> bytes)
{
        auto t0 = getTicks();
        auto len = bytes.size();
	pio_sm_put_blocking(pio, sm, 250);
	pio_sm_put_blocking(pio, sm, len - 1);
	for (int i = 0; i < len; i++)
	{
		pio_sm_put_blocking(pio, sm, bytes[i]);
	}
        writeTicks = getTicks() - t0;
}

uint32_t readTicks;
void readBytes(PIO pio, uint sm, span<uint8_t> bytes)
{
        auto t0 = getTicks();
	pio_sm_put_blocking(pio, sm, 0);
	pio_sm_put_blocking(pio, sm, bytes.size() - 1);
	for (int i = 0; i < bytes.size(); i++)
	{
		bytes[i] = pio_sm_get_blocking(pio, sm) >> 24;
	}
        readTicks = getTicks() - t0;
}

task<float> getTemperature(PIO pio, uint sm)
{
	uint8_t w1[2] = {0xCC, 0x44};
	writeBytes(pio, sm, w1);
        co_await events::sleep(1000ms);
	uint8_t w2[2] = {0xCC, 0xBE};
	writeBytes(pio, sm, w2);
	array<uint8_t,9> data;
	readBytes(pio, sm, data);
	uint8_t crc = crc8(data);
	if (crc != 0)
		co_return -2000.0f;
	int t1 = data[0];
	int t2 = data[1];
	int16_t temp1 = (t2 << 8 | t1);
	volatile float temp = (float)temp1 / 16;
	co_return temp;
}
uint DS18Initalize(PIO pio, int gpio)
{
	uint offset = pio_add_program(pio, &DS1820_program);
	uint sm = pio_claim_unused_sm(pio, true);
	pio_gpio_init(pio, gpio);
	pio_sm_config c = DS1820_program_get_default_config(
	        offset);
	sm_config_set_clkdiv_int_frac(&c, 255, 0);
	sm_config_set_set_pins(&c, gpio, 1);
	sm_config_set_out_pins(&c, gpio, 1);
	sm_config_set_in_pins(&c, gpio);
	sm_config_set_in_shift(&c, true, true, 8);
	pio_sm_init(pio0, sm, offset, &c);
	pio_sm_set_enabled(pio0, sm, true);
	return sm;
}

task<>
print_temp()
{
  uint sm = DS18Initalize(pio0, 2);
  for (;;)
  {
    float t;
    do
    {
      t = co_await getTemperature(pio0, sm);
    } while (t < -999);
    printf("temperature %f\r\n", t);
    co_await events::sleep(500ms);
  };
}

const uint led_pin = 25;
const uint button_pin = 16;
/// Task blinking the LED twice per second.
task<> blink() {
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
task<> report() {
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
    std::cout << "Time " << 1e-6*(t-t0) << " systick " << (st1-st0) << " write ticks " << writeTicks
              << " read ticks " << readTicks << std::endl;
    st0 = st1;
  }
}


int main()
{
  stdio_init_all();
  // Start the main loop with two tasks.
  loop_control.loop(print_temp(), blink(), report(), wrapTick());
  return 0;
}