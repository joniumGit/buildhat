cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(main_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(main
  bhbl.c
  control.c
  ioconv.c
  ports.c
  timer.c
  reboot.s
  ../micro-ecc/uECC.o
  ../sha2/sha2.o
  )

# pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)
pico_add_extra_outputs(main)
target_link_libraries(main pico_stdlib hardware_i2c)
set_target_properties(main PROPERTIES PICO_TARGET_LINKER_SCRIPT ../bhbl.ld)
