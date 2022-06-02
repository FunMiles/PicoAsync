add_executable(pio_DS1820)
add_executable(pio_DS1820_DMA)
# by default the header is generated into the build dir
pico_generate_pio_header(pio_DS1820 ${CMAKE_CURRENT_LIST_DIR}/DS1820.pio)
pico_generate_pio_header(pio_DS1820_DMA ${CMAKE_CURRENT_LIST_DIR}/DS1820_DMA.pio)

# however, alternatively you can choose to generate it somewhere else (in this case in the source tree for check in)
#pico_generate_pio_header(pio_blink ${CMAKE_CURRENT_LIST_DIR}/blink.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR})

target_sources(pio_DS1820 PRIVATE DS1820.cpp)
target_sources(pio_DS1820_DMA PRIVATE DS1820_DMA.cpp DMA.cpp DMA.h)

target_link_libraries(pio_DS1820 PRIVATE Async pico_stdlib hardware_pio hardware_dma)
pico_add_extra_outputs(pio_DS1820)
# Enable USB output, disable UART output.
pico_enable_stdio_usb(pio_DS1820 1)
pico_enable_stdio_uart(pio_DS1820 0)

target_link_libraries(pio_DS1820_DMA PRIVATE Async pico_stdlib hardware_pio hardware_dma)
pico_add_extra_outputs(pio_DS1820_DMA)
# Enable USB output, disable UART output.
pico_enable_stdio_usb(pio_DS1820_DMA 1)
pico_enable_stdio_uart(pio_DS1820_DMA 0)