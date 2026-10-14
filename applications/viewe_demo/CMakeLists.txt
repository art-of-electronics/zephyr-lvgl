cmake_minimum_required(VERSION 3.20.0)

set(EXTRA_ZEPHYR_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(viewe_lvgl_demo)

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
    src/main.c
    src/test_samus_orange.c
    src/test_dynamic_unlimited.c
)

set(external_project_cflags
    "${includes} ${definitions} ${options} ${system_includes} -DDISABLE_AUTOMATIC_FW_UPDATE"
)
