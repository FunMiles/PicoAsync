//
// Created by Michel Lesoinne on 6/26/22.
//

#pragma once
#include <array>

// For debug only
#include <iostream>

#include <hardware/uart.h>

#include <Async/hw/DMA.h>
#include <vector>

#include <Async/loop_control.h>

namespace async::hw {

class UART {
	static constexpr uint size_bits = 8;
	static constexpr uint bufferSize = 1u << size_bits;
	static constexpr uint sizeMask = bufferSize-1;
	static constexpr uint watermark = 16;
public:
	UART(uart_inst *uart, uint tx_pin, uint rx_pin, uint baud_rate = 115200);
	auto send(std::span<const char> data) {
		return dmaTXChannel.send(data);
	}
	template <size_t N>
	auto send(const char null_terminated_string[N]) {
		return send({null_terminated_string, N-1});
	}

	auto getChannel() const { return dmaTXChannel.getChannel(); }

	// Receiving code
	auto receive(std::span<char> userBuffer) {
		struct awaiter {
			~awaiter() {
				if (uartIdx != -1) {
					awaitingHandles[uartIdx] = {};
					triggeredHandles[uartIdx] = {};
				}
			}
			awaiter(UART *receiver, const std::span<char> &destBuffer, uint uartIdx)
			    : receiver(receiver), destBuffer(destBuffer), uartIdx(uartIdx) {}
			awaiter(const awaiter &) = delete;
			awaiter(awaiter &&o) : receiver(o.receiver),
			                       destBuffer(o.destBuffer),
			                       uartIdx(o.uartIdx) {
				o.uartIdx = -1;
			}
			bool await_ready() {
				return !rxBuffers[uartIdx].empty();
			}
			void await_suspend(std::coroutine_handle<> h) {
				awaitingHandles[uartIdx] = h;
			}
			//		my_span<char>
			int await_resume() {
				uint i = 0;
				for (; i < destBuffer.size() && !rxBuffers[uartIdx].empty(); ++i)
					destBuffer[i] = rxBuffers[uartIdx].get();
				return i;//destBuffer.subspan(0, i);
			}
			UART *receiver;
			std::span<char> destBuffer;
			uint uartIdx=-1;
		};
		return awaiter{this, userBuffer, uart == uart0 ? 0u : 1u};
	}
	static auto empty(int i) { return rxBuffers[i].empty(); }
	static inline int64_t  itr_count{0};
	static inline int64_t  timeout_count{0};
	static inline int64_t  char_count{0};


private:
	DMAChannel dmaTXChannel;

	uart_inst *uart;

	struct Buffer {
		Buffer() {}
		[[nodiscard]] bool full() const { return ((writeIndex+1) & sizeMask) == readIndex; }
		[[nodiscard]] bool empty() const { return writeIndex == readIndex; }

		void put(char c) {
			data[writeIndex] = c;
			writeIndex = ((writeIndex+1) & sizeMask);
		}
		int get() {
			if (empty())
				return -1;
			auto c = data[readIndex];
			readIndex = (readIndex+1) & sizeMask;
			return c;
		}
		std::vector<char> data;
		volatile uint writeIndex = 0;
		volatile uint readIndex = 0;
		bool overFlow = false;
	};

	static inline std::array<Buffer,2> rxBuffers;
	static inline std::array<std::coroutine_handle<>,2> awaitingHandles;
	static inline std::array<std::coroutine_handle<>,2> triggeredHandles;

	template <uint uart_index>
	static void __not_in_flash_func(irq_receive)();
};

// Hack
template <typename T>
struct my_span {
	my_span(T *t, size_t l) : t(t), l(l) {}
	my_span(std::span<T> s) : t(s.data()), l(s.size()) {}
	operator std::span<T>() { return {t,l}; }
	auto data() { return t; }
	auto data() const { return t; }
	auto size() const { return l; }
	auto &operator[](size_t i) { return t[i]; }
	my_span subspan(size_t from, size_t to) { return {t+from, to-from}; }
	T *t;
	size_t l;
};

struct S {
	char *t;
	size_t l;
};

}// namespace async::hw
