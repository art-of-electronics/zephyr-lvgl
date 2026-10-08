cmake_minimum_required(VERSION 3.20.0)

zephyr_library()

zephyr_library_sources_ifdef(
    CONFIG_ESP32_LCD_CAM_RGB
    display_esp32_lcd_cam_rgb.c
)
