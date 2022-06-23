//
// Created by Michel Lesoinne on 6/21/22.
//
#include <span>
#include <bit>

#include <hardware/dma.h>
#include "hardware/irq.h"
#include "pico/multicore.h"

#include "DMA.h"
#include "task.h"
#include "loop_control.h"

using std::span;

namespace async::hw {

void
__not_in_flash_func(DMAChannel::dma_irq_handler)()
{
	// Get the channel causing the interrupt.
    auto status = dma_hw->ints0;
	// Acknowledge the interrupt.
	// Clear the interrupt request.
	dma_hw->ints0 = status;
	// Pass it on for processing
	auto work = [](decltype(status) st) -> task<> {
		auto channel = std::countr_zero(st);
		auto &target = DMAChannel::channelTargets[get_core_num()][channel];
		// It is possible that the DMA transfer completed before it is awaited.
		// So there may not be yet an awaiting coroutine handle.
		if (target.handle) {
			// We can resume here. This coroutine is running directly from
			// the loop and the stack use is small until now.
			target.handle.resume();
			target = {};
		} else {
			// The awaitable will not pause the caller's execution.
			target.done = true;
		}
		// Mark there's something or schedule the awaiting
		co_return;
	};
	auto t = work(status);
	uint32_t save = save_and_disable_interrupts();
	core_loop().scheduleInterruptAction(t.get_handle());
	restore_interrupts(save);
}

DMAChannel::DMAChannel(const DMAChannelConfig &readConfig)
{
	static int done =
	    [] {
		    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
		    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
		    irq_set_enabled(DMA_IRQ_0, true);
		    return 1;
	    }();
	// Allocate a DMA channel panic if none available.
	channel = dma_claim_unused_channel(true);
	std::cout << "Channel: " << channel << std::endl;
	dma_channel_config &channel_config = readChannelConfig;
	channel_config = dma_channel_get_default_config(channel);
	// Set the transfer size
	channel_config_set_transfer_data_size(&channel_config, readConfig.transferSize);
	// Read from the same address (the PIO SM FX FIFO)
	channel_config_set_read_increment(&channel_config, false);
	// Increment write address (a different word in the buffer each time)
	channel_config_set_write_increment(&channel_config, true);
	// Transfer based on the given transfer request signal
	channel_config_set_dreq(&channel_config, readConfig.dreq);
	// Setup the channel
	lastConfig = Op::none;
	read_addr = readConfig.read_addr; // Needed for late activation.
	// Tell the DMA to raise IRQ line 0 when the channel finishes a block
	dma_channel_set_irq0_enabled(channel, true);
}

DMAChannel::DMAChannel(const DMAChannelConfig &readConfig2,
                       const DMAChannelConfig &writeConfig)
: DMAChannel(readConfig2)
{
	writeChannelConfig = dma_channel_get_default_config(channel);
	// Set the transfer size
	channel_config_set_transfer_data_size(&writeChannelConfig, writeConfig.transferSize);
	// Increment the read address (a different word in the buffer each time)
	channel_config_set_read_increment(&writeChannelConfig, true);
	// Write to the same address each time
	channel_config_set_write_increment(&writeChannelConfig, false);
	// Transfer based on the given transfer request signal
	channel_config_set_dreq(&writeChannelConfig, writeConfig.dreq);
	write_addr = writeConfig.write_addr; // needed for activation
}
DMAChannel::~DMAChannel()
{
	dma_channel_set_irq0_enabled(channel, false);
	// Cancel any ongoing DMA transfer on the channel.
	// Note: It returns only when canceled. Unknown timing.
	// Writing to the cancel register before disabling the IRQ might save some cycles
	dma_channel_abort(channel);
	dma_channel_unclaim(channel);
}
}
