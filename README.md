# Coroutine Based Asynchronous Embedded Programming

The purpose of this library is to make it easier to write concurrent
processing tasks for embedded systems, starting with the 
Raspberry Pi Pico.

Using the coroutine system, it becomes easy to write individual tasks
that are concurrent, sharing the processor. As long as each task 
awaits some event on a regular basis, other tasks can execute during
that yielded time.

Example of a pin toggling task:
```cpp
task<> blink(int pin) {
    while (true) {
        co_await sleep(250ms);
        gpio_put(led_pin, 1);
        co_await sleep(250ms);
        gpio_put(led_pin, 0);	
    }
}
```
while a task doing a periodic printing every 3 seconds would be:
```cpp
task<> report(int num_seconds) {
    auto t0 = co_await sleep(0s);
    while(true) {
        auto t = co_await sleep(num_seconds * 1s);
        std::cout << "Time " << 1e-6*(t-t0) << std::endl;
    }
}
```

The main for running the two tasks simultaneously is:
```cpp
int main()
{
  gpio_init(led_pin);
  gpio_set_dir(led_pin, GPIO_OUT);

  stdio_init_all();

  // Start the main loop with two tasks.
  loop_control.loop(blink(led_pin), report(3));
}
```

It is hoped that many programmers will find this approach
to embedded programming easy, thanks to the expressive syntax
and the avoidance of what is sometime affectionately referred to
as callback hell.

## Syntactic Sugar

To make any sleeping even shorter to write - and maybe easier to read, use
an import:

```cpp
// Make it possible to await a duration
using events::operator co_await;

task<> blink(int pin) {
    while (true) {
        co_await 250ms;
        gpio_put(led_pin, 1);
        co_await 250ms;
        gpio_put(led_pin, 0);	
    }
}
```

## Multi-core

With the asynchronous library, it is easy to start tasks on both cores.
```cpp
int main()
{
	gpio_init(led_pin);
	gpio_set_dir(led_pin, GPIO_OUT);
	stdio_init_all();
  
  	multicore_launch_core1( []() {
		// Start the loop with one task on core 1.
		core_loop().loop(report(7));
	});
	// Start the main loop on core 0 with two tasks.
	loop_control.loop(blink(led_pin), report(3));
}
```
Tasks get suspended and restarted on the core where they were first started.
In the future, a mechanism for moving a task from one core to the other will be added.
Intercore communication can currently be handled using direct calls to the sdk.
In the future, asynchronous awaitable objects will be provided by this library.

## Pico Hardware
The *async_hardware* library offers asynchronous interface to the following hardware components:
 - DMA Channels
 - UART
## Devices
The library will progressively add supported devices to make it easy
to use most common devices. 
Currently, the list of devices is:
 - [NeoPixel](Async/devices/neopixel/NeoPixel.md)
 - DS1820 : Currently found in the examples.
 - SSD1830 based OLED display
## Boards
The examples in the *example* directory work on the standard
Raspberry Pi Pico board. Demos for other boards with their specific 
hardware will be progressively added.

Currently, the list of other boards is:
 - [Maker Pi RP2040 board](other_boards/maker_pi_rp2040/MakerPiRP2040.md)
## Examples

There are 3 examples:
- main - 3 tasks: flashes the LED; prints a timer; prints when pin 26 experiences
an edge up or down.
- examples/ssd1306_demo - Assumes a 128x32 OLED display connected to GPIO pins 1 and 2: 3 tasks:
    - flashes the LED
    - draw a vertical line moving from left to right
    - draws three horizontal lines moving from right to left
- pio_examples/DS1820 - 4 tasks:
    - flashes the LED
    - read the temperature from a thermometer attached to pin 2
    - prints a timer every 3 seconds
    - count the number of time the system ticker wraps around.
- pio_examples/DS1820_DMA - 4 tasks as in the previous example but much
more efficient via the use of DMA/interrupt for the PIO's input FIFO. Plus one task
waiting for and printing any character input from the USB.

The most interesting example is the last one. It demonstrates
the use of the pico's PIO system and how to use DMA for asynchronous input.

## How to use this library in your own projects

To use this library in your own project, download the library and note the
directory where it is located. 

In your project, start with a normal Raspberry Pi Pico project CMakeLists.txt. 
Then add a few lines to add the library's `Async` directory as a subdirectory.
Because CMake requires an indirect way of using an external directory for
`add_subdirectory`, follow the approach given below which creates
a project with a single `my_async_app` executable target. Note that it seems that the
variable `ASYNC_EXTERNAL_SOURCE_DIR` must be defined for CMake to
accept the external subdirectory:
```cmake
cmake_minimum_required(VERSION 3.20)
include ($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(MyAsyncProject)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
# Creates a pico-sdk folder.
pico_sdk_init()

# MODIFY THE FOLLOWING LINE WITH THE CORRECT PATH OR USE AN
# ENVIRONMENT VARIABLE SUCH AS $ENV{ASYNC_PICO_PATH}
set(ASYNC_PICO_DIR /where/you/have/AsyncPico)
set(ASYNC_EXTERNAL_SOURCE_DIR ${ASYNC_PICO_DIR}/Async CACHE PATH "Async")
add_subdirectory(${ASYNC_EXTERNAL_SOURCE_DIR} Async)

add_executable(my_async_app main.cpp)
# You can now use any library from AsyncPico
target_link_libraries(my_async_app
        Async ssd1306
        pico_stdlib pico_multicore)
pico_add_extra_outputs(my_async_app)

# Enable USB output, disable UART output.
pico_enable_stdio_usb(my_async_app 1)
pico_enable_stdio_uart(my_async_app 0)
```
## Notes and Credits
The code for pio_examples/DS1820 is based on code for
[A 1-Wire PIO Program](https://www.i-programmer.info/programming/hardware/14527-the-pico-in-c-a-1-wire-pio-program.html).
The DMA version modifies the pio code to be more efficient.
