# STM32H7B3I-DK — Morph Ball Demo

**Zephyr RTOS × LVGL × STM32H7**

A demonstration of **Zephyr RTOS 4.5.0** and **LVGL 9.6.0** running on the **STM32H7B3I-DK** development board.

The application features an animated Morph Ball inspired by the *Metroid* series, demonstrating image transformations, touchscreen interaction, hardware display acceleration, and runtime performance monitoring.

> "Why a Morph Ball?"
>
> — Because blinking LEDs is getting boring. 😎

---

## Hardware

| Component | Description |
|---|---|
| Development board | STM32H7B3I-DK |
| Microcontroller | STM32H7B3LIH6Q |
| CPU | Arm Cortex-M7 |
| Display | 4.3" TFT LCD |
| Resolution | 480 × 272 |
| Pixel format | RGB565 |
| Display controller | STM32 LTDC |
| Graphics accelerator | STM32 DMA2D |
| Touch controller | FT5336 |
| Framebuffer memory | External SDRAM |

The application uses the native Zephyr board definition:

`stm32h7b3i_dk`

No STM32CubeIDE, TouchGFX, or external BSP is required.

---

## Features

- Animated 64 × 64 Morph Ball sprite with transparency.
- Dynamic green energy glow beneath the Morph Ball.
- Yellow impact flash using LVGL image recoloring.
- Smooth acceleration and deceleration with position-synchronized rotation.
- Continuous horizontal movement with automatic direction reversal at screen boundaries.
- Position-based rotation simulating rolling without slipping.
- Touch interaction for reversing movement direction.
- LVGL 9.6 image transformations.
- Partial display refresh.
- Double framebuffer configuration.
- STM32 LTDC display output.
- DMA2D graphics acceleration enabled.
- Runtime FPS and animation update monitoring.
- LVGL handler execution time measurements.
- Dynamic display of Zephyr and LVGL versions.

The application uses a fixed-point position representation to maintain smooth movement without floating-point calculations.

### Graphics Configuration

| Parameter | Configuration |
|---|---|
| Color format | RGB565 |
| Rendering mode | Partial refresh |
| Framebuffers | 2 |
| LVGL refresh period | 16 ms |
| Animation timer | 16 ms |
| LVGL memory pool | 16 KiB |
| Main thread stack | 8 KiB |

The display uses external SDRAM for framebuffer storage.

DMA2D is enabled through Zephyr's LVGL integration.

---

## Performance Monitoring

The application reports performance statistics once per second using Zephyr's logging subsystem.

Example output:

```text
*** Booting Zephyr OS build v4.5.0 ***
<inf> morph_ball: Morph Ball demo started
<inf> morph_ball: FPS: 46 | APS: 46 | LVGL avg: 11670 us | LVGL max: 19005 us
```

The reported metrics are:

| Metric | Description |
|---|---|
| FPS | Completed LVGL refresh cycles per second |
| APS | Animation updates per second |
| LVGL avg | Average execution time of `lv_timer_handler()` |
| LVGL max | Maximum execution time of `lv_timer_handler()` |

The timing measurements use Zephyr's cycle counter and are reported in microseconds.

**Note:** FPS measures LVGL refresh cycles, not the physical LCD scanout frequency. Handler execution time includes LVGL processing and possible waiting; it is not a direct measurement of CPU utilization.

### Reference Results

The following results were observed on STM32H7B3I-DK:

| Metric | Result |
|---|---|
| FPS | ~45–46 |
| APS | ~45–46 |
| Average LVGL handler time | ~11.7 ms |
| Maximum LVGL handler time | ~19–31 ms |

Measurements were collected with partial refresh, double buffering, and DMA2D enabled.

Actual performance may vary depending on rendering configuration, application workload, and compiler optimization.

---

## Build

### Environment

Create and activate the Python virtual environment:

```bash
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
```

The environment must contain an initialized Zephyr workspace with the required dependencies.

### Compile

```bash
west build -p always \
-b stm32h7b3i_dk \
"$HOME/VSCodeProjects/zephyr-lvgl/applications/stm32h7b3idk_demo"
```

### Flash

```bash
sudo "$(which python)" "$(which west)" flash -r openocd
```

The application uses the board's onboard ST-LINK debugger.

---

## Project Structure

```text
stm32h7b3idk_demo/
├── CMakeLists.txt
├── prj.conf
├── README.md
├── assets/
│   ├── morph_ball.c
│   └── morph_ball.png
└── src/
    └── main.c
```

- `main.c` — application initialization, animation, input handling, and performance monitoring.
- `morph_ball.c` — LVGL-compatible image data.
- `morph_ball.png` — original sprite asset.
- `prj.conf` — Zephyr and LVGL configuration.

---

## Notes

The application demonstrates a native **Zephyr + LVGL** graphics stack on STM32H7 hardware.

LTDC handles continuous display scanout, while LVGL manages the graphical interface, image transformations, and refresh scheduling.

The Morph Ball rotation angle is calculated directly from its horizontal position, preventing cumulative rounding errors and keeping rotation synchronized with movement.

The project intentionally avoids additional graphics frameworks and vendor-specific application layers.

---

## License

See the repository's main license for details.

*Metroid* and Samus Aran are trademarks and intellectual property of Nintendo. This is an unofficial, non-commercial technical demonstration and is not affiliated with or endorsed by Nintendo.
