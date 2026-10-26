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
- Synchronized framebuffer presentation and dirty-region synchronization for multi-buffer modes.
- Board-specific display resolutions and RGB timings configured through devicetree.
- LVGL integration through the Zephyr display subsystem.
- GT911 capacitive touchscreen integration through the Zephyr input subsystem.
- WS2812 RGB LED and display backlight control in the demonstration application.
- Resolution-specific LVGL demos selected at build time.
- Optional graphics stress test and LCD_CAM diagnostics.

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

The project is developed using:

- Zephyr RTOS 4.4
- LVGL 9.x
- Zephyr SDK with ESP32-S3 support
- West build and flash tools

## Getting Started

The `viewe_demo` application supports both VIEWE boards listed above. Select the appropriate Zephyr board target when building; the application chooses the corresponding demo source automatically.

See [VIEWE LVGL Demo](applications/viewe_demo/README.md) for build, flash, display-orientation, and application configuration instructions.

## Implementation Notes

The ESP32-S3 RGB display driver uses a continuous **LCD_CAM scanout pipeline** with a circular GDMA descriptor ring and two bounce buffers in internal SRAM. Framebuffers reside in external PSRAM.

Depending on the selected configuration, **one, two, or three framebuffers** are allocated. Double- and triple-buffering support synchronized presentation and dirty-region copying to reduce unnecessary PSRAM traffic.

The driver reads panel resolution and RGB timing parameters from **devicetree**. Its shared DMA configuration supports both 800 × 480 and 480 × 272 panels without board-specific driver source files.

GDMA handles the transfer of pixel data to the LCD_CAM peripheral; **LVGL rendering and framebuffer synchronization are not GPU-accelerated**.

The driver targets the ESP32-S3 LCD_CAM peripheral and is **not a generic display controller driver**. Panel-specific pin assignments, timings, and touch setup belong to board definitions.

## Project Status

**Working prototype — validated on two physical display boards.**

Current milestones:

- LCD_CAM RGB display initialization and continuous scanout on both resolutions.
- Shared GDMA and bounce-buffer configuration.
- LVGL rendering using board-specific demos.
- GT911 touchscreen integration.
- Interactive GUI elements and animations.
- Framebuffer synchronization and diagnostic support.

The 800 × 480 board has undergone more extensive development testing, including display rotations and multi-buffer stress workloads. The 480 × 272 board is operational, but equivalent full regression testing is still pending.

Further development may include additional boards, panel configurations, and demonstration applications.

---

**Built with Zephyr, LVGL, and a suspicious amount of debugging.**
