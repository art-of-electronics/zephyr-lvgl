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

- Custom Zephyr board definitions for embedded display platforms.
- Support for parallel RGB and SPI display interfaces.
- LVGL integration with interactive elements and animations.
- Capacitive and resistive touchscreen support.
- Build-time display rotation and orientation-aware touch mapping.
- RGB LED and display backlight control.
- PWM-based brightness adjustment.
- ADC-based sensor measurements.
- Demonstration applications for hardware validation and peripheral testing.

---

## Supported Hardware

| Board | MCU | Display | Touch | Status |
|---|---|---|---|---|
| VIEWE UEDX80480050E-WB-A v1.2 | ESP32-S3-WROOM-1 N16R8 | 800 × 480 RGB LCD | GT911 | Tested |
| VIEWE UEDX80480043E-WB-A v1.3 | ESP32-S3-WROOM-1 N16R8 | 480 × 272 RGB LCD | GT911 | Tested |
| CYD ESP32-035 | ESP32-D0WD-V3 | 320 × 480 ST7796S SPI LCD | XPT2046 | Tested |

Hardware-specific configurations, supported features, and known limitations are documented in the corresponding demonstration applications.

---

## Repository Structure

```text
zephyr-lvgl/
├── applications/
│   ├── viewe_demo/
│   └── cyd_demo/
│
├── boards/
│   ├── viewe/
│   └── cyd/
│
└── drivers/
```

- **`applications/`** — LVGL demonstration applications and hardware validation workloads.
- **`boards/`** — Custom Zephyr board definitions, grouped by manufacturer.
- **`drivers/`** — Custom peripheral drivers maintained independently of board-specific configurations.

Each application contains its own build instructions, hardware configuration details, and validation notes.

---

## Software

The project has been developed and tested using:

- **Zephyr RTOS 4.5.0** (tag `v4.5.0`)
- **LVGL 9.6**
- Zephyr SDK
- West build and flash tools

### Zephyr Compatibility

The currently supported hardware has been tested with **Zephyr RTOS v4.5.0**.

To reproduce the tested Zephyr environment, run the following commands from the root of an existing West workspace:

```bash
git -C zephyr fetch origin
git -C zephyr switch --detach v4.5.0
west update
```

The `west update` command synchronizes Zephyr modules with the revisions specified by the selected manifest.

Compatibility with other Zephyr revisions and hardware platforms requires separate validation.

---

## Getting Started

Each supported hardware family has a dedicated demonstration application containing build instructions, configuration options, and validation details.

| Application | Supported Hardware | Documentation |
|---|---|---|
| `viewe_demo` | VIEWE ESP32-S3 displays | [README](applications/viewe_demo/README.md) |
| `cyd_demo` | CYD ESP32-035 | [README](applications/cyd_demo/README.md) |

Select the appropriate application and follow its documentation to build, flash, and test the target hardware.

---

**Built with Zephyr, LVGL, and a suspicious amount of debugging.**
