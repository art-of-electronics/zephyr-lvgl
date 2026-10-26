# ESP32-S3 LCD_CAM RGB display driver (Zephyr)

Custom Zephyr display driver for the **ESP32-S3 LCD_CAM** peripheral operating in parallel RGB mode.

The driver implements continuous RGB scanout using a circular **GDMA descriptor ring**, two internal-SRAM **bounce buffers**, and **1, 2, or 3 framebuffers in external PSRAM**. Double- and triple-buffering synchronize framebuffer presentation with the scanout prefetch boundary and use per-region synchronization to avoid unnecessary PSRAM traffic.

Panel dimensions, RGB timing parameters, LCD_CAM pin assignments, and the GDMA channel come from **devicetree**. The same driver has been brought up on both 800 × 480 and 480 × 272 ESP32-S3 display boards without panel-specific driver branches.

> **Scope:** This README documents the current ESP32-S3 LCD_CAM RGB driver implementation, not a generic RGB controller or any particular board definition. The supported data path and DMA geometry have been exercised on the hardware listed below; other resolutions, timings, and memory configurations require validation.

### Validated hardware

| Board | Panel resolution | Requested pixel clock | Status |
| --- | --- | ---: | --- |
| VIEWE UEDX80480050E-WB-A v1.2 | 800 × 480 | 20 MHz | Working; extensive LVGL, framebuffer, rotation, and touch integration testing |
| VIEWE UEDX80480043E-WB-A v1.3 (480 × 272 variant) | 480 × 272 | 16 MHz | Display operational with the shared driver; full regression matrix pending |

Both boards use an ESP32-S3 platform with external PSRAM. The requested pixel clocks above are **board/panel configuration values**, not hard-coded driver settings or independent measurements of the actual output clock. Board-specific RGB porches, sync timings, pinctrl, PSRAM setup, and touchscreen wiring belong in the respective board definitions.

## 1. Architecture

```text
           LVGL / Zephyr display API
                      |
                display_write()
                      |
                 DRAW buffer
               (PSRAM, RGB565X)
                      |
        [1 FB: shared with FRONT]
        [2 FB: FRONT / BACK + sync]
        [3 FB: FRONT / READY / DRAW]
                      |
               FRONT framebuffer
                 (external PSRAM)
                      |
               lcd_bounce_fill()
                 memcpy, in ISR
                      |
           +------------------------+
           | SRAM bounce 0 (8 lines)| <---+
           | SRAM bounce 1 (8 lines)| <---+ alternating refills
           +------------------------+
                      |
              GDMA descriptor ring
       (circular; size depends on width)
                      |
               LCD_CAM RGB output
                      |
              RGB panel (from DTS)
```

The **LCD timing engine runs continuously**. GDMA consumes descriptor-backed SRAM data while the GDMA EOF callback refills the freed bounce buffer from the currently selected front framebuffer. The code contains a VSYNC recovery path for underruns; this is not a normal per-frame restart.

### Memory requirements

The framebuffer size is calculated from the configured physical panel dimensions:

```text
framebuffer_size     = width × height × 2 bytes
bounce_buffer_size   = width × LCD_BOUNCE_LINES × 2 bytes
bounce_storage_total = LCD_BOUNCE_COUNT × bounce_buffer_size
```

With the current 8-line bounce buffers and 1,280-byte DMA payloads:

| Resource | 800 × 480 | 480 × 272 |
| --- | ---: | ---: |
| One PSRAM framebuffer (RGB565X) | 768,000 B (750 KiB) | 261,120 B (255 KiB) |
| 1 / 2 / 3 PSRAM framebuffers | 750 / 1,500 / 2,250 KiB | 255 / 510 / 765 KiB |
| One internal-SRAM bounce buffer | 12,800 B | 7,680 B |
| Both bounce buffers | 25,600 B | 15,360 B |
| One DMA descriptor payload | 1,280 B | 1,280 B |
| Descriptors per bounce buffer | 10 | 6 |
| Total descriptors in the circular ring | 20 | 12 |

Descriptor bookkeeping, synchronization state, work items (2 FB), and optional diagnostics require additional memory. Framebuffers are allocated from the external-memory shared heap with **32-byte alignment**. Descriptor payload size is **not** the memory consumed by each descriptor structure.

## 2. RGB and clock configuration

The driver obtains panel geometry and RGB timings from devicetree:

- Compatible: `espressif,esp32-lcd-cam-rgb` (C macro `espressif_esp32_lcd_cam_rgb`).
- LCD_CAM hardware node: `DT_NODELABEL(lcd_cam)`.
- Display `width`, `height`, and the `display_timings` child: `clock-frequency`, horizontal/vertical sync length, back porch, and front porch.
- Pin assignments: LCD_CAM pinctrl in the board definition.
- GDMA TX controller and channel: `dmas` entry named `tx` on the LCD_CAM node.

The driver uses **16-bit RGB data** and exposes the Zephyr display format **`PIXEL_FORMAT_RGB_565X`**. Four display orientations (0°, 90°, 180°, and 270°) are supported through build-time Kconfig selection.

The pixel clock and porch values are **properties of the attached RGB panel**, not constants imposed by the driver. Do not copy timings blindly between board DTS files, even when the same firmware appears to display an image on both panels.

The physical scanout rate depends on the generated pixel clock and the complete horizontal and vertical timing periods; it is distinct from the LVGL rendering rate and the driver's completed framebuffer-presentation rate.

## 3. GDMA scanout and VSYNC boundary

Current driver constants and geometry:

```c
#define LCD_BOUNCE_LINES            (8U)
#define LCD_BOUNCE_COUNT            (2U)
#define LCD_BOUNCE_SIZE             (LCD_HACTIVE * LCD_BOUNCE_LINES * sizeof(uint16_t))
#define LCD_DMA_DESC_SIZE           (1280U)
#define LCD_DESCS_PER_BOUNCE        (LCD_BOUNCE_SIZE / LCD_DMA_DESC_SIZE)
#define LCD_DMA_DESC_COUNT          (LCD_DESCS_PER_BOUNCE * LCD_BOUNCE_COUNT)
#define LCD_EXPECTED_EOF_PER_FRAME  (LCD_VACTIVE / LCD_BOUNCE_LINES)
#define LCD_PRESENT_PREFETCH_EOF    (LCD_EXPECTED_EOF_PER_FRAME - LCD_BOUNCE_COUNT)
```

These are **common driver settings**, not board-specific conditionals. `LCD_BOUNCE_SIZE`, the number of descriptors, and the expected EOF count are derived from the dimensions supplied by devicetree. The fixed descriptor payload size happens to satisfy the constraints of both validated panels; it is **not guaranteed** to work for an arbitrary new resolution.

| Scanout parameter | 800 × 480 | 480 × 272 |
| --- | ---: | ---: |
| Bounce-buffer lines | 8 | 8 |
| DMA descriptors per bounce | 10 | 6 |
| Expected bounce-buffer EOF events per frame | 60 | 34 |
| Presentation prefetch EOF threshold | 58 | 32 |
| Lines held across two bounce buffers | 16 | 16 |

The callback sequence is:

1. GDMA reports completion of a bounce-buffer group (`suc_eof` on the final descriptor for that group).
2. `lcd_dma_callback()` refills the released bounce buffer with the next `LCD_BOUNCE_LINES` lines from FRONT.
3. At `LCD_PRESENT_PREFETCH_EOF`, the driver considers a change of FRONT for the next frame.
4. `lcd_vsync_isr()` maintains line/EOF bookkeeping and checks whether a recovery restart is needed.

The **prefetch boundary**, rather than an arbitrary write-completion timestamp, determines when FRONT may switch. This is essential because the DMA pipeline already contains data for upcoming display lines.

### DMA geometry constraints

Compile-time `BUILD_ASSERT` checks require, among other things:

- The display height to be divisible by `LCD_BOUNCE_LINES`.
- `LCD_BOUNCE_SIZE` to be divisible by `LCD_DMA_DESC_SIZE`.
- Descriptor payload size to be no greater than 4,095 bytes and aligned to the configured GDMA PSRAM burst size (32 bytes).
- Bounce buffers and framebuffers to satisfy 32-byte cache alignment.
- The descriptor size to be larger than the FIFO-skip byte count used during recovery.
- Enough EOF groups per frame to leave room for the two-buffer prefetch pipeline.
- Sufficient configured external-memory heap for the requested framebuffer count.

A different panel resolution may therefore require revisiting the **shared** DMA geometry. Do not remove these assertions to make an unsupported geometry compile.

### VSYNC recovery

If EOF progress is below the expected count at VSYNC, the recovery path resets the LCD FIFO, resets the GDMA TX channel, then starts a dedicated descriptor that skips the bytes already held in the LCD FIFO. The LCD timing engine is **not stopped** during this operation. This is an underrun recovery mechanism, not a requirement for steady-state operation.

## 4. Framebuffer modes

Select the build-time framebuffer count with `CONFIG_ESP32_LCD_RGB_FRAMEBUFFER_COUNT` (supported values: **1–3**). The PSRAM heap must be large enough; a `BUILD_ASSERT` checks the total framebuffer allocation requirement.

| Mode | Operation | Advantages | Trade-offs |
| --- | --- | --- | --- |
| **1 FB** | LVGL writes into the same framebuffer used for scanout. | Minimum PSRAM footprint; no cross-buffer synchronization. | Visible tearing is possible. |
| **2 FB** | LVGL writes BACK; scanout reads FRONT. A completed BACK swaps with FRONT at the prefetch boundary. | Tear-free presentation under the tested workload; lower memory use than 3 FB. | Synchronization blocks further drawing while BACK is updated. |
| **3 FB** | Distinct FRONT, READY, and DRAW roles. Finished DRAW is published as READY. | Rendering can proceed independently of an already available READY frame; newest-ready/mailbox semantics. | Greater PSRAM use and extra synchronization traffic. |

### 2 FB: front/back + workqueue

- Each `display_write()` records a dirty region in BACK.
- The final flush (`frame_incomplete == false`) marks the frame ready.
- At a valid prefetch boundary, FRONT and BACK indices swap.
- The accumulated dirty regions are copied from new FRONT to new BACK by a Zephyr work item.
- `sync_pending` prevents drawing until the inactive buffer reflects the displayed frame.

The copy runs in a **workqueue**, not in the DMA EOF ISR.

### 3 FB: mailbox presentation

- LVGL draws into DRAW; on its final flush the completed image becomes READY.
- If an older READY image is still waiting, it is replaced: **mailbox semantics** favor the newest completed frame.
- The recycled buffer becomes the next DRAW.
- Each framebuffer has an accumulated `stale[]` dirty-region list, tracking modifications that buffer has missed.
- The recycled DRAW is brought up to date by copying the necessary regions from the latest completed frame; `sync_pending` prevents writes until this finishes.
- At a valid prefetch boundary, READY becomes FRONT.

When diagnostics are enabled, `drops` counts **replaced READY frames** in 3 FB mode. This does not indicate GDMA data loss.

### Concurrency rules

- The interrupt handlers must **never sleep**.
- `buffer_lock` serializes index/state updates between the write context and the scanout callback.
- `sync_pending` prevents drawing on a framebuffer while it is being synchronized.
- In 2 FB mode, a work item handles the copy; in 3 FB mode, synchronization runs in thread context following frame publication.
- A buffer used as FRONT is not modified by the multi-buffer synchronization process.

## 5. Dirty region tracking

The driver records **up to 16 independent dirty rectangles** for each synchronization list (`LCD_MAX_DIRTY_REGIONS`). Coordinates use an **exclusive** right/bottom bound: `[x1, x2) × [y1, y2)`.

- Rectangles that overlap or touch are merged.
- Independent regions remain separate instead of becoming one enormous bounding rectangle.
- If the region list exceeds its capacity, `full_refresh` requests a safe full-frame synchronization rather than losing updates.
- Copies operate row by row using CPU `memcpy()` between PSRAM framebuffers. The current synchronization routine does **not** explicitly flush each destination range.
- Dirty region lists are used only for 2 FB and 3 FB; no cross-framebuffer copy is needed with 1 FB.

**Why this matters:** a small animated object on the left and changing text on the right can span most of the screen if represented by a single bounding box. Keeping separate regions reduces synchronization bytes substantially.

## 6. Display API behavior

The driver registers a Zephyr `DEVICE_API(display, ...)` implementing:

- `write`: copies incoming RGB565X pixels to the drawing framebuffer, applying the configured software rotation; uses CPU `memcpy()` for normal orientation;
- `get_capabilities`: reports resolution, format and orientation;
- `set_pixel_format`: accepts only `PIXEL_FORMAT_RGB_565X`;
- `set_orientation`: runtime orientation changes are not supported; orientation is selected at build time.

For each write, the implementation validates destination bounds, pitch and source-buffer size. `desc->frame_incomplete` indicates whether additional flushes belong to the same rendered frame. A complete frame is published **only after the final flush**.

`lcd_drawing_buffer_lock()` waits in thread context for an available DRAW/BACK with a **1,000 ms timeout**; a timeout returns `-ETIMEDOUT` and emits an error log.

### Display orientation

The driver supports four fixed display orientations selected at build time through Kconfig. Let `W` and `H` be the physical panel dimensions read from devicetree (`LCD_HACTIVE` and `LCD_VACTIVE`).

| Rotation | Logical resolution | Kconfig |
| --- | --- | --- |
| 0° | W × H | `CONFIG_ESP32_LCD_RGB_ROTATION_0=y` |
| 90° | H × W | `CONFIG_ESP32_LCD_RGB_ROTATION_90=y` |
| 180° | W × H | `CONFIG_ESP32_LCD_RGB_ROTATION_180=y` |
| 270° | H × W | `CONFIG_ESP32_LCD_RGB_ROTATION_270=y` |

Rotation is performed in software during `display_write()`. The **physical framebuffer geometry remains W × H**, and the physical LCD_CAM/GDMA scanout timing is unchanged by logical orientation.

For 90° and 270°, the driver swaps the logical display dimensions reported through `get_capabilities()`. Dirty regions are tracked using physical framebuffer coordinates, ensuring correct synchronization in double- and triple-buffering modes.

Display orientation is fixed at initialization; runtime rotation is not supported. All four orientations, LVGL rendering, and GT911 touch-coordinate mapping were validated during development on the **800 × 480** board. The corresponding full rotation/touch regression on the **480 × 272** board remains to be completed.

## 7. Build configuration

The implementation was developed with Zephyr **4.4** and an ESP32-S3 board configuration enabling external PSRAM, LCD_CAM RGB pinctrl, and GDMA TX. Board-specific setup is maintained separately from this driver README.

Relevant Kconfig entries (project configuration):

```ini
# Required project/SoC configuration must enable LCD_CAM, GDMA and PSRAM.
# Select the driver's framebuffer count (1, 2 or 3):
CONFIG_ESP32_LCD_RGB_FRAMEBUFFER_COUNT=3

# Fixed display orientation (choose one):
CONFIG_ESP32_LCD_RGB_ROTATION_0=y

# Optional driver diagnostics:
CONFIG_LOG=y
CONFIG_LOG_LCD_CAM_DIAGNOSTICS=y
```

The project's Kconfig defines `LOG_LCD_CAM_DIAGNOSTICS` as a boolean. The driver maps it to its compile switch:

```c
#define LOG_PRINT (IS_ENABLED(CONFIG_LOG_LCD_CAM_DIAGNOSTICS))
```

Set `CONFIG_LOG_LCD_CAM_DIAGNOSTICS=n` to compile out optional scanout counters and the bounce-fill timing measurement. The per-write and framebuffer-sync timing aggregators used during development have been removed from the cleaned driver. **Error logs remain subject to the standard Zephyr logging configuration**. Do not confuse this optional switch with `CONFIG_DISPLAY_LOG_LEVEL`: logging must still be enabled at an appropriate level for INFO records to appear.

> **Integration note:** These Kconfig and devicetree properties describe the interface expected by the driver. Exact board-specific pinctrl, panel timings, PSRAM setup, and input/touch configuration are intentionally not reproduced here. There is no universally valid board overlay.

## 8. Diagnostics

When `CONFIG_LOG_LCD_CAM_DIAGNOSTICS=y`, the driver reports a periodic `LCD:` status line with the following fields. Counters other than `fill_max` are cumulative since startup:

| Output | Meaning |
| --- | --- |
| `fill_max` | Maximum observed PSRAM-to-SRAM bounce fill time since prior periodic report. |
| `EOF` | GDMA completed bounce-buffer groups. |
| `VSYNC` | LCD_CAM VSYNC events. |
| `restarts` | VSYNC-triggered recovery restarts. |
| `errors` | Nonzero GDMA callback status events. |
| `swaps` | Completed FRONT buffer presentations (not an LVGL rendering counter). |
| `defer` | In 2 FB mode: attempted presentation deferred due to pending synchronization or unavailable lock. In 3 FB mode: presentation attempt deferred due to an unavailable lock. |
| `drops` | READY frame replacements in mailbox mode. |
| `sync_err` | Failed 2 FB work-item submissions (not used in 1/3 FB modes). |

The periodic status line is emitted from `display_write()` once the configured reporting interval (currently 1,000 ms) has elapsed; it is **not a hardware timer callback**. If the application stops issuing writes, periodic diagnostic lines also stop. `fill_max` is reset after each report; the other counters are not reset.

The **swaps-per-second** figure inferred from consecutive `swaps` samples is the **presentation rate**, not necessarily the LVGL render-loop rate or the panel's physical refresh rate.

## 9. Measured results (historical development tests)

Measurements below were taken on the **800 × 480 VIEWE board** using LVGL 9.6 with RGB565X framebuffers. They are **historical development measurements**, collected before removal of the detailed `LCD write` / `FB sync` profiling code and before the shared 8-line / 1,280-byte DMA configuration was introduced. They are representative observations, **not** a controlled benchmark with identical scheduling, measurement windows or instrument settings, and **not** performance results for the 480 × 272 board.

| Scenario | 2 FB | 3 FB |
| --- | ---: | ---: |
| Morph Ball animation — presentations/s | ~26.9 | ~29.0 |
| Stress test — presentations/s | ~17.5 | ~16.4 |
| Morph Ball — average FB sync | ~2.15 ms | ~4.10 ms |
| Stress test — average FB sync | ~6.56 ms | ~8.76 ms |
| Stress test — average sync bytes | ~40.6 KB | ~46.9 KB |

The stress test uses **independent moving/updated screen regions**: an animated ball, a vertically animated box, a color-changing box, and a periodically updated text counter. These deliberately challenge partial-frame synchronization.

Before the dirty-region-list improvement, merging distant invalidated regions into one bounding rectangle caused observed synchronization of approximately **632 KB per operation** and about **7.3 presentations/s** in the 3 FB stress test. Independent dirty rectangles reduced the typical copy to roughly **47 KB** and improved the observed rate to approximately **16.4 presentations/s** under the updated workload.

No GDMA errors, recovery restarts or synchronization errors appeared in the cited steady-state runs. The 1 FB mode was known to exhibit tearing and is not included in the tear-free comparison.

## 10. Constraints and maintenance notes

1. **Hardware-specific design.** The ESP32-S3 LCD_CAM and GDMA register-level sequence is not a generic Zephyr RGB display abstraction.
2. **RGB565X only.** Other pixel formats are not implemented. Software rotation supports 0°, 90°, 180°, and 270°, selected at build time; runtime orientation changes are not supported.
3. **Shared, constrained DMA geometry.** The current 8-line bounce buffers, 1,280-byte descriptor payload, and 16-bit RGB data path work for the two validated panel geometries. Compile-time assertions enforce alignment and divisibility; different resolutions may require adjusting the common layout and verifying EOF/prefetch behavior.
4. **PSRAM bandwidth matters.** Both scanout refill and synchronization compete for external-memory bandwidth. More framebuffers do not guarantee higher FPS.
5. **Cache and DMA memory ownership.** On initialization, framebuffer memory is zeroed and explicitly flushed; DMA descriptors are also flushed after setup. Runtime framebuffer writes use CPU `memcpy()` for normal orientation and software pixel mapping for rotated orientations. Dirty-region synchronization uses CPU `memcpy()` between PSRAM framebuffers, followed by CPU copies from FRONT to internal-SRAM bounce buffers. Revisit coherence requirements if the memory-access path or CPU-core assignment changes.
6. **Interrupt latency matters.** `lcd_bounce_fill()` executes in the DMA callback path, so avoid expensive diagnostics or blocking operations there.
7. **Recovery remains a fallback.** A rising `restarts` count during normal running should be investigated rather than accepted as normal behavior.
8. **Diagnostic overhead.** Compare optimized builds with diagnostics disabled before making production performance claims. The cleaned driver retains lightweight scanout counters, not the detailed per-copy profiling used in earlier tests.
9. **Touch is a separate subsystem.** GT911, where present, is integrated via Zephyr input and LVGL; it is not managed by this LCD_CAM display driver. Polling/interrupt selection and touch latency are outside this driver's responsibilities.

## 11. Suggested regression checklist

After modifications to buffer ownership, synchronization, DMA descriptors, panel geometry, or VSYNC handling:

- Build **1 FB, 2 FB and 3 FB** with diagnostics on and off for each target panel.
- Verify display initialization, physical dimensions obtained from DTS, RGB timings, and expected RGB565X colors.
- Run the moving-ball test and the multi-region stress test for each applicable mode, using layouts appropriate for the display resolution.
- Check for tearing in 2/3 FB modes, dropped/duplicated visual regions, and incorrect final-frame content.
- Confirm that `errors`, `restarts` and `sync_err` remain at zero during steady state.
- With diagnostics enabled, observe `fill_max` under full animation load and compare with bounce transfer timing headroom.
- Exercise many independent dirty rectangles to verify overflow/full-refresh fallback.
- Test repeated initialization where applicable and inspect failure cleanup paths.
- Verify all four display orientations (0°, 90°, 180°, 270°), including logical resolution and framebuffer synchronization.
- Where a touchscreen is present, verify press/release behavior and coordinate mapping after rotation. **Touch validation is an integration test**, not a function of this display driver.
- Re-run the above on both **800 × 480** and **480 × 272** after any shared DMA or presentation-path change to catch regressions.

## 12. Main implementation entry points

| Function | Responsibility |
| --- | --- |
| `esp32_lcd_cam_rgb_init()` | Initialize display state, LCD_CAM, PSRAM buffers, GDMA, VSYNC IRQ and scanout. |
| `lcd_dma_ring_init()` | Build the continuous descriptor ring and prefill bounce buffers. |
| `lcd_dma_callback()` | Refill freed bounce buffers and invoke prefetch-time presentation. |
| `lcd_vsync_isr()` | Handle frame boundary bookkeeping and recovery on incomplete DMA progress. |
| `lcd_present_ready_frame()` | Select new FRONT at the safe boundary, according to FB mode. |
| `esp32_lcd_cam_rgb_write()` | Validate and copy a flush, record dirty region, publish completed frame. |
| `lcd_dirty_region_list_add()` | Merge or append an invalidated region; detect overflow. |
| `lcd_dirty_region_list_copy()` | Synchronize only required rows/rectangles. |
| `lcd_framebuffer_sync_work_handler()` | Perform 2 FB background synchronization. |
| `lcd_triple_framebuffer_publish()` | Publish READY and choose the next DRAW. |
| `lcd_triple_framebuffer_sync()` | Synchronize recycled DRAW in 3 FB mode. |

---

**Development status (26 October 2026):** Continuous RGB scanout is operational on both the VIEWE 800 × 480 and 480 × 272 boards using the **same driver source**. LVGL animations, stress workloads, four software display orientations, and Zephyr input/GT911 integration were previously validated on the 800 × 480 target. Bring-up on 480 × 272 is successful; the complete multi-buffer, rotation, touchscreen, and stress-test regression matrix on that target remains to be documented. Portability to additional panels and production qualification require target-specific validation.
