# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(EXTRA_DTC_OVERLAY_FILE "dts/dts.overlay")
set(TOOLCHAIN_HAS_PICOLIBC ON CACHE BOOL "Force toolchain picolibc support" FORCE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(smartwater)

target_include_directories(app PRIVATE submodules/esp_generic_lib/include)

set(SRCS 
    src/main.cpp
    src/usb_uart.cpp
    src/soil_sensor.cpp
    src/soil_task.cpp
)

target_sources(app PRIVATE ${SRCS})

add_subdirectory(submodules/nrf_general)
add_subdirectory(submodules/nrf_uart)
add_subdirectory(submodules/nrf_zbm_cpp)

target_include_directories(NrfLibUART PRIVATE submodules/esp_generic_lib/include)

target_link_libraries(app PUBLIC NrfLibGeneral)
target_link_libraries(app PUBLIC NrfZBMCpp)
target_include_directories(NrfZBMCpp PRIVATE $<TARGET_PROPERTY:app,INCLUDE_DIRECTORIES>)

target_link_libraries(app PUBLIC NrfLibUART)

target_compile_options(app PRIVATE -fno-omit-frame-pointer)

target_link_libraries(app PRIVATE c stdc++ supc++)
# target_compile_options(app PRIVATE -fmacro-backtrace-limit=0)
target_compile_options(app PRIVATE -D__cpp_concepts=202002L -Wno-builtin-macro-redefined -Wno-invalid-offsetof -Wno-unused-variable -Wno-double-promotion)
target_compile_options(app PRIVATE -freflection)

