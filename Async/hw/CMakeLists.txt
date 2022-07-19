add_library(async_hardware)
target_sources(async_hardware PUBLIC DMA.cpp UART.cpp)
#target_include_directories(ASYNC_PARENT_DIR
target_link_libraries(async_hardware PUBLIC Async
        pico_stdlib
        pico_multicore)