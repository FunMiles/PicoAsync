add_library(Async INTERFACE)

target_include_directories(Async INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# The library require C++20 as it uses coroutines.
#target_compile_features(Async INTERFACE cxx_std_20)
