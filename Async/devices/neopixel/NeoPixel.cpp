//
// Created by Michel Lesoinne on 6/18/22.
//

#include "NeoPixel.h"

NeoPixel::NeoPixel(uint pin, uint numLeDs, PIO pio, int sm)
    : pin(pin), numLEDs(numLeDs), pio(pio), sm(sm)
{
}

uint32_t packColor(const RGBValues &rgb){
	return
    (static_cast<uint32_t>(rgb[0]) << 8) |
    (static_cast<uint32_t>(rgb[1]) << 16) |
    (static_cast<uint32_t>(rgb[2]));
}

task<>
NeoPixel::show()
{
	const int RGB_Bits = 24;
	if (!initialized) {
		uint offset = pio_add_program(pio, &ws2812_program);
		ws2812_program_init(pio, sm, offset, pin, 800000, RGB_Bits);
		initialized = true;
	}
	for (const auto &rgb: data) {
		pio_sm_put_blocking(pio0, 0, packColor(rgb) << 8u);
	}
	co_return;// data.size();
}
