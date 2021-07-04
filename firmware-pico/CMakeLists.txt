cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(main_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(main
  main.c
  control.c
  command.c
  debug.c
  device.c
  hardware.c
  ioconv.c
  message.c
  ports.c
  pwm_pid.c
  timer.c
  )

pico_generate_pio_header(main ${CMAKE_CURRENT_LIST_DIR}/uart_tx.pio)
pico_generate_pio_header(main ${CMAKE_CURRENT_LIST_DIR}/uart_rx.pio)

pico_set_binary_type(main no_flash)
# pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)
pico_add_extra_outputs(main)
target_link_libraries(main pico_stdlib hardware_adc hardware_i2c hardware_pio)
set_target_properties(main PROPERTIES PICO_TARGET_LINKER_SCRIPT ../main.ld)

target_compile_options(main PRIVATE -Wall)
