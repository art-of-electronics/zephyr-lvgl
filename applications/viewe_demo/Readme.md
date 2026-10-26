# VIEWE LVGL Demo

LVGL demonstration application for the **VIEWE UEDX80480050E-WB-A v1.2** development board running Zephyr RTOS.

The application demonstrates the integration of a custom ESP32-S3 RGB display driver, LVGL graphics, and GT911 capacitive touch input.

## Hardware

| Component | Description |
|---|---|
| Board | VIEWE UEDX80480050E-WB-A v1.2 |
| MCU | ESP32-S3-WROOM-1 N16R8 |
| Flash | 16 MB |
| PSRAM | 8 MB |
| Display | 800 × 480 RGB LCD |
| Pixel format | RGB565X |
| Touch controller | GT911 |
| RGB LED | WS2812 |
| Backlight | Software-controlled |

## Demo Features

The application includes:

- LVGL screen initialization.
- Custom colors and UI styling.
- Animated Morph Ball.
- Interactive button with touch event handling.
- Application version information.
- WS2812 RGB LED initialization.
- Display backlight brightness control.
- Optional graphics stress test.

The demonstration uses the Zephyr display and input subsystems together with LVGL.

## Requirements

- Zephyr RTOS 4.4 development environment.
- Zephyr SDK with ESP32-S3 support.
- Python virtual environment with Zephyr dependencies.
- West command-line tool.
- VIEWE UEDX80480050E-WB-A v1.2 board.

The instructions below assume the following project location:

```text
~/VSCodeProjects/zephyr-lvgl/
```

Adjust the paths if your workspace differs.

## Activate Python Environment

If the Zephyr virtual environment has not been created yet:

```bash
python3 -m venv ~/zephyrproject/.venv
```

Activate the environment:

```bash
source ~/zephyrproject/.venv/bin/activate

cd ~/zephyrproject
```

The environment must have the required Zephyr Python dependencies installed.

## Build

Target: **UEDX80480050E-WB-A v1.2 / ESP32-S3 PROCPU**

```bash
west build -p always \
$HOME/VSCodeProjects/zephyr-lvgl/applications/viewe_demo \
-b uedx80480050e_wb_a@1.2.0/esp32s3/procpu \
-- -DBOARD_ROOT=$HOME/VSCodeProjects/zephyr-lvgl
```

The custom board definition is located in:

```text
boards/viewe/uedx80480050e_wb_a/
```

## Flash

Connect the board to the development machine and run:

```bash
west flash
```

## Display Configuration

The custom ESP32-S3 LCD_CAM RGB driver supports three framebuffer configurations:

| Framebuffers | Mode | Description |
|---|---|---|
| 1 | Single buffering | Minimum memory usage; tearing possible |
| 2 | Double buffering | Synchronized front/back presentation |
| 3 | Triple buffering | Front/ready/draw buffers with mailbox presentation |

The number of framebuffers is selected at build time:

```ini
CONFIG_ESP32_LCD_RGB_FRAMEBUFFER_COUNT=3
```

Framebuffers are stored in external PSRAM. The display driver uses internal-SRAM bounce buffers and GDMA for continuous RGB scanout.

### Display Orientation

The driver supports four display orientations, selected at build time through Kconfig.

| Rotation | Logical Resolution | Configuration |
|---|---|---|
| 0° | 800 × 480 | `CONFIG_ESP32_LCD_RGB_ROTATION_0=y` |
| 90° | 480 × 800 | `CONFIG_ESP32_LCD_RGB_ROTATION_90=y` |
| 180° | 800 × 480 | `CONFIG_ESP32_LCD_RGB_ROTATION_180=y` |
| 270° | 480 × 800 | `CONFIG_ESP32_LCD_RGB_ROTATION_270=y` |

For example, to enable portrait mode:

```ini
CONFIG_ESP32_LCD_RGB_ROTATION_90=y
```

Rotation is applied in software when writing pixels to the framebuffer. The physical RGB scanout configuration remains unchanged.

The driver reports the logical display resolution and orientation through the Zephyr Display API, allowing LVGL to adapt its layout accordingly.

The GT911 touchscreen coordinates are automatically transformed to match the selected display orientation.

**Notes:**
- Display orientation is fixed at build time; runtime rotation is not supported.
- All four orientations have been validated on the target hardware.
- Software rotation may increase framebuffer write overhead, particularly in portrait mode (90° and 270°).
- Framebuffer count and display orientation can be configured independently.

## Touchscreen

The board uses a **GT911 capacitive touchscreen controller** connected through I²C.

Touch input is integrated with the Zephyr input subsystem and LVGL.

The demo includes an interactive button to verify touch event handling and LVGL object states.

## Optional Stress Test

The application contains an optional graphics stress test controlled by:

```c
#define RUN_STRESS_TEST
```

Enable the stress test according to the application's existing compile-time configuration.

The test exercises LVGL rendering and framebuffer synchronization using multiple independently updated screen regions.

## Diagnostics

Optional LCD_CAM diagnostics can be enabled with:

```ini
CONFIG_LOG=y
CONFIG_LOG_LCD_CAM_DIAGNOSTICS=y
```

The diagnostics report scanout activity, framebuffer presentations, DMA errors, and recovery events.

Disable the optional diagnostics for normal operation:

```ini
CONFIG_LOG_LCD_CAM_DIAGNOSTICS=n
```

## Notes

- The application is intended for the VIEWE UEDX80480050E-WB-A v1.2 board.
- The RGB display driver is hardware-specific.
- The display scanout runs continuously after initialization.
- Backlight brightness is controlled independently of the display scanout.
- Touchscreen operation depends on the GT911 configuration provided by the board definition.

---

**Zephyr RTOS + LVGL + ESP32-S3**

Because apparently, writing a custom display driver sounded like a good idea. 😎
