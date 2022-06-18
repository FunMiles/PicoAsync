add_library(neopixel)
pico_generate_pio_header(neopixel ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
target_sources(neopixel PRIVATE NeoPixel.cpp)

target_link_libraries(neopixel PUBLIC hardware_pio PRIVATE pico_stdlib hardware_dma)