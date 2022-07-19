//
// Created by Michel Lesoinne on 6/26/22.
//
#include "UART.h"
#include <hardware/gpio.h>
#include <loop_control.h>

namespace async::hw {

uart_hw_t volatile *hw0 = reinterpret_cast<uart_hw_t *>(((char *)0)+UART0_BASE);
uart_hw_t volatile *hw1 = reinterpret_cast<uart_hw_t *>(((char *)0)+UART1_BASE);

UART::UART(uart_inst *uart, uint tx_pin, uint rx_pin, uint baud_rate)
    : uart(uart),
      dmaTXChannel(
              {.transferSize = DMA_SIZE_16,
               .dreq = uart_get_dreq(uart, false),
               .read_addr = &uart_get_hw(uart)->dr},
              {.transferSize = DMA_SIZE_8,
               .dreq = uart_get_dreq(uart, true),
               .write_addr = &uart_get_hw(uart)->dr})
{
	// Set up our UART with the required speed.
	// It also enables FIFO
	uart_init(uart, baud_rate);
	// Set the TX and RX pins by using the function select on the GPIO
	// Set datasheet for more information on function select
	gpio_set_function(tx_pin, GPIO_FUNC_UART);
	gpio_set_function(rx_pin, GPIO_FUNC_UART);

	auto *uart_hw = uart_get_hw(uart);
	auto uart_irq = (uart_hw == hw0) ? UART0_IRQ : UART1_IRQ;
	uint uart_index = (uart == uart0) ? 0 : 1;
	rxBuffers[uart_index].data.resize(bufferSize);
	irq_handler_t irq_fct  = (uart == uart0) ? &UART::irq_receive<0> : &UART::irq_receive<1>;
	irq_set_exclusive_handler(uart_irq, irq_fct);

	uint32_t fillBits = watermark >= 16 ? 0b010 : 0b001;
	uart_hw->imsc = UART_UARTIMSC_RXIM_BITS  // IRQ when receive FIFO above watermark.
	                | UART_UARTIMSC_RTIM_BITS; // IRQ on receive timeout.
	hw_write_masked(&uart_hw->ifls,
	                fillBits << UART_UARTIFLS_RXIFLSEL_LSB,
	                UART_UARTIFLS_RXIFLSEL_BITS);
	irq_set_enabled(uart_irq, true);
}


template<uint uart_index>
void __not_in_flash_func(UART::irq_receive)()
{
	auto hw = uart_index == 0 ? hw0 : hw1;
	auto irq_status = hw->mis;
	// Clear the interrupt for the current status
	hw->icr = irq_status;
	if (irq_status & UART_UARTRIS_RTRIS_BITS)
		++timeout_count;
	++itr_count;
	if (!rxBuffers[uart_index].data.empty()) {
		auto &buffer = rxBuffers[uart_index];
		uint maxChar = bufferSize-1;// : watermark;
		// If we have space and the FIFO is not empty.
		for (uint i = 0; i < maxChar && ! (hw->fr & UART_UARTFR_RXFE_BITS); ++i)
		{
			if (!buffer.full())
				buffer.put( (uint8_t) hw->dr );
			else {
				// consume the next character
				uint8_t dummy = hw->dr;
				buffer.overFlow = true;
			}
			++char_count;
		}
		// Awaken user-side awaiter if any
		auto t = awaitingHandles[uart_index];
		if (t) {
			triggeredHandles[uart_index] = t;
			// Clear the handle
			awaitingHandles[uart_index] = {};
			uint32_t save = save_and_disable_interrupts();
			core_loop().scheduleInterruptAction(
			        [](loop_control::ActionArg arg) {
				        triggeredHandles[arg.u].resume();
				        triggeredHandles[arg.u] = {};
			        },
			        {.u = uart_index}
			);

			//			core_loop().scheduleInterruptAction(t);
			restore_interrupts(save);
		}
	}
}

}// namespace async::hw