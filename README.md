# zephyr-lvgl

**Zephyr RTOS × LVGL**

A collection of custom board definitions, display drivers, and demonstration applications integrating [Zephyr RTOS](https://www.zephyrproject.org/) with [LVGL](https://lvgl.io/).

The project focuses on bringing up embedded graphical interfaces on hardware that requires custom Zephyr integration, while keeping display drivers independent of individual board definitions.

> “Ark, why did you do this?”
>
> — Because Zephyr compiled it. 😎

---

## Features

The current implementation provides:

- Custom Zephyr board support for two VIEWE ESP32-S3 display boards.
- A shared ESP32-S3 LCD_CAM driver operating in parallel RGB mode.
- Continuous RGB display scanout using GDMA and internal-SRAM bounce buffers.
- Framebuffers allocated in external PSRAM.
- Single, double, and triple framebuffer configurations.
- Dirty-region synchronization between framebuffers.
- LVGL integration with GDMA-backed RGB scanout.
- Build-time display rotation (0°, 90°, 180°, and 270°).
- GT911 capacitive touchscreen integration, including orientation-aware touch mapping.
- RGB LED (WS2812) and display backlight control.
- Demonstration application with interactive LVGL elements and animations.

## Supported Hardware

| Board | MCU | Display | Touch | Status |
|---|---|---|---|---|
| VIEWE UEDX80480050E-WB-A v1.2 | ESP32-S3-WROOM-1 N16R8 | 800 × 480 RGB LCD | GT911 | Display and LVGL integration tested |
| VIEWE UEDX80480043E-WB-A v1.3 (480 × 272 variant) | ESP32-S3-WROOM-1 N16R8 | 480 × 272 RGB LCD | GT911 | Display and LVGL integration tested |

**Note:** The 4.3-inch board's PCB identifier contains `80480043E`, even though this hardware variant uses a **480 × 272** panel. Verify the actual panel resolution rather than inferring it from the PCB marking.

Both boards use the **same LCD_CAM RGB display driver**. Their panel dimensions, RGB timings, and pin configuration are provided by their respective board definitions. Validation of every framebuffer mode, rotation, and touch interaction on each board remains a separate regression task.

## Repository Structure

```text
zephyr-lvgl/
├── applications/
│   └── viewe_demo/
│       ├── CMakeLists.txt
│       ├── README.md
│       └── src/
│           ├── main_8048.c
│           ├── main_4827.c
│           └── test_stress_test.c
├── boards/
│   └── viewe/
│       └── ...                    # Board definitions for both VIEWE models
└── drivers/
    └── ...                        # Custom ESP32-S3 LCD_CAM RGB driver
```

- **`applications/`** — LVGL demonstration applications and test workloads.
- **`boards/`** — Custom Zephyr board definitions, grouped by manufacturer.
- **`drivers/`** — Custom peripheral drivers shared across supported boards.

The `viewe_demo` CMake configuration selects **`main_8048.c`** for the 800 × 480 target or **`main_4827.c`** for the 480 × 272 target. The stress-test implementation is shared.

## Software

The project was developed and validated with:

- **Zephyr RTOS 4.5.0** (official stable release, tag `v4.5.0`)
- **Zephyr revision:** `41d10b59730406bbab4ca9f1fbbe9e96aafb4966`
- **hal_espressif revision:** `cc81db00233af6020f2fea9fbb0d0098fb3d0d86` (as reported in the tested workspace)
- **LVGL 9.6**
- Zephyr SDK
- West build and flash tools

### Zephyr compatibility

The project has been built and validated on physical hardware using the official **Zephyr RTOS v4.5.0** release.

The tested Zephyr revision and hal_espressif revision are listed above. Both supported VIEWE display boards have been verified with this release.

To reproduce the tested environment, run the following commands from the root of an existing West workspace:

```bash
git -C zephyr fetch origin
git -C zephyr switch --detach v4.5.0
west update
```

The `west update` command synchronizes Zephyr modules with the revisions specified by the v4.5.0 manifest.

## Getting Started

The `viewe_demo` application supports both VIEWE boards listed above. Select the appropriate Zephyr board target when building; the application chooses the corresponding demo source automatically.

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

The ESP32-S3 RGB display driver uses a continuous **LCD_CAM scanout pipeline** with a circular GDMA descriptor ring and two bounce buffers in internal SRAM. Framebuffers reside in external PSRAM.

Depending on the selected configuration, **one, two, or three framebuffers** are allocated. Double- and triple-buffering support synchronized presentation and dirty-region copying to reduce unnecessary PSRAM traffic.

The driver reads panel resolution and RGB timing parameters from **devicetree**. Its shared DMA configuration supports both 800 × 480 and 480 × 272 panels without board-specific driver source files.

GDMA handles the transfer of pixel data to the LCD_CAM peripheral; **LVGL rendering and framebuffer synchronization are not GPU-accelerated**.

The driver targets the ESP32-S3 LCD_CAM peripheral and is **not a generic display controller driver**. Panel-specific pin assignments, timings, and touch setup belong to board definitions.

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
