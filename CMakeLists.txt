cmake_minimum_required(VERSION 3.22)

include ($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(AsyncPico C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# Creates a pico-sdk folder.
pico_sdk_init()

add_subdirectory(Async)
add_subdirectory(examples)

add_executable(AsyncPico main.cpp)

pico_add_extra_outputs(AsyncPico)

target_link_libraries(AsyncPico Async
        pico_stdlib)

# Enable USB output, disable UART output.
pico_enable_stdio_usb(AsyncPico 1)
pico_enable_stdio_uart(AsyncPico 0)

add_subdirectory(pio_examples)