# CYD ESP32-035 - ST7796S LCD

Board-specific LCD initialization and display orientation support for the **CYD ESP32-035** running **Zephyr RTOS 4.5.0**.

The display uses Zephyr's upstream `sitronix,st7796s` driver and the MIPI-DBI SPI interface. The local `cyd_st7796s.c` module provides the panel-specific initialization sequence; it is **not** a replacement for the upstream display driver.

## Hardware and integration

| Component | Configuration |
| --- | --- |
| Display controller | ST7796S |
| Native resolution | 320 × 480 |
| Interface | 4-wire SPI via Zephyr MIPI-DBI |
| SPI clock | 32 MHz |
| Pixel format | RGB565 |
| Touch controller | XPT2046, shared SPI bus |
| Touch SPI clock | 2 MHz |
| Backlight | LEDC PWM, exposed as `backlight` alias |

The board Devicetree defines the `lcd` and `touchscreen` nodes and selects them through `zephyr,display` and `zephyr,touch`. Panel timing, power-control and gamma parameters are defined in Devicetree and used by the board-specific initialization module.

## LCD initialization

`cyd_st7796s_configure()` sends the panel-specific command sequence over MIPI-DBI, including command-set unlock/lock, frame-rate and porch settings, power and VCOM configuration, gamma curves, color inversion, pixel format and memory access control (`MADCTL`).

**Keep `MADCTL` synchronized with Devicetree.** The rotation overlay updates the `madctl` property, so the initialization module must obtain its value from the LCD node instead of using a fixed constant:

```c
#define LCD_NODE DT_CHOSEN(zephyr_display)

static const uint8_t lcd_madctl = (uint8_t)DT_PROP(LCD_NODE, madctl);
```

This avoids overwriting the selected orientation during panel initialization.

## Build-time rotation

The board supports four fixed display orientations. Select one in the application's `prj.conf`:

```ini
CONFIG_CYD_LCD_ROTATION_0=y
# CONFIG_CYD_LCD_ROTATION_90=y
# CONFIG_CYD_LCD_ROTATION_180=y
# CONFIG_CYD_LCD_ROTATION_270=y
```

| Rotation | Resolution | ST7796S `MADCTL` |
| --- | --- | --- |
| 0° | 320 × 480 | `0x48` |
| 90° | 480 × 320 | `0x28` |
| 180° | 320 × 480 | `0x88` |
| 270° | 480 × 320 | `0xE8` |

The orientation choice is defined in the board's `Kconfig`. A CMake step reads the selection from `prj.conf`, generates the rotation overlay from `lcd_rotation.overlay.in`, and registers the generated file with `EXTRA_DTC_OVERLAY_FILE` **before** `find_package(Zephyr ...)`.

The overlay updates both devices together:

- **ST7796S:** `width`, `height` and `madctl`.
- **XPT2046:** `swapped-x-y`, `inverted-x` and `inverted-y` as required by the selected orientation.

Touch calibration remains in the native portrait coordinate system (`320 × 480`), with the same ADC calibration values for every rotation. The upstream Zephyr input touchscreen layer applies the coordinate transformation; no separate LVGL touch-rotation code is required.

> **Note:** The overlay is generated at CMake configure time, not selected directly by the Devicetree preprocessor. The current integration reads `prj.conf`; applications using `CONF_FILE` or `EXTRA_CONF_FILE` must ensure that the CMake selection logic reads the effective configuration source.

## Integration in an application

An application using this board should:

1. Enable the required Zephyr display, MIPI-DBI, input and LVGL features in its configuration.
2. Select a single `CONFIG_CYD_LCD_ROTATION_*` option.
3. Include the board's rotation CMake integration before `find_package(Zephyr ...)` so the generated overlay is applied.
4. Call `cyd_st7796s_configure()` at the appropriate point in panel initialization, before rendering.
5. Obtain the active resolution from `display_get_capabilities()` instead of assuming a fixed 320 × 480 layout.

The `cyd_demo` application demonstrates LCD initialization, LVGL rendering, touchscreen input and PWM backlight control.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Image is rotated but touch is not | Confirm the generated overlay updates `&touchscreen` and that the final `zephyr.dts` contains the expected inversion/swap properties. |
| Display returns to portrait after initialization | Look for a hard-coded `MADCTL` value in custom initialization code. |
| Incorrect colors | Verify the board's `rgb-is-inverted` property and the pixel-format configuration. |
| LVGL uses 320 × 480 in landscape mode | Check the final LCD `width`/`height` properties and avoid hard-coded application dimensions. |
| Configuration changes have no effect | Perform a pristine build and inspect the generated overlay and `build/zephyr/zephyr.dts`. |

All four orientations have been exercised on the CYD ESP32-035 with the LCD and touchscreen aligned.
