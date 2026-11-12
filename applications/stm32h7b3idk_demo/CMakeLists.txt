cmake_minimum_required(VERSION 3.20.0)

set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/backlight.overlay")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(stm32h7b3i_demo)

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE 
    src/main.c
    assets/morph_ball.c
)
