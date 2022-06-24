add_library(ssd1306 frameBuffer/FrameBuffer.h frameBuffer/FrameBuffer.cpp)

target_sources(ssd1306 PRIVATE SSD1306.cpp)

target_link_libraries(ssd1306 PUBLIC async_hardware hardware_i2c hardware_dma PRIVATE pico_stdlib)