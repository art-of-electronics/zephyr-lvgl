cmake_minimum_required(VERSION 3.20.0)

set(EXTRA_ZEPHYR_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(viewe_lvgl_demo)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
    src/main.c
    src/display.c
    src/backlight.c
    src/rgb_led.c
    src/gui.c
)
