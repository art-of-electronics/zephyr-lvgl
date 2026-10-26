cmake_minimum_required(VERSION 3.20.0)

set(EXTRA_ZEPHYR_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(viewe_lvgl_demo)

target_include_directories(app PRIVATE src)

if(BOARD STREQUAL "uedx80480050e_wb_a")
    target_sources(app PRIVATE src/main_8048.c)

elseif(BOARD STREQUAL "uedx48270043e_wb_a")
    target_sources(app PRIVATE src/main_4827.c)

else()
    message(FATAL_ERROR "Unsupported board for viewe_lvgl_demo: ${BOARD}")
endif()

target_sources(app PRIVATE
    src/test_stress_test.c
)

set(external_project_cflags
    "${includes} ${definitions} ${options} ${system_includes} -DDISABLE_AUTOMATIC_FW_UPDATE"
)
