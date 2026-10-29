# VIEWE LVGL Demo

LVGL demonstration application for **VIEWE ESP32-S3 RGB display boards**, using the custom Zephyr LCD_CAM RGB display driver and GT911 touchscreen input.

The application supports two display sizes. CMake selects the appropriate demo entry point for the Zephyr board target; both variants share the same display driver and stress-test module.

## Supported hardware

| Board / PCB marking | PCB revision | Display | Demo entry point | Validation |
|---|---|---|---|---|
| VIEWE UEDX80480050E-WB-A | v1.2 | 800 × 480 RGB LCD | `src/main_8048.c` | Display, touch, buffering, and all four orientations tested on hardware |
| VIEWE UEDX80480043E-WB-A | v1.3 | 480 × 272 RGB LCD | `src/main_4872.c` | Display output confirmed on hardware |

> **Board naming:** The 4.3-inch PCB is marked `UEDX80480043E-WB-A`, although its panel resolution is **480 × 272**. The corresponding Zephyr board definition may use `uedx48270043e_wb_a`. Always use the target reported by `west boards`.

Both boards use the ESP32-S3-WROOM-1 N16R8. Display timing, pin assignments, touch, backlight, and optional WS2812 configuration belong to the board definitions rather than the application.

## Demo features

- Interactive LVGL UI with GT911 touch input.
- Animated Morph Ball and responsive layout on the 800 × 480 variant.
- Compact UI on the 480 × 272 variant.
- Application version information and backlight control.
- Optional WS2812 initialization, where enabled by the board.
- Optional graphics stress test and LCD_CAM diagnostics.

## Application structure

```text
applications/viewe_demo/
├── CMakeLists.txt
├── prj.conf
└── src/
    ├── main_8048.c         # 800 × 480 demo
    ├── main_4872.c         # 480 × 272 demo
    └── test_stress_test.c  # Shared stress test
```

CMake compiles only the entry point associated with the selected board, not both `main_*.c` files.

## Prerequisites

Set up the Zephyr SDK, West, and Python environment following the official Zephyr instructions. Use the official Zephyr `v4.5.0` release; see [Zephyr compatibility](../../README.md#zephyr-compatibility) in the project README for the commit and module synchronization commands.

The examples below assume:

- Zephyr workspace: `~/zephyrproject`
- Project repository: `~/VSCodeProjects/zephyr-lvgl`

Activate your existing environment and enter the workspace:

```bash
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
```

Adjust the paths if your checkout is located elsewhere.

## Build and flash

### 800 × 480 — UEDX80480050E-WB-A v1.2

```bash
west build -p always \
"$HOME/VSCodeProjects/zephyr-lvgl/applications/viewe_demo" \
-b uedx80480050e_wb_a@1.2.0/esp32s3/procpu \
-- -DBOARD_ROOT="$HOME/VSCodeProjects/zephyr-lvgl"
```

### 480 × 272 — UEDX80480043E-WB-A v1.3

Check the board identifier available in your checkout:

```bash
west boards | grep -i uedx
```

Use the registered **480 × 272** target in the build command:

```bash
BOARD_4827="<your-480x272-board-target>"

west build -p always \
"$HOME/VSCodeProjects/zephyr-lvgl/applications/viewe_demo" \
-b uedx48270043e_wb_a@1.3.0/esp32s3/procpu \
-- -DBOARD_ROOT="$HOME/VSCodeProjects/zephyr-lvgl"
```

A pristine build (`-p always`) is recommended when switching boards so CMake selects the correct demo source and Zephyr regenerates the devicetree configuration.

Flash the board from the corresponding build directory:

```bash
west flash
```

## Demo configuration

The following settings are selected at **build time**, typically in `prj.conf`.

| Purpose | Configuration / location |
|---|---|
| Framebuffer count (1–3) | `CONFIG_ESP32_LCD_RGB_FRAMEBUFFER_COUNT=3` |
| Orientation (0° / 90° / 180° / 270°) | `CONFIG_ESP32_LCD_RGB_ROTATION_90=y` (portrait example) |
| Optional LCD_CAM diagnostics | `CONFIG_LOG=y` and `CONFIG_LOG_LCD_CAM_DIAGNOSTICS=y` |
| Disable optional diagnostics | `CONFIG_LOG_LCD_CAM_DIAGNOSTICS=n` |
| Graphics stress test | `RUN_STRESS_TEST` in the applicable `main_*.c` source |

Display orientation is fixed at build time; runtime rotation is not supported. Framebuffer count and orientation can be configured independently. Refer to the [project README](../../README.md#display-configuration) for configuration details and to the LCD_CAM RGB driver documentation for DMA, synchronization, and recovery behavior.

The stress test (`src/test_stress_test.c`) exercises multiple independently updated LVGL regions. Check its layout when changing the display size or orientation.

## Validation notes

- **800 × 480:** LVGL rendering, interactive touch, all four orientations, and multi-framebuffer behavior have been tested on physical hardware.
- **480 × 272:** Display output has been confirmed on physical hardware. Do not assume that the complete orientation, touch, stress, and buffering regression coverage of the 800 × 480 board also applies to this variant.
- After modifying board or display configuration, verify the UI layout, touch coordinates at screen edges, framebuffer behavior, and the absence of unexpected GDMA errors or recovery events.

---

**Zephyr RTOS + LVGL + ESP32-S3**

Because apparently, writing a custom display driver sounded like a good idea. 😎
