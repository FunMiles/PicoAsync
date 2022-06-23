//
// Created by Michel Lesoinne on 6/21/22.
//

#pragma once
#include <coroutine>
#include <iostream>
#include <span>

#include <hardware/dma.h>
#include <pico/multicore.h>

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

	DMAChannel(const DMAChannelConfig &readConfig);
	DMAChannel(const DMAChannelConfig &readConfig,
	           const DMAChannelConfig &writeConfig);
	~DMAChannel();

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

		// In case a transfer was abandoned...
		channelTargets[get_core_num()][channel] = {};
		// Initiate the transfer
		if (lastConfig != read) {
			// Setup the channel and start
			dma_channel_configure(channel, &readChannelConfig,
			                      buffer.data(),  // Write to the buffer
			                      read_addr, // Read from source
			                      buffer.size(), // Amount of data to write.
			                      true          // start
			);
			std::cout << "Set up the channel for reading!!!" << std::endl;
			dma_channel_set_irq0_enabled(channel, true);
			std::cout << "IRQ set" << std::endl;
			lastConfig = read;
		} else {
			dma_channel_transfer_to_buffer_now(channel, buffer.data(), buffer.size());
		}
		std::cout << "Returning" << std::endl;
		return awaitable{channel, buffer};
	}

	template <typename T>
	auto send(std::span<T> buffer)
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

		// In case a transfer was abandoned...
		channelTargets[get_core_num()][channel] = {};
		// Initiate the transfer
		if (lastConfig != write) {
			dma_channel_set_irq0_enabled(channel, true);
			std::cout << "IRQ Enabled" << std::endl;
			// Setup the channel
			dma_channel_configure(channel, &writeChannelConfig,
			                      write_addr, // Write to the peripheral
			                      buffer.data(), // Read from source
			                      buffer.size(), // Amount of data to write.
			                      true          // Start
			);
			std::cout << "Set up the channel for writing. "<< buffer.size() << std::endl;

			lastConfig = write;
		} else {
			dma_channel_set_trans_count(channel, buffer.size(), false);
			dma_channel_set_read_addr(channel, buffer.data(), true);
		}

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
	dma_channel_config readChannelConfig;
	dma_channel_config writeChannelConfig;
	volatile void *write_addr;
	volatile const void *read_addr;
	enum Op { none, read, write };
	Op lastConfig;
	static void
	__not_in_flash_func(dma_irq_handler)();
};

}