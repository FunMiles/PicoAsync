//
// Created by Michel Lesoinne on 6/18/22.
//
#include "Async/hw/DMA.h"

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
	static std::array<uint32_t,32> buffer;
	static std::array<uint32_t,32> dmaSendBuffer;
	static async::hw::DMAChannelConfig readConfig{.transferSize = DMA_SIZE_32,
	                                              .dreq = pio_get_dreq(pio, sm, false),
	                                              .write_addr = buffer.data(),
	                                              .read_addr = &pio->rxf[sm],
	                                              .transfer_count = buffer.size()};
	static async::hw::DMAChannelConfig writeConfig{.transferSize = DMA_SIZE_32,
	                                               .dreq = pio_get_dreq(pio, sm, true),
	                                               .write_addr = &pio->txf[sm],
	                                               .read_addr = dmaSendBuffer.data(),
	                                               .transfer_count = buffer.size()};
	static async::hw::DMAChannel dmaChannel(readConfig, writeConfig);
	const int RGB_Bits = 24;
	if (!initialized) {
		uint offset = pio_add_program(pio, &ws2812_program);
		ws2812_program_init(pio, sm, offset, pin, 800000, RGB_Bits);
		initialized = true;
	}
	const bool is_old = false;
	if (is_old)
	for (const auto &rgb: data) {
		pio_sm_put_blocking(pio0, sm, packColor(rgb) << 8u);
	}
	else {
		for (int i = 0; const auto &rgb : data) {
			dmaSendBuffer[i++] = packColor(rgb) << 8u;
		}
		co_await dmaChannel.send(std::span<uint32_t>(dmaSendBuffer.data(), data.size()));
	}
	co_return;
}
