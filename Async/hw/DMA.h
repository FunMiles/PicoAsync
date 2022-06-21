//
// Created by Michel Lesoinne on 6/21/22.
//

#pragma once
#include <coroutine>

#include <hardware/dma.h>

namespace async::hw {

struct DMAChannelConfig {
	dma_channel_transfer_size transferSize;
	uint dreq;
	volatile void *write_addr;
	volatile const void *read_addr;
	uint transfer_count;
};



class DMAChannel {
public:

	DMAChannel(const DMAChannelConfig &config);
	template <typename T>
	auto receive(std::span<T> buffer)
	{
		struct awaitable {
			bool await_ready() {
				return channelTargets[get_core_num()][channel].done;
			}
			std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
				if (channelTargets[get_core_num()][channel].done) {
					channelTargets[get_core_num()][channel] = {};
					return h;
				}
				// Set the handle to be rescheduled by the interrupt
				channelTargets[get_core_num()][channel].handle = h;
				return std::noop_coroutine();
			}
			auto await_resume() { return buffer.data(); }
			int channel;
			std::span<T> buffer;
		};

		// Initiate the transfer
		dma_channel_set_write_addr(channel, buffer.data(), true);

		return awaitable{channel, buffer};
	}

private:
	static constexpr const uint numCores = 2;
	static constexpr const uint numChannels = 16;
	struct Awaiting {
		bool done{false};
		std::coroutine_handle<> handle;
		Awaiting(){};
	};
	static inline Awaiting channelTargets[numCores][numChannels];
	/// \brief DMA Hardware channel
	int channel;

	void initiateReceive();

	static void
	__not_in_flash_func(dma_irq_handler)();
};

}