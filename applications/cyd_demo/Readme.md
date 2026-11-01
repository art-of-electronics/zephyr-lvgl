# CYD Demo

**Zephyr RTOS × ESP32-035 × LVGL**

Demonstration application for the **CYD ESP32-035**, a low-cost ESP32 development board featuring a 3.5-inch SPI display, resistive touchscreen, RGB LED, and ambient light sensor.

The application demonstrates the integration of custom board support, display drivers, and peripheral interfaces with **Zephyr RTOS 4.5.0** and **LVGL**.

---

## Hardware

| Component | Description |
|---|---|
| MCU | ESP32-D0WD-V3 |
| CPU | Dual-core Xtensa LX6, up to 240 MHz |
| Flash | 4 MB |
| Display | 3.5-inch TFT, 320 × 480 |
| Display controller | ST7796S (SPI) |
| Touchscreen | XPT2046 (resistive, SPI) |
| RGB LED | Onboard RGB LED |
| Backlight | PWM-controlled |
| Light sensor | LDR (GPIO34 / ADC1_CH6) |
| USB-to-UART | CH340 |

### Board Target

```text
esp32_035/esp32/procpu
```

The custom board definition is located in:

```text
boards/cyd/esp32_035/
```

---

## Features

The current implementation provides:

- Custom Zephyr board support for the CYD ESP32-035.
- ST7796S display initialization and SPI communication.
- LVGL graphics rendering and demonstration widgets.
- XPT2046 resistive touchscreen support.
- Touchscreen calibration and coordinate transformation.
- Configurable display orientation:
  - 0° — portrait
  - 90° — landscape
  - 180° — inverted portrait
  - 270° — inverted landscape
- Automatic touchscreen coordinate adjustment for the selected display orientation.
- RGB LED control.
- PWM-based display backlight control.
- Ambient light measurements using the ESP32 ADC.
- Application and peripheral diagnostics through Zephyr logging.

---

## Requirements

- Zephyr RTOS **4.5.0**
- Zephyr SDK
- Python 3
- West build tool
- ESP32 flashing tools supported by Zephyr

The following instructions assume that Zephyr is installed in:

```text
~/zephyrproject/
```

and the `zephyr-lvgl` repository is located in:

```text
~/VSCodeProjects/zephyr-lvgl/
```

---

## Environment Setup

Activate the Zephyr Python virtual environment:

```bash
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
```

If the virtual environment already exists, only activation is required.

The Zephyr development environment and dependencies must be configured before building the application.

---

## Building

Build the application using the custom ESP32-035 board definition:

```bash
west build -p always \
"$HOME/VSCodeProjects/zephyr-lvgl/applications/cyd_demo" \
-b esp32_035/esp32/procpu \
-- -DBOARD_ROOT="$HOME/VSCodeProjects/zephyr-lvgl"
```

The `BOARD_ROOT` parameter allows Zephyr to locate the custom board definition provided by this repository.

The resulting firmware is generated in the build directory.

---

## Flashing

Connect the CYD ESP32-035 board to the host computer using USB.

Flash the firmware:

```bash
west flash
```

The flashing procedure uses the runner configured for the board.

If automatic serial port detection fails, specify the appropriate serial port according to the selected runner configuration.

---

## Display Configuration

The application uses the **ST7796S** display controller with a native resolution of **320 × 480 pixels**.

The display is connected through the ESP32 SPI interface and integrated with LVGL using Zephyr's display subsystem.

### Display Rotation

The board supports four display orientations.

| Rotation | Orientation | LVGL Resolution |
|---|---|---|
| 0° | Portrait | 320 × 480 |
| 90° | Landscape | 480 × 320 |
| 180° | Inverted portrait | 320 × 480 |
| 270° | Inverted landscape | 480 × 320 |

Rotation is selected at build time using the board-specific Kconfig options.

Select exactly one option in `prj.conf`:

**0° — Portrait**

```ini
CONFIG_CYD_LCD_ROTATION_0=y
```

**90° — Landscape**

```ini
CONFIG_CYD_LCD_ROTATION_90=y
```

**180° — Inverted portrait**

```ini
CONFIG_CYD_LCD_ROTATION_180=y
```

**270° — Inverted landscape**

```ini
CONFIG_CYD_LCD_ROTATION_270=y
```

The selected orientation controls:

- ST7796S memory access configuration (`MADCTL`).
- Display width and height.
- Touchscreen coordinate transformations.

The application generates the corresponding Devicetree overlay during the CMake configuration stage.

A pristine rebuild is recommended after changing the rotation configuration.

---

## Touchscreen

The board uses the **XPT2046** resistive touchscreen controller.

Touch input is handled through Zephyr's input subsystem and integrated with LVGL.

The touchscreen configuration includes:

- Raw coordinate calibration.
- X/Y coordinate inversion.
- X/Y axis swapping.
- Orientation-dependent coordinate transformation.

Touchscreen settings are synchronized with the selected display rotation.

All four orientations have been tested with touch input.

---

## RGB LED

The onboard RGB LED is controlled through Zephyr's GPIO subsystem.

The demonstration application provides basic RGB LED operation, allowing the individual color channels to be controlled.

---

## Display Backlight

Display backlight brightness is controlled using the ESP32 PWM peripheral.

The implementation uses Zephyr's PWM subsystem.

PWM control allows software-based brightness adjustment without modifying the display controller configuration.

---

## Ambient Light Sensor

The CYD ESP32-035 includes an onboard light-dependent resistor (LDR).

The sensor is connected to:

| Parameter | Value |
|---|---|
| GPIO | GPIO34 |
| ADC unit | ADC1 |
| ADC channel | 6 |
| Zephyr controller | `adc0` |
| ADC resolution | 12 bits |

The application periodically reads the ADC and reports the measured values through Zephyr logging.

Example:

```text
<inf> cyd_demo: LDR: raw=1399, voltage=1503 mV
```

### Hardware Limitations

The onboard LDR circuit uses a voltage divider consisting of:

- R15 — pull-up resistor.
- R19 — pull-down resistor.
- R21 — light-dependent resistor.

The LDR and R19 are connected in parallel between GPIO34 and GND.

The resulting voltage divider exhibits poor sensitivity under normal ambient lighting conditions.

The ADC input voltage rapidly approaches the lower measurement limit even under relatively weak illumination.

Meaningful measurements are primarily obtained under very low lighting conditions.

**The limitation originates from the onboard analog circuitry rather than the Zephyr ADC driver.**

Improving the usable measurement range may require reducing the resistance of R15.

No hardware modifications are required to build or run the demonstration application.

---

## Logging

Application diagnostics are available through Zephyr's logging subsystem.

The demo provides information about:

- Peripheral initialization.
- Display and touchscreen operation.
- ADC measurements.
- Peripheral errors.

Additional ADC diagnostics can be enabled in `prj.conf`:

```ini
CONFIG_ADC_LOG_LEVEL_DBG=y
```

This enables detailed information from the ESP32 ADC driver, including hardware conversion results and calibrated voltage values.

Debug logging is intended for development and troubleshooting.

---

## Validation

The application has been tested on the CYD ESP32-035 with Zephyr RTOS 4.5.0.

| Feature | Status |
|---|---|
| Custom board initialization | ✅ |
| ST7796S display | ✅ |
| LVGL rendering | ✅ |
| XPT2046 touchscreen | ✅ |
| 0° rotation | ✅ |
| 90° rotation | ✅ |
| 180° rotation | ✅ |
| 270° rotation | ✅ |
| RGB LED | ✅ |
| PWM backlight | ✅ |
| ADC / LDR measurements | ✅ |
| LDR sensitivity in normal lighting | ⚠️ Hardware limitation |

---

## Project Structure

```text
zephyr-lvgl/
├── applications/
│   └── cyd_demo/
│       ├── CMakeLists.txt
│       ├── prj.conf
│       └── README.md
│
└── boards/
    └── cyd/
        └── esp32_035/
```

The application demonstrates the hardware interfaces provided by the custom board definition.

Board-specific hardware configuration is maintained separately from the application whenever possible.

---

## Notes

This project is intended as an experimental Zephyr platform for exploring custom ESP32 board definitions, SPI display integration, LVGL, touchscreen input, and peripheral control.

The demonstration application is not intended to provide production-ready ambient light measurement functionality.

Hardware-specific limitations are documented to distinguish board behavior from software implementation issues.

---
