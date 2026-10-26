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
- LVGL integration with hardware-accelerated display data transfer via GDMA.
- GT911 capacitive touchscreen integration.
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

The project is developed using:

- Zephyr RTOS 4.4
- LVGL 9.x
- Zephyr SDK
- West build and flash tools

## Getting Started

The current demonstration targets the VIEWE UEDX80480050E-WB-A v1.2 board.

See [Viewe Demo](applications/viewe_demo/Readme.md) for build instructions and application details.

## Implementation Notes

The ESP32-S3 RGB display driver uses a continuous LCD_CAM scanout pipeline with GDMA and internal-SRAM bounce buffers.

Depending on the selected configuration, one, two, or three framebuffers are allocated in external PSRAM.

Double- and triple-buffering support synchronized presentation and dirty-region copying to reduce unnecessary memory traffic.

The implementation is currently hardware-specific and has been validated on the supported VIEWE board.

## Project Status

**Working prototype — hardware validated.**

The current milestone includes:

- LCD_CAM RGB display initialization and scanout.
- GDMA-based framebuffer transfers.
- LVGL rendering and animations.
- GT911 touchscreen input.
- Interactive GUI elements.

Further development may include additional boards, display controllers, and demonstration applications.

---

**Built with Zephyr, LVGL, and a suspicious amount of debugging.**
