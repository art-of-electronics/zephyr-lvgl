# VIEWE LVGL Demo

LVGL demonstration application for **VIEWE ESP32-S3 RGB display boards** running **Zephyr RTOS 4.4**.

The application demonstrates the integration of a custom ESP32-S3 LCD_CAM RGB display driver, LVGL graphics, and GT911 capacitive touch input. It uses **one application and one display driver**, while CMake selects a display-specific demo entry point for the target board.

## Supported hardware

| Board | PCB revision | Display | Demo entry point | Status |
|---|---|---|---|---|
| VIEWE UEDX80480050E-WB-A | v1.2 | 800 × 480 RGB LCD | `src/main_8048.c` | Working on hardware |
| VIEWE UEDX80480043E-WB-A (480 × 272 variant) | v1.3 | 480 × 272 RGB LCD | `src/main_4872.c` | Working on hardware |

> **Naming note:** The 4.3-inch board carries the `UEDX80480043E-WB-A` marking, but the tested panel is **480 × 272**, not 800 × 480. The 480 × 272 variant may also be referred to as `UEDX48270043E-WB-A`. Use the identifier in **your Zephyr board definition** when building.

Hardware used in the project:

| Component | Description |
|---|---|
| MCU | ESP32-S3-WROOM-1 N16R8 |
| Flash / PSRAM | 16 MB flash / 8 MB PSRAM |
| Displays | Parallel RGB LCD, 800 × 480 or 480 × 272 |
| Pixel format | RGB565X (`PIXEL_FORMAT_RGB_565X`) |
| Touch controller | GT911 (I²C, via Zephyr input subsystem) |
| RGB LED | WS2812 (where present/enabled by the board) |
| Backlight | Software-controlled through the board configuration |

The board definitions provide the display dimensions, RGB timing parameters, LCD_CAM pinctrl, and peripheral configuration. These are **not hardcoded in the application**.

## Demo features

The application supports:

- LVGL screen initialization and custom UI styling.
- Touch interaction through Zephyr input and LVGL.
- Application version information.
- Software-controlled backlight brightness.
- WS2812 RGB LED initialization, if configured for the target board.
- An optional graphics stress test.

The **800 × 480** variant retains the existing larger UI, including its animated Morph Ball and interactive elements. The **480 × 272** variant uses a separate, compact UI entry point so its layout can fit the smaller display.

Both variants use the same Zephyr display driver and common stress-test source. The exact widgets and animations depend on the corresponding `main_*.c` file.

## Project structure

```text
applications/viewe_demo/
├── CMakeLists.txt
├── prj.conf
└── src/
    ├── main_8048.c        # 800 × 480 demo
    ├── main_4872.c        # 480 × 272 demo
    └── test_stress_test.c # Shared stress test
```

CMake selects **one** entry point based on the Zephyr `BOARD` setting; it does not compile both `main_*.c` files into the same application. No board-specific preprocessor switches are required in the display driver.

## Requirements

- Zephyr RTOS **4.4.99** development environment.
- Zephyr SDK with ESP32-S3 support.
- Python virtual environment with Zephyr dependencies installed.
- West command-line tool.
- One of the supported VIEWE boards and its custom Zephyr board definition.

The instructions below assume the project is located at:

```text
~/VSCodeProjects/zephyr-lvgl/
```

Adjust paths if your workspace differs.

## Activate Python environment

Create a virtual environment if you do not already have one:

```bash
python3 -m venv ~/zephyrproject/.venv
```

Activate the environment and enter the Zephyr workspace:

```bash
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
```

Install the required Zephyr Python dependencies in the environment if they have not been installed already.

## Build

The application uses the custom board definitions from the project directory and includes the parent project as an additional Zephyr module via `EXTRA_ZEPHYR_MODULES`.

### 800 × 480 — UEDX80480050E-WB-A v1.2

```bash
west build -p always \
    "$HOME/VSCodeProjects/zephyr-lvgl/applications/viewe_demo" \
    -b uedx80480050e_wb_a@1.2.0/esp32s3/procpu \
    -- -DBOARD_ROOT="$HOME/VSCodeProjects/zephyr-lvgl"
```

Board definition:

```text
boards/viewe/uedx80480050e_wb_a/
```

### 480 × 272 — UEDX80480043E-WB-A v1.3

The exact Zephyr board target must match the name, revision, and qualifier declared by your new board definition. Check the registered targets:

```bash
west boards | grep -i uedx
```

Then build with the **480 × 272 board target** returned by Zephyr:

```bash
BOARD_4872="<your-480x272-board-target>"

west build -p always \
    "$HOME/VSCodeProjects/zephyr-lvgl/applications/viewe_demo" \
    -b "$BOARD_4872" \
    -- -DBOARD_ROOT="$HOME/VSCodeProjects/zephyr-lvgl"
```

Replace `<your-480x272-board-target>` with the actual target; do not assume the PCB marking and Zephyr board identifier are identical. The application's `CMakeLists.txt` must recognize that board and select `src/main_4872.c`.

A pristine build is recommended after switching boards because CMake must select a different source file and Zephyr must regenerate devicetree-derived settings.

## Flash

Connect the target board and run from the corresponding build directory:

```bash
west flash
```

## Display configuration

The custom ESP32-S3 LCD_CAM RGB driver supports three framebuffer modes:

| Framebuffers | Mode | Description |
|---|---|---|
| 1 | Single buffering | Minimum PSRAM use; visible tearing is possible. |
| 2 | Double buffering | Synchronized FRONT/BACK presentation. |
| 3 | Triple buffering | FRONT/READY/DRAW buffers with mailbox presentation. |

Select the framebuffer count at build time in the project's Kconfig configuration:

```ini
CONFIG_ESP32_LCD_RGB_FRAMEBUFFER_COUNT=3
```

Framebuffers reside in external PSRAM. The driver uses two internal-SRAM bounce buffers and a circular GDMA descriptor ring for continuous RGB scanout. With the current shared driver configuration:

| Parameter | 800 × 480 | 480 × 272 |
|---|---:|---:|
| RGB565X framebuffer size | 768,000 B | 261,120 B |
| Bounce lines per buffer | 8 | 8 |
| Bounce buffer size | 12,800 B | 7,680 B |
| GDMA descriptor payload | 1,280 B | 1,280 B |
| Descriptors in ring | 20 | 12 |
| Bounce-buffer EOF events per frame | 60 | 34 |

These values describe the current implementation, not a requirement of the panel itself. Refer to the **LCD_CAM RGB driver README** for DMA, presentation synchronization, and recovery details.

### RGB timings

Panel dimensions and RGB timings are specified by the **board devicetree**, not by `main_8048.c` or `main_4872.c`.

| Setting | 800 × 480 board | 480 × 272 board |
|---|---:|---:|
| Active width | 800 | 480 |
| Active height | 480 | 272 |
| Configured pixel clock | 20 MHz | 16 MHz |

HSYNC/VSYNC timings, porch values, pin assignments, and backlight control belong to each board definition. A working image alone is not a substitute for validating panel timings and scanout stability.

### Display orientation

The driver supports four fixed display orientations selected at build time through Kconfig.

Let **W × H** denote the physical resolution declared in devicetree:

| Rotation | Logical resolution | Configuration |
|---|---|---|
| 0° | W × H | `CONFIG_ESP32_LCD_RGB_ROTATION_0=y` |
| 90° | H × W | `CONFIG_ESP32_LCD_RGB_ROTATION_90=y` |
| 180° | W × H | `CONFIG_ESP32_LCD_RGB_ROTATION_180=y` |
| 270° | H × W | `CONFIG_ESP32_LCD_RGB_ROTATION_270=y` |

For portrait mode:

```ini
CONFIG_ESP32_LCD_RGB_ROTATION_90=y
```

Rotation is applied in software when writing pixels to the framebuffer; the physical RGB scanout geometry remains unchanged. The driver reports the logical resolution and orientation through the Zephyr Display API, and touchscreen coordinate mapping must follow the chosen orientation.

**Notes:**

- Orientation is fixed at build time; runtime rotation is not supported.
- Software rotation may increase framebuffer write overhead, particularly at 90° and 270°.
- Framebuffer count and orientation can be configured independently.
- All four orientations and touch-coordinate mapping were validated on the original **800 × 480** board. Repeat these regression tests on the **480 × 272** variant before claiming the same validation coverage.

## Touchscreen

The tested VIEWE configurations use a **GT911 capacitive touchscreen** over I²C. Input is processed through the Zephyr input subsystem and LVGL.

Touch handling is a board/application integration concern, **not part of the LCD_CAM RGB display driver**. Verify press/release events and coordinates near all four edges, especially after changing panel resolution or enabling rotation.

## Optional stress test

The project includes `src/test_stress_test.c`, a shared graphics stress-test source used to exercise LVGL rendering and framebuffer synchronization with multiple independently updated screen regions.

The existing demo controls stress-test activation with:

```c
#define RUN_STRESS_TEST
```

Enable it in the applicable application source as provided by the project's current compile-time configuration. UI geometry should be checked for the selected display resolution.

## Diagnostics

Enable optional LCD_CAM diagnostics with:

```ini
CONFIG_LOG=y
CONFIG_LOG_LCD_CAM_DIAGNOSTICS=y
```

Diagnostics provide scanout and presentation statistics, including GDMA completion, framebuffer swaps, errors, and recovery events. The driver reports its own counters; they should not be interpreted as LVGL rendering FPS or physical LCD refresh frequency without further measurement.

Disable optional diagnostics for normal operation:

```ini
CONFIG_LOG_LCD_CAM_DIAGNOSTICS=n
```

## Notes and validation

- The **same LCD_CAM RGB driver** operates on both supported boards. Panel-specific resolution, timings, and pinctrl are supplied by devicetree.
- The application has **separate LVGL entry points** to accommodate different screen sizes; its display driver and stress-test module are shared.
- Display scanout runs continuously after initialization. Backlight brightness is controlled independently.
- ESP32-S3 LCD_CAM/GDMA register-level behavior remains hardware-specific; the driver is not a generic RGB implementation for other SoCs.
- Display output has been confirmed on **both** 800 × 480 and 480 × 272 hardware.
- Detailed multi-framebuffer, stress, orientation, and touch regression results from the original 800 × 480 board must not be treated as automatically validated on the 480 × 272 board.
- When modifying the driver or board definitions, retest 1/2/3 framebuffer modes, all four orientations, touch edges, and GDMA error/recovery counters on both displays.

---

**Zephyr RTOS + LVGL + ESP32-S3**

Because apparently, writing a custom display driver sounded like a good idea. 😎
