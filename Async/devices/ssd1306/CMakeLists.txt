add_library(ssd1306 frameBuffer/FrameBuffer.h textRenderer/8x8_font.h)

target_sources(ssd1306 PRIVATE frameBuffer/FrameBuffer.cpp textRenderer/TextRenderer.cpp SSD1306.cpp)
target_compile_definitions(ssd1306 PUBLIC SSD1306_ASCII_FULL)
target_link_libraries(ssd1306 PUBLIC async_hardware hardware_i2c hardware_dma Async async_hardware
        PRIVATE pico_stdlib)