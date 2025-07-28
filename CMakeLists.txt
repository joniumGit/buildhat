cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(main_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_library(ecc
        micro-ecc/uECC.c
        micro-ecc/uECC.h
)
set_target_properties(ecc PROPERTIES uECC_PLATFORM 0)

add_library(sha2
        sha2/sha2.c
        sha2/sha2.h
)

include_directories(${CMAKE_CURRENT_LIST_DIR}/micro-ecc ${CMAKE_CURRENT_LIST_DIR}/sha2)

add_executable(bhbl
        bhbl-pico/bhbl.c
        bhbl-pico/control.c
        bhbl-pico/ioconv.c
        bhbl-pico/ports.c
        bhbl-pico/timer.c
        bhbl-pico/reboot.s
  )

# no usb onboard
# pico_enable_stdio_usb(bhbl 1)
pico_enable_stdio_uart(bhbl 1)
pico_add_extra_outputs(bhbl)
target_link_libraries(bhbl pico_stdlib hardware_i2c sha2 ecc)
set_target_properties(bhbl PROPERTIES PICO_TARGET_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/bhbl-pico/bhbl.ld)


add_executable(fw
        firmware-pico/main.c
        firmware-pico/control.c
        firmware-pico/command.c
        firmware-pico/debug.c
        firmware-pico/device.c
        firmware-pico/hardware.c
        firmware-pico/ioconv.c
        firmware-pico/message.c
        firmware-pico/ports.c
        firmware-pico/pwm_pid.c
        firmware-pico/timer.c
)

pico_generate_pio_header(fw ${CMAKE_CURRENT_LIST_DIR}/firmware-pico/uart_tx.pio)
pico_generate_pio_header(fw ${CMAKE_CURRENT_LIST_DIR}/firmware-pico/uart_rx.pio)

pico_set_binary_type(fw no_flash)
pico_enable_stdio_uart(fw 1)
pico_add_extra_outputs(fw)

target_link_libraries(fw pico_stdlib hardware_adc hardware_i2c hardware_pio)
set_target_properties(fw PROPERTIES PICO_TARGET_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/firmware-pico/main.ld)

target_compile_options(fw PRIVATE -Wall)
