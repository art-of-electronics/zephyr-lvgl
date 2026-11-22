cmake_minimum_required(VERSION 3.20.0)

zephyr_library()

zephyr_library_sources(
    display_esp32_lcd_cam_rgb_v2.c
    rgb_v2_scanout.c
    rgb_v2_bounce_state.c
    rgb_v2_frame_manager.c
    rgb_v2_dirty.c
    rgb_v2_rotation.c
)
