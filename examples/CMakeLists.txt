add_executable(ssd1306_demo)

target_sources(ssd1306_demo PRIVATE ssd1306_demo.cpp)

target_link_libraries(ssd1306_demo
        PRIVATE Async ssd1306 pico_stdlib )

pico_add_extra_outputs(ssd1306_demo)

# Enable USB output, disable UART output.
pico_enable_stdio_usb(ssd1306_demo 1)
pico_enable_stdio_uart(ssd1306_demo 0)

add_subdirectory(multicore)
add_subdirectory(es_1)