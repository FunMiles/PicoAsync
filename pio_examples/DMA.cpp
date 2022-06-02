//
// Created by Michel Lesoinne on 6/1/22.
//
#include <span>

using std::span;
#include "DMA.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

// Communication back from the interrupt.
bool is_triggered{false};

int pio_dma_chan;

extern "C" void
dma_handler()
{

	is_triggered = true;
	// Clear the interrupt request.
	dma_hw->ints0 = 1u << pio_dma_chan;
}

void
initInputDMA(PIO pio, uint sm, span<uint32_t> buffer)
{
	// Allocate a DMA channel to feed the pin_ctrl SM its command words
	pio_dma_chan = dma_claim_unused_channel(true);

	dma_channel_config pio_dma_chan_config = dma_channel_get_default_config(pio_dma_chan);
	// Transfer 32 bits each time
	channel_config_set_transfer_data_size(&pio_dma_chan_config, DMA_SIZE_32);
	// Read from the same address (the PIO SM FX FIFO)
	channel_config_set_read_increment(&pio_dma_chan_config, false);
	// Increment write address (a different word in the buffer each time)
	channel_config_set_write_increment(&pio_dma_chan_config, true);

	// Transfer when PIO SM RX FIFO has data
	channel_config_set_dreq(&pio_dma_chan_config, pio_get_dreq(pio, sm, false));

	// Setup the channel and set it going
	dma_channel_configure(pio_dma_chan, &pio_dma_chan_config,
	                      buffer.data(), // Write to the buffer
	                      &pio->rxf[sm], // Read from PIO RX FIFO
	                      buffer.size(), // Amount of data to read.
	                      false          // don't start yet
	);

	// Tell the DMA to raise IRQ line 0 when the channel finishes a block
	dma_channel_set_irq0_enabled(pio_dma_chan, true);

	// Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
	irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
	irq_set_enabled(DMA_IRQ_0, true);

	is_triggered = false;
}

void
triggerDMAReceive(span<uint32_t> buffer)
{
	is_triggered = false;
	dma_channel_set_write_addr(pio_dma_chan, buffer.data(), true);
}