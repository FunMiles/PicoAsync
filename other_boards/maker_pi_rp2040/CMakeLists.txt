add_executable(maker_pi_rp2040)

target_sources(maker_pi_rp2040 PRIVATE maker_pi_rp2040.cpp)

target_link_libraries(maker_pi_rp2040
        PRIVATE Async pico_stdlib pico_multicore neopixel hardware_pwm)

pico_add_extra_outputs(maker_pi_rp2040)

# Enable USB output, disable UART output.
pico_enable_stdio_usb(maker_pi_rp2040 1)
pico_enable_stdio_uart(maker_pi_rp2040 0)