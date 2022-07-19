set(WIFI_SSID "${WIFI_SSID}" CACHE INTERNAL "WiFi SSID for examples")
set(WIFI_PASSWORD "${WIFI_PASSWORD}" CACHE INTERNAL "WiFi password for examples")
if ("${WIFI_SSID}" STREQUAL "")
    message("Skipping es_1 example as WIFI_SSID is not defined")
elseif ("${WIFI_PASSWORD}" STREQUAL "")
    message("Skipping es_1 example as WIFI_PASSWORD is not defined")
else ()
    add_executable(es_1_demo)
    target_sources(es_1_demo PRIVATE es_1_demo.cpp)
    target_link_libraries(es_1_demo PRIVATE Async pico_stdlib)
    pico_add_extra_outputs(es_1_demo)
    # Enable USB output, disable UART output.
    pico_enable_stdio_usb(es_1_demo 1)
    pico_enable_stdio_uart(es_1_demo 0)

    target_compile_definitions(es_1_demo PRIVATE
            WIFI_SSID=\"${WIFI_SSID}\"
            WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
            )
endif ()