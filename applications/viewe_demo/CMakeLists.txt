cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(viewe_lvgl_demo)

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
    src/main.c
    src/lcd_rgb_poc.c
)

set(external_project_cflags
    "${includes} ${definitions} ${options} ${system_includes} -DDISABLE_AUTOMATIC_FW_UPDATE"
)
