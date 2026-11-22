# ESP32-S3 LCD_CAM RGB display driver (Zephyr)

Custom Zephyr display driver for the **ESP32-S3 LCD_CAM** peripheral in 16-bit parallel RGB mode. **Driver v2** uses continuous GDMA TX scanout, two internal-SRAM bounce buffers, **three RGB565 framebuffers in external PSRAM**, and tile-based partial-update synchronization.

Unlike the legacy driver, v2 has **one framebuffer architecture: triple buffering**. It supports both normal Zephyr `display_write()` (including multi-part LVGL flushes) and an explicit framebuffer lease API for applications that render directly into PSRAM.

> **Scope:** This document covers the v2 driver, not a particular board definition or the GT911 input driver. Other panels and memory configurations need target-specific validation. The source snapshot used for this README is the cleaned Chapter VIII version; some integration Kconfig options are defined outside this driver's local `Kconfig`.

## 1. Tested hardware

| Board | RGB panel | Requested PCLK | Bounce geometry | Status |
| --- | --- | --- | --- | --- |
| VIEWE UEDX80480050E-WB-A v1.2 | 800 × 480 | 20 MHz | 10 rows × 2 buffers | v2 scanout, LVGL, partial writes and rotations exercised |
| VIEWE UEDX80480043E-WB-A v1.3 (480 × 272 variant) | 480 × 272 | 16 MHz | 8 rows × 2 buffers | v2 scanout and GUI operational; rerun complete regression after cleanup |

These pixel-clock values come from panel devicetree configuration. RGB porches, GPIO/pinctrl, PSRAM and GT911 wiring belong to the board definitions; **do not copy display timing values between panels without checking the datasheets**.

## 2. Architecture

```text
          LVGL / Zephyr display API            Direct framebuffer API
          display_write()                      acquire() / submit() / cancel()
                |                                        |
      RGB565X + optional rotation              exclusive DRAWING lease
                |                                        |
                +----------------+-----------------------+
                                 |
                    Three RGB565 PSRAM frames
                     SCANNING / QUEUED / DRAWING
                                 |
                  Dirty-tile state (32 × 16 px)
                   selective PSRAM frame sync
                                 |
                  selected SCANNING framebuffer
                                 |
                     CPU memcpy() refill
                  (GDMA TX EOF callback)
                                 |
                   Two SRAM bounce buffers
                  ping-pong, prefilled in turn
                                 |
                     Circular GDMA TX ring
                                 |
                       ESP32-S3 LCD_CAM
                                 |
                          RGB panel
```

LCD_CAM timing runs continuously. GDMA TX reads prefilled internal-SRAM bounce buffers and supplies the LCD pixel stream. Each bounce completion triggers a callback that reserves the drained bounce, selects the required PSRAM source, and refills that bounce using **CPU `memcpy()`**. The bounce state machine validates ownership transitions; an invalid or late refill marks the pipeline for recovery.

**GDMA memory-to-memory (M2M) is not used in the production refill path.** The production DMA is the LCD TX channel.

### Frame ownership

The frame manager maintains three distinct roles:

| State | Meaning |
| --- | --- |
| `SCANNING` | PSRAM source for subsequent bounce refills; must not be modified |
| `QUEUED` | Complete frame awaiting a safe source switch |
| `DRAWING` | Exclusively writable framebuffer owned by one writer |
| `FREE` | Available for a new drawing lease |

Initially framebuffer 0 is `SCANNING`; the other two are `FREE`. Only one `DRAWING` lease and one `QUEUED` frame are permitted at a time. On successful submission, `DRAWING` becomes `QUEUED`. At the **prefetch wrap boundary**, the queued frame becomes `SCANNING`, the former scanning frame becomes `FREE`, and any drawing lease stays exclusive.

This is **not** a mailbox that silently replaces an already queued frame. A second submission returns `-EBUSY` until the previous queued frame is latched; for the direct API, ownership remains with the caller.

### Why the switch happens before VSYNC

Two bounce buffers hold pixels in advance of actual LCD consumption. The driver switches the PSRAM source when the next refill is about to read **row zero of the following frame**, rather than waiting until VSYNC. This ensures prefetched next-frame rows come from a coherent new frame.

## 3. Memory and GDMA geometry

The relevant compile-time geometry is derived from the physical resolution in devicetree:

```c
#define RGB_V2_BOUNCE_LINES  ((0U == (RGB_V2_HEIGHT % 20U)) ? (10U) : (8U))
#define RGB_V2_FB_BYTES      (RGB_V2_WIDTH * RGB_V2_HEIGHT * sizeof(uint16_t))
#define RGB_V2_BOUNCE_BYTES  (RGB_V2_WIDTH * RGB_V2_BOUNCE_LINES * sizeof(uint16_t))
#define RGB_V2_DESC_BYTES    (RGB_V2_WIDTH * 2U * sizeof(uint16_t))
```

| Resource | 800 × 480 | 480 × 272 |
| --- | ---: | ---: |
| One PSRAM framebuffer | 768,000 B (750 KiB) | 261,120 B (255 KiB) |
| Three PSRAM framebuffers | 2,304,000 B (2,250 KiB) | 783,360 B (765 KiB) |
| Rows per SRAM bounce | 10 | 8 |
| One SRAM bounce | 16,000 B | 7,680 B |
| Two SRAM bounces | 32,000 B | 15,360 B |
| Payload per GDMA descriptor | 3,200 B | 1,920 B |
| Descriptors per bounce | 5 | 4 |
| Total circular-ring descriptors | 10 | 8 |
| Expected bounce EOF events per frame | 48 | 34 |

PSRAM framebuffer allocations use the external shared heap and **64-byte alignment**. Bounce storage and scanout state live in internal memory; descriptor structures and other driver state require additional space beyond the byte counts above.

Compile-time assertions check the frame/bounce divisibility, descriptor alignment and payload limits, and FIFO-recovery constraints. The selected `10`/`8` row geometry and descriptor layout are **not a general-purpose formula for arbitrary RGB panels**: review them before porting to a different resolution.

### Devicetree integration

The current implementation reads:

- Display compatible `espressif,esp32-lcd-cam-rgb` (`DT_INST(0, espressif_esp32_lcd_cam_rgb)`).
- Physical `width`, `height`, and `display_timings` child (pixel clock, sync lengths and porches).
- LCD_CAM pinctrl associated with `DT_NODELABEL(lcd_cam)`.
- LCD_CAM GDMA `tx` controller/channel from the node's `dmas` property.

Only **16-bit RGB565X** is exposed to Zephyr (`PIXEL_FORMAT_RGB_565X`). The hardware clock, RGB interface, and descriptor ring use the physical panel geometry even when the logical display is rotated.

## 4. Partial rendering and dirty tiles

`display_write()` accepts an RGB565X source buffer and a `display_buffer_descriptor`. It validates bounds, pitch and source-buffer size, then copies pixels into the currently leased PSRAM framebuffer. For 0° orientation it uses row-by-row `memcpy()`; for rotated modes it maps pixels into physical coordinates in software.

`desc->frame_incomplete` controls publication:

1. The first write in a new frame claims a `DRAWING` buffer.
2. Only tiles that are stale in this buffer are synchronized from the latest complete source (`QUEUED` if present, otherwise `SCANNING`).
3. Each write records the physical tiles it changes.
4. Intermediate writes with `frame_incomplete = true` keep the lease open.
5. The final write (`frame_incomplete = false`) flushes the framebuffer cache and submits one coherent frame.

Dirty tracking uses a **32 × 16 px tile grid**, with a per-frame stale bitmap and a changed-tile bitmap. At most **1,024 tiles** are supported by the current tracker structure. The validated geometries use 750 tiles (800 × 480) and 255 tiles (480 × 272). `rgb_v2_dirty_sync()` copies only obsolete tiles; tile-edge copies are clipped to panel dimensions.

A direct framebuffer submission marks the **whole frame** changed because the driver cannot infer which pixels the application modified. Partial writes instead publish the precise set of touched tiles.

### Write-path waiting

The `display_write()` path uses a thread-context mutex and a **200 ms** bounded wait for an available lease or queue slot, checking availability at 1 ms intervals while waiting to submit. If publication cannot complete within that deadline, the write fails with an error (normally `-ETIMEDOUT`) and cancels the incomplete drawing lease. The direct API does **not** implicitly wait for `-EBUSY` to clear.

## 5. Direct framebuffer API

Declared in `rgb_v2_display_control.h`:

```c
int rgb_v2_display_acquire(const struct device *dev, uint8_t *index,
                           uint16_t **pixels);
int rgb_v2_display_submit(const struct device *dev, uint8_t index);
int rgb_v2_display_cancel(const struct device *dev, uint8_t index);
```

These functions operate on full-screen **RGB565 PSRAM buffers**. `acquire()` grants a single exclusive `DRAWING` lease; do not access the pointer after successful `submit()` until it has been acquired again. `submit()` flushes the **entire** framebuffer cache range and queues the completed image. On `-EBUSY`, the caller retains the `DRAWING` lease and may retry or cancel. `cancel()` releases unfinished work without presenting it.

Use these calls from **thread context**. Do not concurrently submit/cancel the same lease from separate callers, and do not modify `QUEUED` or `SCANNING` buffers.

## 6. Orientation and display API

Four **build-time** orientations are supported:

| Orientation | Logical resolution | Selection |
| --- | --- | --- |
| 0° | W × H | `CONFIG_ESP32_LCD_RGB_ROTATION_0=y` |
| 90° | H × W | `CONFIG_ESP32_LCD_RGB_ROTATION_90=y` |
| 180° | W × H | `CONFIG_ESP32_LCD_RGB_ROTATION_180=y` |
| 270° | H × W | `CONFIG_ESP32_LCD_RGB_ROTATION_270=y` |

Select **exactly one** rotation option in project configuration. Runtime changes to a different orientation are not supported (`-ENOTSUP`). `get_capabilities()` reports the logical dimensions after rotation, while LCD_CAM retains the physical resolution and timing.

The Zephyr display API implements `write`, `get_capabilities`, `set_pixel_format`, and `set_orientation`. Supported pixel format: `PIXEL_FORMAT_RGB_565X` only. **Touch rotation is separate:** GT911 is integrated through Zephyr input/LVGL and is not driven by the LCD_CAM display module.

## 7. Build configuration

The driver was exercised with **Zephyr 4.5.0**, an ESP32-S3 board definition configured for external PSRAM, LCD_CAM and GDMA TX, and LVGL 9.6 in the demo application. A representative selection is:

```ini
# Select the v2 backend while the legacy backend still exists.
CONFIG_ESP32_LCD_RGB_BACKEND_V2=y

# Display orientation (select one).
CONFIG_ESP32_LCD_RGB_ROTATION_0=y
# CONFIG_ESP32_LCD_RGB_ROTATION_90=y
# CONFIG_ESP32_LCD_RGB_ROTATION_180=y
# CONFIG_ESP32_LCD_RGB_ROTATION_270=y

# Optional periodic scanout diagnostics.
CONFIG_LOG=y
CONFIG_ESP32_LCD_RGB_V2_STATS=y
```

The local `Kconfig` currently provides a mutually exclusive v1/v2 backend choice and the v2 statistics toggle. **Once v1 is removed, update the backend selection and build rules** rather than leaving the legacy option documented as permanent. Rotation symbols belong to the surrounding project configuration and should be verified after that cleanup.

This README does not prescribe a universal board overlay. Verify actual GDMA channel, RGB pinctrl, timings, external-memory heap sizing and Zephyr configuration against the target board.

## 8. Diagnostics and recovery

With `CONFIG_ESP32_LCD_RGB_V2_STATS=y`, a delayed work item reports scanout statistics approximately once per second, independent of `display_write()` activity:

| Field | Meaning |
| --- | --- |
| `EOF` | Cumulative completed bounce-group callbacks |
| `VSYNC` | Cumulative LCD_CAM VSYNC-end events |
| `recoveries` | LCD TX recoveries performed |
| `dma_err` | Nonzero status events from the GDMA callback |
| `misses` | Diagnostic missed/pipeline count as reported by the build |
| `refill_max` | Maximum measured CPU PSRAM→SRAM refill duration during the reporting window (µs) |
| `scan`, `queued`, `drawing` | Frame-manager ownership indices |
| `acquired`, `submitted`, `latched`, `released` | Cumulative frame-management activity |
| `short`, `pending`, `pipeline` | Short-frame, pending-fault and bounce-pipeline diagnostic counters |
| `last_err`, `eof`, `over`, `last_short`, `min`, `max` | Additional recovery detail and EOF counts |

At each VSYNC the driver compares EOF progress against the expected count (48 for 800 × 480; 34 for 480 × 272). If the frame is short or a pipeline fault is pending, the recovery sequence resets LCD FIFO and GDMA TX, primes both bounces, and restarts TX through a dedicated recovery descriptor. The recovery mechanism is a **fallback**, not an expected steady-state event. A sustained increase in `recoveries`, `short`, or `dma_err` warrants investigation.

The periodic work item itself does not run in the ISR. Keep costly logging and blocking operations out of the bounce-refill callback.

## 9. Why the production path does not use GDMA M2M

Research evaluated a separate GDMA memory-to-memory channel for PSRAM→SRAM bounce refill. Data integrity eventually passed **100/100** transfers with 4,032-byte aligned M2M descriptor chunks and ISR-chained transfer rearming; however, LCD scanout and M2M competed for resources.

Measurements on the **800 × 480** board, using 100 consecutive transfers of **16,000 bytes** each:

| Experiment | M2M throughput | LCD recoveries during burst |
| --- | ---: | ---: |
| LCD OFF; no LVGL | **35.44 MB/s** | Not applicable |
| LCD ON; no LVGL | **17.91 MB/s** | 4 |
| LCD ON with LVGL | **~17.41 MB/s** | 5 |

These are **development measurements**, not interchangeable production FPS figures or direct `memcpy()`-versus-M2M performance benchmarks. The LCD ON test without LVGL shows that the conflict is present independently of GUI rendering. Adding M2M to the critical refill path would also introduce extra descriptor and synchronization complexity. **Production v2 retains CPU `memcpy()`**.

## 10. Constraints and maintenance notes

1. **ESP32-S3-specific.** Register-level LCD_CAM/GDMA setup is not a portable generic display driver.
2. **Triple buffering only.** The v2 frame manager assumes exactly three PSRAM frames; the legacy 1/2 FB architectures are not v2 features.
3. **RGB565X only; rotation at build time.** The selected physical scanout resolution never changes with logical rotation.
4. **Memory bandwidth and interrupt latency matter.** Both CPU bounce refill and dirty-frame sync read external PSRAM; keep ISR refill and buffer ownership paths predictable.
5. **Ownership and cache coherence matter.** Full-frame submission flushes the PSRAM cache before queuing. If adding another DMA engine or core, reassess data ownership and cache operations.
6. **GDMA geometry is constrained.** New panel sizes may require different bounce lengths, descriptor sizes, assertions, and timing validation.
7. **Recovery is not normal operation.** A nonzero recovery count during heavy stress should be investigated even if the image looks correct.
8. **Input is separate.** The display driver does not implement GT911, pointer event mapping, or LVGL configuration.

## 11. Suggested regression checklist

- Build both 800 × 480 and 480 × 272 targets with diagnostics enabled and disabled.
- Confirm pinctrl, requested PCLK, display colors and expected 48/34 EOF events per VSYNC.
- Exercise repeated partial `display_write()` calls with `frame_incomplete = true`, followed by the final flush.
- Test isolated and overlapping dirty tiles, screen edges, and a full-frame update.
- Validate frame leases (`acquire`, `submit`, `cancel`) including `-EBUSY` ownership retention.
- Test 0°, 90°, 180°, 270° with LVGL and GT911 input mapping.
- Run Morph Ball and the multi-region stress workload; inspect tearing, stale tiles, drops and synchronization behavior.
- Verify stable `recoveries=0`, `dma_err=0`, and no bounce pipeline faults during normal sustained operation.
- Check startup failure paths, heap exhaustion, repeat initialization where supported, and recovery after deliberate stress.

## 12. Implementation map

| File | Responsibility |
| --- | --- |
| `display_esp32_lcd_cam_rgb_v2.c` | Zephyr display device registration, capabilities, orientation and public framebuffer API |
| `rgb_v2_scanout.[ch]` | LCD_CAM clocks/timings, GDMA TX descriptors, SRAM bounce refill, VSYNC recovery, partial writes |
| `rgb_v2_bounce_state.[ch]` | Ownership/state transitions for the two SRAM bounces; refill tokens and fault handling |
| `rgb_v2_frame_manager.[ch]` | Three-frame `FREE` / `DRAWING` / `QUEUED` / `SCANNING` lifecycle |
| `rgb_v2_dirty.[ch]` | Dirty-tile tracking, publication and selective framebuffer synchronization |
| `rgb_v2_rotation.[ch]` | Coordinate and rectangle transforms for fixed orientations |
| `rgb_v2_display_control.h` | Direct framebuffer lease API |
| `Kconfig`, `CMakeLists.txt` | Backend selection, diagnostics and source integration |

---

**Development status (November 2026):** Dricer v2 has been exercised on the two VIEWE ESP32-S3 RGB panels. The validated architecture uses triple PSRAM buffering, dirty-tile synchronization and CPU-refilled SRAM bounces feeding continuous GDMA TX. The driver is undergoing final cleanup and regression testing before the legacy backend is removed. This document describes the source implementation and development observations, **not a claim of production qualification**.
