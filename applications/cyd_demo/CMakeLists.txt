cmake_minimum_required(VERSION 3.20.0)

set(EXTRA_ZEPHYR_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

# CYD LCD orientation
set(CYD_BOARD_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../boards/cyd/esp32_035"
)

set(CYD_LCD_ROTATION 0)

set(CYD_ROTATION_CONF
    "${CMAKE_CURRENT_SOURCE_DIR}/prj.conf"
)

set_property(
    DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CYD_ROTATION_CONF}"
)

file(
    STRINGS
    "${CYD_ROTATION_CONF}"
    CYD_ROTATION_CONFIG
    REGEX "^CONFIG_CYD_LCD_ROTATION_(0|90|180|270)=y[ \t]*$"
)

list(LENGTH CYD_ROTATION_CONFIG CYD_ROTATION_COUNT)

if(CYD_ROTATION_COUNT GREATER 1)
    message(FATAL_ERROR
        "Multiple CYD LCD orientations selected"
    )
elseif(CYD_ROTATION_COUNT EQUAL 1)
    string(
        REGEX REPLACE
        "^CONFIG_CYD_LCD_ROTATION_([0-9]+)=y[ \t]*$"
        "\\1"
        CYD_LCD_ROTATION
        "${CYD_ROTATION_CONFIG}"
    )
endif()

message(STATUS
    "CYD LCD rotation: ${CYD_LCD_ROTATION} degrees"
)

configure_file(
    "${CYD_BOARD_DIR}/cyd_rotation.overlay.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cyd_rotation.overlay"
    @ONLY
)

list(
    APPEND EXTRA_DTC_OVERLAY_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/cyd_rotation.overlay"
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cyd_demo)

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE 
    src/main.c
    src/demo_rgb.c
    src/demo_lcd.c
    #src/demo_touch.c
)

set(external_project_cflags
    "${includes} ${definitions} ${options} ${system_includes} -DDISABLE_AUTOMATIC_FW_UPDATE"
)
