#include "Async/events.h"
#include "Async/task.h"
#include "pico/stdlib.h"
#include <iostream>

using namespace std::chrono_literals;
const uint led_pin = 25;

/// Task blinking the LED twice per second.
task<> blink() {
  for (int i = 0; true; ++i) {
    std::cout << "Task-based Blink++ " << i << std::endl;
    co_await events::sleep(250ms);
    gpio_put(led_pin, 1);
    co_await events::sleep(250ms);
    gpio_put(led_pin, 0);
  }
}

/// Task printing every 3 seconds.
task<> report() {
  for (int i = 0; true; ++i) {
    std::cout << "Three seconds x " << i << std::endl;
    co_await events::sleep(3s);
  }
}

int main() {

  gpio_init(led_pin);
  gpio_set_dir(led_pin, GPIO_OUT);

  stdio_init_all();

  // Start the main loop with two tasks.
  loop_control.loop(blink(), report());
}
