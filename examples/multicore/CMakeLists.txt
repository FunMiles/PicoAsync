add_executable(simple_multicore)

target_sources(simple_multicore PRIVATE simple.cpp)

target_link_libraries(simple_multicore PRIVATE Async pico_stdlib)

pico_add_extra_outputs(simple_multicore)

# Enable USB output, disable UART output.
pico_enable_stdio_usb(simple_multicore 1)
pico_enable_stdio_uart(simple_multicore 0)