//
// Demo created on 7/19/22.
//
//
// Created by Michel Lesoinne on 6/24/22.
//
#include <iostream>
#include <regex>
//
#include "Async/events.h"
#include "Async/task.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include <Async/hw/UART.h>

using namespace std;
using namespace std::chrono_literals;
using namespace async;
// Make it possible to await a duration
using events::operator co_await;
#include <Async/compose.h>
#include <device/usbd.h>

using namespace async;

uint led_pin = 25;

/// Task blinking the LED with a given delay between toggling on/off
task<>
blink(uint pin, uint delay)
{
	gpio_init(pin);
	gpio_set_dir(pin, GPIO_OUT);

	while (true) {
		co_await events::sleep(delay * 1ms);
		gpio_put(pin, 1);
		co_await events::sleep(delay * 1ms);
		gpio_put(pin, 0);
	}
}

using async::hw::UART;

task<span<char>>
read_line(auto &receiver, span<char> buffer, chrono::duration<uint64_t, std::micro> timeout)
{
	auto t0 = time_us_64();
	size_t offset = 0; // Offset to the end of the already filled part of the buffer
	do {
		auto unfilled = buffer.subspan(offset);
		auto timeout_receive = co_await either(receiver.receive(unfilled), events::sleep(timeout));
		if (timeout_receive.index() == 1)
			co_return buffer.subspan(0, offset);
		auto n = std::get<0>(timeout_receive);
		for (size_t i = 0; i < n; ++i) {
			if (unfilled[i] == '\n')
				co_return buffer.subspan(0, offset+n);
		}
		offset += n;
		if (offset == buffer.size())
			co_return buffer;
	} while(true);
}

/** \brief Read data from UART until "OK" or "ERROR" are found, or time out.
 *
 * @param receiver UART object.
 * @param timeout Timeout after which the coroutine will return false.
 * @return true if "OK" was received, false if "ERROR" was received or timeout was exceeded.
 */
task<bool> read_till_result(auto &receiver, chrono::duration<uint64_t, std::micro> timeout) {
	char buffer[256];

	regex ok_regex{"OK"};
	regex error_regex("ERROR");
	while (true) {
		auto sp = co_await read_line(receiver, buffer, timeout);
		auto inString = string_view{sp.data(), sp.size()};
		if(inString.size() == 0)
			co_return false;
		std::cout << inString;
		if(regex_search(inString.begin(), inString.end(), ok_regex))
			co_return true;
		if(regex_search(inString.begin(), inString.end(), error_regex))
			co_return false;

	}
}

/// \brief Task driving an ESP8266 ES-1 Wifi module connected to pins 4 and 5.
task<> setup_es_1()
{
	// Wait 4s to give a user time to start USB output printing program 
	co_await 4s;
	std::cout << "Starting." << std::endl;

	const char *failure_message = "Failed to get an OK, giving up.";
	auto uart_id = uart1;
	uint tx_pin = 4;
	uint rx_pin = tx_pin + 1;
	uint baud_rate = 115200;

	async::hw::UART uart(uart_id, tx_pin, rx_pin, baud_rate);
	auto &receiver = uart;

	std::cout << "Ready" << std::endl;
	co_await 10ms;
	std::cout << "Putting things" << std::endl;
	co_await uart.send("AT\r\n");
	if (!co_await read_till_result(receiver, 10ms)) {
		std::cout << failure_message << std::endl;
		co_return;
	}
	std::cout << "Sending AT+CWMODE=1" << std::endl;
	// Send out a string, with CR/LF conversions
	co_await uart.send("AT+CWMODE=1\r\n");
	if (!co_await read_till_result(receiver, 2s)) {
		std::cout << failure_message << std::endl;
		co_return;
	}
	const char connedCmd[] = "AT+CWJAP=\"" WIFI_SSID"\",\"" WIFI_PASSWORD "\"\r\n";
	std::cout << "  - Connecting to WiFi..." << std::endl;
	co_await uart.send(connedCmd);
	if (!co_await read_till_result(receiver, 10s)) {
		std::cout << failure_message << std::endl;
		co_return;
	}
	co_await uart.send( "AT+CIPSTA?\r\n");
	if (!co_await read_till_result(receiver, 200ms)) {
		std::cout << failure_message << std::endl;
		co_return;
	}
	std::cout << "  - Starting Webserver (1).." << std::endl;
	co_await uart.send("AT+CIPMUX=1\r\n");
	if (!co_await read_till_result(receiver, 2s)) {
		std::cout << failure_message << std::endl;
		co_return;
	}
	std::cout << "  - Starting Webserver (2).." << std::endl;
	co_await uart.send("AT+CIPSERVER=1,80\r\n");
	if (!co_await read_till_result(receiver, 10ms)) {
		std::cout << failure_message << std::endl;
		co_return;
	}
	std::cout << "Web server should be running" << std::endl;
	char buffer[256];
	while(true) {
		auto data = co_await receiver.receive(buffer);
		std::cout << string_view(buffer, data);
	}
}


int
main()
{
	stdio_init_all();
	async::hw::UART dummy(uart0, 0,1);
	multicore_launch_core1( []() {
		core_loop().loop(setup_es_1());
	});
	// Start the main loop with three tasks.
	loop_control.loop(blink(led_pin, 250));
}