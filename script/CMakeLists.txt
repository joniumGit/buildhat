cmake_minimum_required(VERSION 3.13)

project(scripts C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_library(ecc
        ../micro-ecc/uECC.c
        ../micro-ecc/uECC.h
)
set_target_properties(ecc PROPERTIES uECC_PLATFORM 0)

add_library(sha2
        ../sha2/sha2.c
        ../sha2/sha2.h
)

include_directories(${CMAKE_CURRENT_LIST_DIR}/../micro-ecc ${CMAKE_CURRENT_LIST_DIR}/../sha2)

add_executable(sign
        sign.c
)
target_link_libraries(sign sha2 ecc)

add_executable(upload
        upload.c
)

add_executable(mkkeys
        mkkeys.c
)
target_link_libraries(mkkeys ecc)