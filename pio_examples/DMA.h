//
// Created by Michel Lesoinne on 6/1/22.
//

#pragma once
#include <span>

#include "hardware/pio.h"
#include "pico/stdlib.h"

extern bool is_triggered;

void initInputDMA(PIO pio, uint sm, std::span<uint32_t> buffer);
void triggerDMAReceive(std::span<uint32_t> buffer);