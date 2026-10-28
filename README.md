# zephyr-lvgl

**Zephyr RTOS × LVGL**

A collection of custom board definitions, display drivers, and demonstration applications integrating [Zephyr RTOS](https://www.zephyrproject.org/) with [LVGL](https://lvgl.io/).

The project focuses on bringing up embedded graphical interfaces on hardware that requires custom Zephyr integration.

> “Ark, why did you do this?”
>
> — Because Zephyr compiled it. 😎

---

## Features

The current implementation provides:

- Custom Zephyr board support for the **VIEWE UEDX80480050E-WB-A**.
- ESP32-S3 LCD_CAM driver operating in parallel RGB mode.
- Continuous display scanout using GDMA.
- Framebuffers allocated in external PSRAM.
- Single, double, and triple framebuffer configurations.
- Dirty-region synchronization between framebuffers.
- LVGL integration with GDMA-backed RGB scanout.
- Build-time display rotation (0°, 90°, 180°, and 270°).
- GT911 capacitive touchscreen integration, including orientation-aware touch mapping.
- RGB LED (WS2812) and display backlight control.
- Demonstration application with interactive LVGL elements and animations.

## Supported Hardware

| Board | MCU | Display | Touch |
|---|---|---|---|
| VIEWE UEDX80480050E-WB-A v1.2 | ESP32-S3-WROOM-1 N16R8 | 800 × 480 RGB LCD | GT911 |

Additional boards may be added in the future.

## Repository Structure

```text
zephyr-lvgl/
├── applications/
│   └── viewe_demo/
├── boards/
│   └── viewe/
│       └── uedx80480050e_wb_a/
└── drivers/
```

- **applications/** — LVGL demonstration applications.
- **boards/** — Custom Zephyr board definitions, grouped by manufacturer.
- **drivers/** — Custom peripheral drivers.

## Software

The project was developed and validated with:

- **Zephyr RTOS 4.4.99** (development snapshot from `main`; **not** the stable `v4.4.0` release)
- **Zephyr revision:** `434233e9751e7cca7bea82047e812bca607722d3`
- **hal_espressif revision:** `5e3b702250c3e05de5a4cb3a53f74fc3cd8094cf` (as reported in the tested workspace)
- **LVGL 9.6**
- Zephyr SDK
- West build and flash tools

### Zephyr compatibility

The known-working baseline is the exact Zephyr revision listed above. A standard installation following Zephyr's *Getting Started* guide may check out a different revision of `main` or a stable release.

**The official Zephyr `v4.4.0` tag is not currently a supported build target.** Its ESP32-S3 pinctrl header does not define `RMT_OUT0_GPIO0`, which is referenced by the board's WS2812/RMT pin configuration; parsing the board's devicetree therefore fails. Compatibility with other Zephyr revisions has not been established.

To reproduce the tested Zephyr revision in an existing West workspace, run from the workspace root (the directory containing `zephyr/`):

```bash
git -C zephyr fetch origin
git -C zephyr switch --detach 434233e9751e7cca7bea82047e812bca607722d3
west update
```

The detached HEAD is intentional: it pins Zephyr to the validated commit rather than a moving `main` branch. `west update` then synchronizes modules with that revision's manifest. Use a clean build after switching Zephyr revisions.

## Getting Started

The current demonstration targets the VIEWE UEDX80480050E-WB-A v1.2 board.

First, use the Zephyr revision specified in [Zephyr compatibility](#zephyr-compatibility). Then see [Viewe Demo](applications/viewe_demo/Readme.md) for build instructions and application details.

## Display Configuration

The ESP32-S3 LCD_CAM RGB driver supports three build-time framebuffer configurations:

| Framebuffers | Mode | Description |
|---|---|---|
| 1 | Single buffering | Minimum memory usage; tearing possible |
| 2 | Double buffering | Synchronized front/back presentation |
| 3 | Triple buffering | Front/ready/draw buffers with mailbox presentation |

Select the framebuffer count with:

```ini
CONFIG_ESP32_LCD_RGB_FRAMEBUFFER_COUNT=3
```

Framebuffers reside in external PSRAM. The driver uses internal-SRAM bounce buffers and GDMA for continuous RGB scanout.

### Display orientation

The driver supports four orientations selected at build time:

| Rotation | Logical resolution | Configuration |
|---|---|---|
| 0° | 800 × 480 | `CONFIG_ESP32_LCD_RGB_ROTATION_0=y` |
| 90° | 480 × 800 | `CONFIG_ESP32_LCD_RGB_ROTATION_90=y` |
| 180° | 800 × 480 | `CONFIG_ESP32_LCD_RGB_ROTATION_180=y` |
| 270° | 480 × 800 | `CONFIG_ESP32_LCD_RGB_ROTATION_270=y` |

For example, to select portrait orientation:

```ini
CONFIG_ESP32_LCD_RGB_ROTATION_90=y
```

Rotation is performed in software while writing pixels to the framebuffer. Physical LCD_CAM/GDMA scanout remains at 800 × 480. The Zephyr Display API reports the selected logical resolution and orientation, allowing LVGL and GT911 touch input to operate in the correct coordinate system.

Runtime orientation changes are not supported. Framebuffer count and orientation can be configured independently. All four orientations have been validated on the target board.

## Implementation Notes

The ESP32-S3 RGB display driver uses a continuous LCD_CAM scanout pipeline with GDMA and internal-SRAM bounce buffers.

Depending on the selected configuration, one, two, or three framebuffers are allocated in external PSRAM.

Double- and triple-buffering support synchronized presentation and dirty-region copying to reduce unnecessary memory traffic.

The implementation is currently hardware-specific and has been validated on the supported VIEWE board.

## Project Status

**v1.0.0 — initial stable release; hardware validated on the supported board.**

Completed milestones include:

- LCD_CAM RGB display initialization and continuous GDMA scanout.
- Single, double, and triple buffering with dirty-region synchronization.
- LVGL rendering, interactive elements, and animations.
- GT911 touchscreen input.
- Display rotation in all four orientations with corresponding touch mapping.
- Project and driver documentation.

Further development may include additional boards, display controllers, and demonstration applications. Portability to other Zephyr revisions and hardware targets requires separate validation.

---

**Built with Zephyr, LVGL, and a suspicious amount of debugging.**
