cmake_minimum_required(VERSION 3.16)

set(PICO_BOARD pico_w)
set(PICO_TARGET_LINKER_SCRIPT memmap_ota.ld)

# Pull in SDK (must be before project)
include(cmake/pico_sdk_import.cmake)

project(nabutftp)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPICO_USE_MALLOC_MUTEX=1")

add_executable(nabutftp)

pico_generate_pio_header(nabutftp
    ${CMAKE_CURRENT_SOURCE_DIR}/duart_tx.pio)

pico_generate_pio_header(nabutftp
    ${CMAKE_CURRENT_SOURCE_DIR}/duart_rx.pio)

pico_generate_pio_header(nabutftp
    ${CMAKE_CURRENT_SOURCE_DIR}/suart_tx.pio)

pico_generate_pio_header(nabutftp
    ${CMAKE_CURRENT_SOURCE_DIR}/suart_rx.pio)

target_compile_definitions(nabutftp PRIVATE
            WIFI_SSID=\"${WIFI_SSID}\"
            WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
            TFTP_SERVER_IP=\"${TFTP_SERVER_IP}\"
            )

target_sources(nabutftp PUBLIC
    ota.o
    main.c
    config.c
    duart.c
    packet.c
    wifi.c
    tftp.c
)

target_include_directories(nabutftp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(lib/littlefs)

target_link_libraries(nabutftp PUBLIC
    pico_multicore
    pico_stdlib
    littlefs-lib
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_dma
    hardware_pio
)

# enable usb output, disable uart output
pico_enable_stdio_usb(nabutftp 1)
pico_enable_stdio_uart(nabutftp 0)

pico_add_extra_outputs(nabutftp)
