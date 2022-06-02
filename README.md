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
task<> report() {
    auto t0 = co_await sleep(0s);
    while(true) {
        auto t = co_await sleep(3s);
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
  loop_control.loop(blink(led_pin), report());
}
```

It is hoped that many programmers will find this approach
to embedded programming easy, thanks to the expressive syntax
and the avoidance of what is sometime affectionately referred to
as callback hell.

## Examples

There are 3 examples:
- main - 3 tasks: flashes the LED; prints a timer; prints when pin 26 experiences
an edge up or down.
- pio_examples/DS1820 - 4 tasks:
    - flashes the LED
    - read the temperature from a thermometer attached to pin 2
    - prints a timer every 3 seconds
    - count the number of time the system ticker wraps around.
- pio_examples/DS1820_DMA - 4 tasks as in the previous example but much
more efficient via the use of DMA/interrupt for the PIO's input FIFO.

The most interesting example is the last one. It demonstrates
the use of the pico's PIO system and how to use DMA for asynchronous input.

### TODO

The current temperature reading the DS1820_DMA returns the result of
the last read and not the current one. Suspending the task until it is available
should be implemented next.

## Notes and Credits
The code for pio_examples/DS1820 is based