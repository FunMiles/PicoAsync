add_library(Async INTERFACE inter_core_queue.h compose.h)

set(ASYNC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(ASYNC_PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
target_include_directories(Async INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_link_libraries(Async INTERFACE neopixel)
# The library require C++20 as it uses coroutines.
#target_compile_features(Async INTERFACE cxx_std_20)
add_subdirectory(hw)
add_subdirectory(devices)
