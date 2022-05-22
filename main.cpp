#include "pico/stdlib.h"

int main()
{
	const uint led_pin = 25;

	gpio_init(led_pin);
	gpio_set_dir(led_pin, GPIO_OUT);
	while (true) {
		gpio_put(led_pin, 1);
		sleep_ms(250);
		gpio_put(led_pin, 0);
		sleep_ms(1500);
	}
}
