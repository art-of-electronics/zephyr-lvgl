# ESP32-S3 LCD_CAM RGB display driver (Zephyr)

Custom Zephyr display driver for the **ESP32-S3 LCD_CAM** peripheral operating in parallel RGB mode. Developed and tested on the **VIEWE UEDX80480050E-WB-A** display board (ESP32-S3-WROOM-1 N16R8, 800 × 480 RGB panel).

The driver implements continuous RGB scanout using a circular **GDMA descriptor ring**, two internal-SRAM **bounce buffers**, and **1, 2, or 3 framebuffers in external PSRAM**. Double- and triple-buffering synchronize framebuffer presentation with the scanout prefetch boundary and use per-region synchronization to avoid unnecessary PSRAM traffic.

> **Scope:** This README documents the current implementation, not a generic ESP32-S3 RGB controller. Clock, DMA sizing, bounce geometry, and synchronization have been validated for the board above. Other panels require additional validation.

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
        +-------------------------+
        | SRAM bounce 0 (10 lines)| <---+
        | SRAM bounce 1 (10 lines)| <---+ alternating refills
        +-------------------------+
                    |
           GDMA descriptor ring
            (circular, 10 nodes)
                    |
            LCD_CAM RGB output
                    |
             800 x 480 panel
```

The **LCD timing engine runs continuously**. GDMA consumes descriptor-backed SRAM data while the GDMA EOF callback refills the freed bounce buffer from the currently selected front framebuffer. The code contains a VSYNC recovery path for underruns; this is not a normal per-frame restart.

**Memory at the tested resolution:**

| Resource | Geometry | Size |
| --- | --- | ---: |
| One PSRAM framebuffer | 800 × 480 × 2 bytes | 768,000 B (750 KiB) |
| 1 / 2 / 3 framebuffers | RGB565X | 750 / 1,500 / 2,250 KiB |
| One SRAM bounce buffer | 800 × 10 × 2 bytes | 16,000 B |
| Both SRAM bounce buffers | 2 × 16,000 bytes | 32,000 B |
| One DMA descriptor payload | 3,200 bytes | 3,200 B |
| GDMA descriptor ring | 5 descriptors per bounce × 2 | 10 descriptors |

Descriptor bookkeeping, synchronization state, work items (2 FB), and optional diagnostics require additional memory. Framebuffers are allocated from the external-memory shared heap with **32-byte alignment**.

## 2. RGB and clock configuration

The driver gets panel dimensions and RGB timing parameters from devicetree:

- Compatible: `espressif,esp32-lcd-cam-rgb` (C macro `espressif_esp32_lcd_cam_rgb`).
- LCD_CAM hardware node: `DT_NODELABEL(lcd_cam)`.
- Display width/height and `display_timings` child: `clock-frequency`, horizontal/vertical sync length, back porch, and front porch.
- Pin assignments: board pinctrl for LCD_CAM.
- GDMA TX controller and channel: `dmas` entry named `tx` on the LCD_CAM node.

The driver uses **16-bit RGB data** and exposes the Zephyr display format **`PIXEL_FORMAT_RGB_565X`** with normal orientation only. Pixel-clock source frequency is read from the ESP clock tree, divided by `LCD_PERIPH_CLOCK_PRE_SCALE` (currently 2), then the pixel divider is chosen with round-up division. The resulting clock may be below the requested devicetree value.

For the tested configuration the requested pixel clock is **20 MHz** and the panel's physical scanout is approximately **46 Hz**. This scanout rate is separate from the observed application presentation rate; the latter depends on LVGL activity and synchronization overhead.

## 3. GDMA scanout and VSYNC boundary

Constants in the current driver:

```c
#define LCD_BOUNCE_LINES            (10U)
#define LCD_BOUNCE_COUNT            (2U)
#define LCD_DMA_DESC_SIZE           (3200U)
#define LCD_EXPECTED_EOF_PER_FRAME  (LCD_VACTIVE / LCD_BOUNCE_LINES)
#define LCD_PRESENT_PREFETCH_EOF    (LCD_EXPECTED_EOF_PER_FRAME - LCD_BOUNCE_COUNT)
```

At 480 lines, the driver expects **48 bounce-buffer EOF events per complete frame**. Each bounce buffer spans 10 lines, so the two buffers carry 20 lines in flight. The GDMA ring is circular, and each descriptor is owned by DMA.

The callback sequence is:

1. GDMA reports completion of a bounce-buffer group (`suc_eof` on the final descriptor for that group).
2. `lcd_dma_callback()` refills the released bounce buffer with the next 10 lines from FRONT.
3. At `LCD_PRESENT_PREFETCH_EOF` (46 in the tested configuration), the driver considers a change of FRONT for the next frame.
4. `lcd_vsync_isr()` maintains line/EOF bookkeeping and checks whether a recovery restart is needed.

The **prefetch boundary**, rather than an arbitrary write-completion timestamp, determines when FRONT may switch. This is essential because the DMA pipeline already contains data for upcoming display lines.

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

- `write`: copies incoming RGB565X pixels to the drawing framebuffer using CPU `memcpy()`;
- `get_capabilities`: reports resolution, format and orientation;
- `set_pixel_format`: accepts only `PIXEL_FORMAT_RGB_565X`;
- `set_orientation`: accepts only `DISPLAY_ORIENTATION_NORMAL`.

For each write, the implementation validates destination bounds, pitch and source-buffer size. `desc->frame_incomplete` indicates whether additional flushes belong to the same rendered frame. A complete frame is published **only after the final flush**.

`lcd_drawing_buffer_lock()` waits in thread context for an available DRAW/BACK with a **1,000 ms timeout**; a timeout returns `-ETIMEDOUT` and emits an error log.

## 7. Build configuration

The tested project uses Zephyr **4.4** and the board definition under `boards/viewe/uedx80480050e_wb_a/`, with external PSRAM enabled, LCD_CAM RGB pinctrl and GDMA TX configured in devicetree. The `boards/viewe/` directory groups VIEWE board definitions; it is not a driver requirement.

Relevant Kconfig entries (project configuration):

```ini
# Required project/SoC configuration must enable LCD_CAM, GDMA and PSRAM.
# Set framebuffer count using the driver's existing Kconfig symbol:
CONFIG_ESP32_LCD_RGB_FRAMEBUFFER_COUNT=3

# Optional driver diagnostics (the project's Kconfig symbol):
CONFIG_LOG=y
CONFIG_LOG_LCD_CAM_DIAGNOSTICS=y
```

The project's Kconfig defines `LOG_LCD_CAM_DIAGNOSTICS` as a boolean. The driver maps it to its compile switch:

```c
#define LOG_PRINT (IS_ENABLED(CONFIG_LOG_LCD_CAM_DIAGNOSTICS))
```

Set `CONFIG_LOG_LCD_CAM_DIAGNOSTICS=n` to compile out optional scanout counters and the bounce-fill timing measurement. The per-write and framebuffer-sync timing aggregators used during development have been removed from the cleaned driver. **Error logs remain subject to the standard Zephyr logging configuration**. Do not confuse this optional switch with `CONFIG_DISPLAY_LOG_LEVEL`: logging must still be enabled at an appropriate level for INFO records to appear.

> **Integration note:** The Kconfig and devicetree fragments above describe the symbols used by this driver. The precise board-specific pinctrl/clock/PSRAM setup belongs to the board definition and is not reproduced here; no universally valid board overlay is implied.

## 8. Diagnostics

When `CONFIG_LOG_LCD_CAM_DIAGNOSTICS=y`, the **cleaned driver** reports a periodic `LCD:` status line with the following fields. Counters other than `fill_max` are cumulative since startup:

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

The *swaps-per-second* figure inferred from consecutive `swaps` samples is the **presentation rate**, not necessarily the LVGL render-loop rate or the panel's physical refresh rate.

## 9. Measured results (development tests)

Measurements below were taken on the 800 × 480 VIEWE board using LVGL 9.6 with RGB565X framebuffers. They are **historical development measurements**, collected before removal of the detailed `LCD write` / `FB sync` profiling code. They are representative observations, **not** a controlled benchmark with identical scheduling, measurement windows or instrument settings.

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
2. **RGB565X only.** Other formats and display rotations are not implemented.
3. **Fixed bounce/descriptor geometry.** The 10-line, 3,200-byte layout and 16-bit data path are explicitly asserted at build time; changes require recalculating EOF and prefetch behavior.
4. **PSRAM bandwidth matters.** Both scanout refill and synchronization compete for external-memory bandwidth. More framebuffers do not guarantee higher FPS.
5. **Cache and DMA memory ownership.** On initialization, framebuffer memory is zeroed and explicitly flushed; DMA descriptors are also flushed after setup. Runtime framebuffer writes and dirty-region synchronization use CPU `memcpy()` into PSRAM, followed by CPU copy from FRONT to internal-SRAM bounce buffers. The current driver does **not** explicitly flush PSRAM destination ranges after every runtime copy. Revisit coherence requirements if the memory-access path or CPU-core assignment changes.
6. **Interrupt latency matters.** `lcd_bounce_fill()` executes in the DMA callback path, so avoid expensive diagnostics or blocking operations there.
7. **Recovery remains a fallback.** A rising `restarts` count during normal running should be investigated rather than accepted as normal behavior.
8. **Diagnostic overhead.** Compare optimized builds with diagnostics disabled before making production performance claims. The cleaned driver retains lightweight scanout counters, not the detailed per-copy profiling used in earlier tests.
9. **Touch is a separate subsystem.** GT911 is integrated via Zephyr input and LVGL; it is not managed by this LCD_CAM display driver. Polling/interrupt selection and touch latency are outside this driver's responsibilities.

## 11. Suggested regression checklist

After modifications to buffer ownership, synchronization, DMA descriptors, or VSYNC handling:

- Build **1 FB, 2 FB and 3 FB** with diagnostics on and off.
- Verify screen initialization and the expected RGB565X colors.
- Run the moving-ball test and the multi-region stress test for each applicable mode.
- Check for tearing in 2/3 FB modes, dropped/duplicated visual regions, and incorrect final-frame content.
- Confirm that `errors`, `restarts` and `sync_err` remain at zero during steady state.
- With diagnostics enabled, observe `fill_max` under full animation load and compare with bounce transfer timing headroom.
- Exercise many independent dirty rectangles to verify overflow/full-refresh fallback.
- Test repeated initialization where applicable and inspect failure cleanup paths.
- For the LVGL + GT911 milestone, verify press/release behavior and visual updates with the cleaned driver; this is an **integration test**, not a function of the LCD_CAM driver.

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

**Development status (26 October 2026):** tested on physical hardware with LVGL animations, stress workloads and Zephyr input/GT911 interaction. The LVGL + touch integration milestone is functionally complete, pending the final build and on-board smoke test with the cleaned driver. Portability and production qualification require additional target-specific validation.
