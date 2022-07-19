//
// Created by Michel Lesoinne on 6/18/22.
//

#pragma once
#include <vector>
#include <array>

#include "hardware/pio.h"
#include "ws2812.pio.h"

#include "../../task.h"

using RGBValues = std::array<uint8_t, 3>;

class NeoPixel {
public:
	NeoPixel(uint pin, uint numLEDs, PIO pio, int sm);

	void clear() { data.clear(); }
	void push_back(const RGBValues &rgb) { data.push_back(rgb); }
	auto size() const { return data.size(); }
	/// \brief Display the current state.
	async::task<> show();
private:
	uint pin;
	uint numLEDs;
	PIO pio;
	int sm;
	bool initialized = false;

	std::vector<RGBValues> data;
};
