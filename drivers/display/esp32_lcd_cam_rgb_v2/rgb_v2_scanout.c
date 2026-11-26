/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/cache.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/multi_heap/shared_multi_heap.h>
#include <zephyr/sys/util.h>

#include <esp_clk_tree.h>
#include <esp_intr_alloc.h>
#include <soc/interrupts.h>
#include <soc/gdma_struct.h>
#include <hal/gdma_channel.h>
#include <hal/gdma_ll.h>
#include <hal/lcd_ll.h>

#include "rgb_v2_scanout.h"


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
#define RGB_V2_LCD_CAM_NODE      DT_NODELABEL(lcd_cam)
#define RGB_V2_DMA_DEVICE        DEVICE_DT_GET(DT_DMAS_CTLR_BY_NAME(RGB_V2_LCD_CAM_NODE, tx))
#define RGB_V2_DMA_CHANNEL       DT_DMAS_CELL_BY_NAME(RGB_V2_LCD_CAM_NODE, tx, channel)
#define RGB_V2_DMA_SLOT          SOC_GDMA_TRIG_PERIPH_LCD0

#define RGB_V2_LCD_BUS          (0U)
#define RGB_V2_PIXEL_BITS       (16U)
#define RGB_V2_CLOCK_PRESCALE   (2U)
#define RGB_V2_CACHE_ALIGN      (64U)
#define RGB_V2_FIFO_SKIP_BYTES  ((LCD_LL_FIFO_DEPTH + 1U) * sizeof(uint16_t))
#define RGB_V2_STATS_PERIOD_MS  (1000U)
#define RGB_V2_WRITE_WAIT_MS    (200U)
#define RGB_V2_WRITE_RETRY_MS   (1U)


PINCTRL_DT_DEFINE(RGB_V2_LCD_CAM_NODE);
LOG_MODULE_REGISTER(rgb_v2_scanout, CONFIG_DISPLAY_LOG_LEVEL);

BUILD_ASSERT((0U == (RGB_V2_HEIGHT % (RGB_V2_BOUNCE_COUNT * RGB_V2_BOUNCE_LINES))),
             "Frame height must contain a complete number of bounce pairs");
BUILD_ASSERT((0U == (RGB_V2_BOUNCE_BYTES % RGB_V2_DESC_BYTES)),
             "The bounce buffer must contain complete GDMA descriptors");
BUILD_ASSERT((0U == (RGB_V2_DESC_BYTES % RGB_V2_CACHE_ALIGN)),
             "Descriptor payload must be 64-byte aligned");
BUILD_ASSERT((0U == (RGB_V2_FB_BYTES % RGB_V2_CACHE_ALIGN)),
             "PSRAM buffer must have a whole number of cache lines");
BUILD_ASSERT((RGB_V2_DESC_BYTES <= 4095U), "GDMA descriptor payload is 12 bits");
BUILD_ASSERT((RGB_V2_FIFO_SKIP_BYTES < RGB_V2_DESC_BYTES),
             "LCD FIFO skip must fit inside the first descriptor");
BUILD_ASSERT((2U == RGB_V2_BOUNCE_COUNT), "This scanout implementation uses two bounce buffers");


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/

/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static int rgb_v2_init_pins(void)
{
    const struct pinctrl_dev_config *pins = PINCTRL_DT_DEV_CONFIG_GET(RGB_V2_LCD_CAM_NODE);
    return pinctrl_apply_state(pins, PINCTRL_STATE_DEFAULT);
}

static int rgb_v2_init_clocks(struct rgb_v2_scanout *scanout)
{
    const soc_periph_lcd_clk_src_t source = LCD_CLK_SRC_DEFAULT;
    uint32_t source_hz = 0U;
    uint32_t peripheral_hz = 0U;
    uint32_t divider = 0U;
    int result = 0;

    result = esp_clk_tree_src_get_freq_hz(source, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &source_hz);
    if ((ESP_OK != result) || (0U == RGB_V2_PCLK)) {
        return -EINVAL;
    }

    peripheral_hz = source_hz / RGB_V2_CLOCK_PRESCALE;
    divider = DIV_ROUND_UP(peripheral_hz, RGB_V2_PCLK);
    if ((0U == divider) || (divider > LCD_LL_PCLK_DIV_MAX)) {
        return -EINVAL;
    }

    lcd_ll_select_clk_src(scanout->hal.dev, source);
    lcd_ll_set_group_clock_coeff(scanout->hal.dev, RGB_V2_CLOCK_PRESCALE, 0, 0);
    lcd_ll_set_pixel_clock_prescale(scanout->hal.dev, divider);

    LOG_INF("Clock source=%u Hz, LCD=%u Hz, PCLK=%u Hz",
            (unsigned int)source_hz, (unsigned int)peripheral_hz,
            (unsigned int)(peripheral_hz / divider));
    return 0;
}

static void rgb_v2_init_timings(struct rgb_v2_scanout *scanout)
{
    lcd_cam_dev_t *lcd = scanout->hal.dev;

    lcd_ll_set_dma_read_stride(lcd, RGB_V2_PIXEL_BITS);
    lcd_ll_set_clock_idle_level(lcd, false);
    lcd_ll_set_pixel_clock_edge(lcd, true);
    lcd_ll_enable_rgb_mode(lcd, true);
    lcd_ll_enable_color_convert(lcd, false);
    lcd_ll_set_phase_cycles(lcd, 0, 0, 1);
    lcd_ll_enable_output_always_on(lcd, true);
    lcd_ll_set_idle_level(lcd, true, true, false);
    lcd_ll_set_blank_cycles(lcd, 1, 1);
    lcd_ll_set_horizontal_timing(lcd, DT_PROP(RGB_V2_DT_TIMINGS, hsync_len),
                                 DT_PROP(RGB_V2_DT_TIMINGS, hback_porch),
                                 RGB_V2_WIDTH, DT_PROP(RGB_V2_DT_TIMINGS, hfront_porch));
    lcd_ll_set_vertical_timing(lcd, DT_PROP(RGB_V2_DT_TIMINGS, vsync_len),
                               DT_PROP(RGB_V2_DT_TIMINGS, vback_porch),
                               RGB_V2_HEIGHT, DT_PROP(RGB_V2_DT_TIMINGS, vfront_porch));
    lcd_ll_enable_output_hsync_in_porch_region(lcd, true);
    lcd_ll_set_hsync_position(lcd, 0);
    lcd_ll_enable_auto_next_frame(lcd, true);
    lcd_ll_set_delay_ticks(lcd, 0, 0, 0);
    lcd_ll_set_data_delay_ticks(lcd, 0);
}

static int rgb_v2_init_lcd(struct rgb_v2_scanout *scanout)
{
    int result = 0;

    lcd_ll_enable_bus_clock(RGB_V2_LCD_BUS, true);
    lcd_ll_reset_register(RGB_V2_LCD_BUS);
    scanout->hal.dev = LCD_LL_GET_HW(RGB_V2_LCD_BUS);
    lcd_ll_enable_output_always_on(scanout->hal.dev, false);

    result = rgb_v2_init_clocks(scanout);
    if (0 != result) {
        return result;
    }

    lcd_ll_reset(scanout->hal.dev);
    lcd_ll_fifo_reset(scanout->hal.dev);
    rgb_v2_init_timings(scanout);
    return 0;
}

static void rgb_v2_free_frames(struct rgb_v2_scanout *scanout)
{
    uint8_t index = 0U;

    for (index = 0U; index < RGB_V2_FRAME_COUNT; ++index) {
        if (NULL != scanout->frame_psram[index]) {
            shared_multi_heap_free(scanout->frame_psram[index]);
            scanout->frame_psram[index] = NULL;
        }
    }
}

static int rgb_v2_init_frames(struct rgb_v2_scanout *scanout)
{
    uint8_t index = 0U;

    for (index = 0U; index < RGB_V2_FRAME_COUNT; ++index) {
        scanout->frame_psram[index] = shared_multi_heap_aligned_alloc(SMH_REG_ATTR_EXTERNAL, 
                                                                      RGB_V2_CACHE_ALIGN, RGB_V2_FB_BYTES);
        if (NULL == scanout->frame_psram[index]) {
            rgb_v2_free_frames(scanout);
            return -ENOMEM;
        }
        if (0U != ((uintptr_t)scanout->frame_psram[index] % RGB_V2_CACHE_ALIGN)) {
            rgb_v2_free_frames(scanout);
            return -EINVAL;
        }

        memset(scanout->frame_psram[index], 0, RGB_V2_FB_BYTES);

        if (0U == index) {
            int result = sys_cache_data_flush_range(scanout->frame_psram[index], RGB_V2_FB_BYTES);
            if (0 != result) {
                rgb_v2_free_frames(scanout);
                return result;
            }
        }
    }
    return 0;
}

static void rgb_v2_copy_lines(struct rgb_v2_scanout *scanout, uint8_t index)
{
    const size_t offset = (size_t)scanout->next_line * RGB_V2_WIDTH;
    const uint8_t active = rgb_v2_frames_active(&scanout->frames);
    const uint32_t start_cycles = k_cycle_get_32();
    uint32_t cycles = 0U;

    memcpy(scanout->bounce[index], &scanout->frame_psram[active][offset], RGB_V2_BOUNCE_BYTES);
    scanout->next_line = (scanout->next_line + RGB_V2_BOUNCE_LINES) % RGB_V2_HEIGHT;
    
    cycles = k_cycle_get_32() - start_cycles;
    if (cycles > scanout->refill_max_cycles) {
        scanout->refill_max_cycles = cycles;
    }
}

static int rgb_v2_prime_locked(struct rgb_v2_scanout *scanout)
{
    int result = 0;

    rgb_v2_bounce_reset(&scanout->ownership);
    scanout->next_line = 0U;
    scanout->eof_in_frame = 0U;

    rgb_v2_copy_lines(scanout, 0U);
    result = rgb_v2_bounce_mark_ready(&scanout->ownership, 0U);
    if (0 != result) {
        return result;
    }

    rgb_v2_copy_lines(scanout, 1U);
    result = rgb_v2_bounce_mark_ready(&scanout->ownership, 1U);
    if (0 != result) {
        return result;
    }
    
    return rgb_v2_bounce_start(&scanout->ownership, 0U);
}

static void rgb_v2_dma_callback(const struct device *device, void *user_data,
                                uint32_t channel, int status)
{
    struct rgb_v2_scanout *scanout = user_data;
    struct rgb_v2_refill_token token = {0};
    k_spinlock_key_t key;
    uint8_t completed = 0U;
    int result = 0;
    int latch_result = 0;

    ARG_UNUSED(device);
    ARG_UNUSED(channel);

    key = k_spin_lock(&scanout->state_lock);
    if (0 != status) {
        scanout->dma_errors++;
        scanout->recovery_needed = true;
        k_spin_unlock(&scanout->state_lock, key);
        return;
    }

    if (true == scanout->recovery_needed) {
        k_spin_unlock(&scanout->state_lock, key);
        return;
    }
    completed = (uint8_t)(scanout->eof_in_frame % RGB_V2_BOUNCE_COUNT);

    result = rgb_v2_bounce_eof(&scanout->ownership, completed);
    if (0 == result) {
        result = rgb_v2_bounce_refill_begin(&scanout->ownership, completed, &token);
    }
    if (0 == result) {
        /*
         * The first two bounces of frame N+1 are populated during the tail
         * of frame N. Latch here, BEFORE filling its line zero, not at VSYNC.
         * Switching at VSYNC would show 20 stale lines in the new frame.
         * The current frame's tail is already in the other DMA-owned bounce.
         */
        if (0U == scanout->next_line) {
            latch_result = rgb_v2_frames_latch_at_wrap(&scanout->frames);
            if ((0 != latch_result) && (-ENOENT != latch_result)) {
                result = latch_result;
            }
        }
        if (0 == result) {
            rgb_v2_copy_lines(scanout, completed);
            result = rgb_v2_bounce_refill_finish(&scanout->ownership, &token, true);
        }
    }
    if (0 != result) {
#if defined(CONFIG_ESP32_LCD_RGB_V2_STATS)
        scanout->pipeline_errors++;
        scanout->last_pipeline_error = result;
#endif
        scanout->recovery_needed = true;
    } else {
        scanout->eof_in_frame++;
        scanout->eof_total++;
    }

    k_spin_unlock(&scanout->state_lock, key);
}

static void rgb_v2_ring_init(struct rgb_v2_scanout *scanout)
{
    uint32_t desc = 0U;
    uint32_t index = 0U;
    uint32_t offset = 0U;

    memset(scanout->descriptors, 0, sizeof(scanout->descriptors));
    for (desc = 0U; desc < RGB_V2_DESC_COUNT; ++desc) {
        index = desc / RGB_V2_DESCS_PER_BOUNCE;
        offset = (desc % RGB_V2_DESCS_PER_BOUNCE) * RGB_V2_DESC_BYTES;
        scanout->descriptors[desc].buffer = &scanout->bounce[index][offset];
        scanout->descriptors[desc].dw0.size = RGB_V2_DESC_BYTES;
        scanout->descriptors[desc].dw0.length = RGB_V2_DESC_BYTES;
        scanout->descriptors[desc].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
        scanout->descriptors[desc].dw0.suc_eof = (0U == ((desc + 1U) % RGB_V2_DESCS_PER_BOUNCE));
        scanout->descriptors[desc].next = &scanout->descriptors[(desc + 1U) % RGB_V2_DESC_COUNT];
    }

    memset(&scanout->restart_descriptor, 0, sizeof(scanout->restart_descriptor));
    scanout->restart_descriptor.buffer = &scanout->bounce[0][RGB_V2_FIFO_SKIP_BYTES];
    scanout->restart_descriptor.dw0.size = RGB_V2_DESC_BYTES - RGB_V2_FIFO_SKIP_BYTES;
    scanout->restart_descriptor.dw0.length = RGB_V2_DESC_BYTES - RGB_V2_FIFO_SKIP_BYTES;
    scanout->restart_descriptor.dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    scanout->restart_descriptor.dw0.suc_eof = 0;
    scanout->restart_descriptor.next = &scanout->descriptors[1];

    (void)sys_cache_data_flush_range(scanout->descriptors, sizeof(scanout->descriptors));
    (void)sys_cache_data_flush_range(&scanout->restart_descriptor, sizeof(scanout->restart_descriptor));
}

static int rgb_v2_init_gdma(struct rgb_v2_scanout *scanout)
{
    int result = 0;

    scanout->dma_dev = RGB_V2_DMA_DEVICE;
    scanout->dma_channel = RGB_V2_DMA_CHANNEL;
    if (0U == (scanout->dma_channel & 1U)) {
        return -EINVAL;
    }

    if (false == device_is_ready(scanout->dma_dev)) {
        return -ENODEV;
    }

    memset(&scanout->setup_block, 0, sizeof(scanout->setup_block));
    memset(&scanout->dma_cfg, 0, sizeof(scanout->dma_cfg));
    scanout->setup_block.source_address = (uintptr_t)&scanout->bounce[0][0];
    scanout->setup_block.block_size = sizeof(uint32_t);
    scanout->dma_cfg.channel_direction = MEMORY_TO_PERIPHERAL;
    scanout->dma_cfg.channel_priority = GDMA_LL_CHANNEL_MAX_PRIORITY;
    scanout->dma_cfg.dma_slot = RGB_V2_DMA_SLOT;
    scanout->dma_cfg.block_count = 1U;
    scanout->dma_cfg.head_block = &scanout->setup_block;
    scanout->dma_cfg.dma_callback = rgb_v2_dma_callback;
    scanout->dma_cfg.user_data = scanout;

    result = dma_config(scanout->dma_dev, scanout->dma_channel, &scanout->dma_cfg);
    if (0 != result) {
        return result;
    }

    LOG_INF("GDMA TX Zephyr channel=%u / HW=%u; %u descriptors; %u-byte bounce",
            (unsigned int)scanout->dma_channel,
            (unsigned int)(scanout->dma_channel / 2U),
            (unsigned int)RGB_V2_DESC_COUNT, (unsigned int)RGB_V2_BOUNCE_BYTES);
    return 0;
}

static void rgb_v2_recover_locked(struct rgb_v2_scanout *scanout)
{
    const uint32_t hw = scanout->dma_channel / 2U;
    int result = 0;

    lcd_ll_fifo_reset(scanout->hal.dev);
    gdma_ll_tx_reset_channel(&GDMA, hw);
    gdma_ll_tx_clear_interrupt_status(&GDMA, hw, GDMA_LL_EVENT_TX_EOF);

    result = rgb_v2_prime_locked(scanout);
    if (0 != result) {
        scanout->recovery_needed = true;
        return;
    }

    gdma_ll_tx_set_desc_addr(&GDMA, hw, (uint32_t)(uintptr_t)&scanout->restart_descriptor);
    gdma_ll_tx_start(&GDMA, hw);
    scanout->recovery_needed = false;
    scanout->recoveries++;
}

static void rgb_v2_vsync_irq(void *argument)
{
    struct rgb_v2_scanout *scanout = argument;
    const uint32_t status = lcd_ll_get_interrupt_status(scanout->hal.dev);
    k_spinlock_key_t key;
    bool short_frame = false;
    bool pending_fault = false;

    lcd_ll_clear_interrupt_status(scanout->hal.dev, status);
    if (0U == (status & LCD_LL_EVENT_VSYNC_END)) {
        return;
    }

    key = k_spin_lock(&scanout->state_lock);
    scanout->vsync_total++;
    short_frame = (scanout->eof_in_frame < RGB_V2_EOF_PER_FRAME);
    pending_fault = (true == scanout->recovery_needed);
#if defined(CONFIG_ESP32_LCD_RGB_V2_STATS)
    scanout->last_vsync_eof = scanout->eof_in_frame;
    if (scanout->eof_in_frame > RGB_V2_EOF_PER_FRAME) {
        scanout->overfull_vsync_frames++;
    }
    if (true == short_frame) {
        scanout->short_vsync_recoveries++;
        scanout->last_short_eof = scanout->eof_in_frame;
        if ((1U == scanout->short_vsync_recoveries) ||
            (scanout->eof_in_frame < scanout->min_short_eof)) {
            scanout->min_short_eof = scanout->eof_in_frame;
        }
        if (scanout->eof_in_frame > scanout->max_short_eof) {
            scanout->max_short_eof = scanout->eof_in_frame;
        }
    }
    if (true == pending_fault) {
        scanout->pending_fault_recoveries++;
    }
#endif
    if ((true == pending_fault) || (true == short_frame)) {
        rgb_v2_recover_locked(scanout);
    }

    scanout->eof_in_frame = 0U;
    k_spin_unlock(&scanout->state_lock, key);
}

static int rgb_v2_init_vsync_irq(struct rgb_v2_scanout *scanout)
{
    int result = 0;

    result = esp_intr_alloc(ETS_LCD_CAM_INTR_SOURCE, ESP_INTR_FLAG_INTRDISABLED,
                            rgb_v2_vsync_irq, scanout, &scanout->vsync_irq);
    if (ESP_OK != result) {
        return -EIO;
    }

    lcd_ll_clear_interrupt_status(scanout->hal.dev, LCD_LL_EVENT_VSYNC_END);
    lcd_ll_enable_interrupt(scanout->hal.dev, LCD_LL_EVENT_VSYNC_END, true);

    result = esp_intr_enable(scanout->vsync_irq);
    if (ESP_OK != result) {
        lcd_ll_enable_interrupt(scanout->hal.dev, LCD_LL_EVENT_VSYNC_END, false);
        (void)esp_intr_free(scanout->vsync_irq);
        scanout->vsync_irq = NULL;
        return -EIO;
    }
    return 0;
}

static void rgb_v2_start(struct rgb_v2_scanout *scanout)
{
    const uint32_t hw = scanout->dma_channel / 2U;
    lcd_cam_dev_t *lcd = scanout->hal.dev;

    lcd_ll_stop(lcd);
    lcd_ll_reset(lcd);
    lcd_ll_fifo_reset(lcd);
    lcd_ll_clear_interrupt_status(lcd, LCD_LL_EVENT_VSYNC_END);

    gdma_ll_tx_clear_interrupt_status(&GDMA, hw, GDMA_LL_EVENT_TX_EOF);
    gdma_ll_tx_enable_interrupt(&GDMA, hw, GDMA_LL_EVENT_TX_EOF, true);
    gdma_ll_tx_enable_data_burst(&GDMA, hw, false);
    gdma_ll_tx_set_desc_addr(&GDMA, hw, (uint32_t)(uintptr_t)scanout->descriptors);
    gdma_ll_tx_start(&GDMA, hw);
    k_busy_wait(1);
    lcd_ll_start(lcd);
}

#if defined(CONFIG_ESP32_LCD_RGB_V2_STATS)
static void rgb_v2_stats_work(struct k_work *work)
{
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct rgb_v2_scanout *scanout = CONTAINER_OF(delayable, struct rgb_v2_scanout, stats_work);
    k_spinlock_key_t key;
    uint32_t eof = 0U;
    uint32_t vsync = 0U;
    uint32_t errors = 0U;
    uint32_t recoveries = 0U;
    uint32_t misses = 0U;
    uint32_t max_cycles = 0U;
    uint32_t requested = 0U;
    uint32_t latched = 0U;
    uint8_t active = 0U;
    uint8_t pending = 0U;
    uint8_t drawing = 0U;
    uint32_t acquisitions = 0U;
    uint32_t releases = 0U;
    uint32_t short_vsync = 0U;
    uint32_t pending_fault = 0U;
    uint32_t pipeline_errors = 0U;
    int32_t last_pipeline_error = 0;
    uint32_t last_vsync_eof = 0U;
    uint32_t overfull = 0U;
    uint32_t last_short_eof = 0U;
    uint32_t min_short_eof = 0U;
    uint32_t max_short_eof = 0U;

    key = k_spin_lock(&scanout->state_lock);
    eof = scanout->eof_total;
    vsync = scanout->vsync_total;
    errors = scanout->dma_errors;
    recoveries = scanout->recoveries;
    misses = scanout->ownership.deadline_misses;
    max_cycles = scanout->refill_max_cycles;
    scanout->refill_max_cycles = 0U;
    requested = scanout->frames.requests;
    latched = scanout->frames.latches;
    active = scanout->frames.active_index;
    pending = scanout->frames.pending_index;
    drawing = scanout->frames.drawing_index;
    acquisitions = scanout->frames.acquisitions;
    releases = scanout->frames.releases;
    short_vsync = scanout->short_vsync_recoveries;
    pending_fault = scanout->pending_fault_recoveries;
    pipeline_errors = scanout->pipeline_errors;
    last_pipeline_error = scanout->last_pipeline_error;
    last_vsync_eof = scanout->last_vsync_eof;
    overfull = scanout->overfull_vsync_frames;
    last_short_eof = scanout->last_short_eof;
    min_short_eof = scanout->min_short_eof;
    max_short_eof = scanout->max_short_eof;
    k_spin_unlock(&scanout->state_lock, key);

    LOG_INF("v2: EOF=%u VSYNC=%u recoveries=%u dma_err=%u misses=%u refill_max=%u us",
            (unsigned int)eof, (unsigned int)vsync, (unsigned int)recoveries,
            (unsigned int)errors, (unsigned int)misses,
            (unsigned int)k_cyc_to_us_floor32(max_cycles));
    LOG_INF("v2 FB: scan=%u queued=%d drawing=%d acquired=%u submitted=%u "
            "latched=%u released=%u",
            (unsigned int)active,
            (RGB_V2_FRAME_NO_PENDING == pending) ? (-1) : ((int)pending),
            (RGB_V2_FRAME_NO_PENDING == drawing) ? (-1) : ((int)drawing),
            (unsigned int)acquisitions, (unsigned int)requested,
            (unsigned int)latched, (unsigned int)releases);
    LOG_INF("v2 recovery: short=%u pending=%u pipeline=%u last_err=%d "
            "eof=%u/%u over=%u last_short=%u min=%u max=%u",
            (unsigned int)short_vsync, (unsigned int)pending_fault,
            (unsigned int)pipeline_errors, (int)last_pipeline_error,
            (unsigned int)last_vsync_eof, (unsigned int)RGB_V2_EOF_PER_FRAME,
            (unsigned int)overfull, (unsigned int)last_short_eof,
            (unsigned int)min_short_eof, (unsigned int)max_short_eof);
    (void)k_work_reschedule(&scanout->stats_work, K_MSEC(RGB_V2_STATS_PERIOD_MS));
}
#endif

static int rgb_v2_scanout_wait_for_submit(struct rgb_v2_scanout *scanout)
{
    const int64_t deadline = k_uptime_get() + RGB_V2_WRITE_WAIT_MS;
    k_spinlock_key_t key;
    bool queue_available = false;
    int result = -ETIMEDOUT;

    /* Do not repeatedly flush the complete PSRAM buffer while the prior
     * QUEUED frame is waiting for its prefetch boundary.
     */
    while (k_uptime_get() < deadline) {
        key = k_spin_lock(&scanout->state_lock);
        queue_available = (RGB_V2_FRAME_NO_PENDING == scanout->frames.pending_index);
        k_spin_unlock(&scanout->state_lock, key);
        if (true == queue_available) {
            result = rgb_v2_scanout_submit(scanout, scanout->write_index);
            if (-EBUSY != result) {
                return result;
            }
        }
        k_sleep(K_MSEC(RGB_V2_WRITE_RETRY_MS));
    }
    return -ETIMEDOUT;
}

/* A DRAWING lease is exclusive. Once acquired, QUEUED/SCANNING source cannot
 * become writable while this lease exists, even if VSYNC changes its role.
 * Do not perform PSRAM copies while holding state_lock (LCD ISR uses it).
 */
static int rgb_v2_scanout_begin_partial(struct rgb_v2_scanout *scanout)
{
    k_spinlock_key_t key;
    uint8_t destination = RGB_V2_FRAME_NO_PENDING;
    uint8_t source = RGB_V2_FRAME_NO_PENDING;
    int result = 0;

    key = k_spin_lock(&scanout->state_lock);
    result = rgb_v2_frames_acquire(&scanout->frames, &destination);

    if (0 == result) {
        source = (RGB_V2_FRAME_NO_PENDING != scanout->frames.pending_index) ?
                 (scanout->frames.pending_index) : (scanout->frames.active_index);
        scanout->write_is_partial = true;
    }

    k_spin_unlock(&scanout->state_lock, key);
    if (0 != result) {
        return result;
    }

    if ((RGB_V2_FRAME_COUNT <= source) || (source == destination)) {
        (void)rgb_v2_scanout_cancel(scanout, destination);
        return -EFAULT;
    }

    result = rgb_v2_dirty_sync(&scanout->dirty, destination,
                               scanout->frame_psram[source],
                               scanout->frame_psram[destination]);
    if (0 != result) {
        (void)rgb_v2_scanout_cancel(scanout, destination);
        return result;
    }

    scanout->write_index = destination;
    scanout->write_open = true;
    return 0;
}


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int rgb_v2_scanout_init(struct rgb_v2_scanout *scanout)
{
    int result = 0;

    if (NULL == scanout) {
        return -EINVAL;
    }

    LOG_INF("Scanout: %ux%u @ %u Hz, %u rows/bounce",
            (unsigned int)RGB_V2_WIDTH, (unsigned int)RGB_V2_HEIGHT,
            (unsigned int)RGB_V2_PCLK, (unsigned int)RGB_V2_BOUNCE_LINES);

    result = rgb_v2_init_pins();
    if (0 != result) {
        LOG_ERR("Failed to configure LCD pins (%d)", result);
        return result;
    }

    result = rgb_v2_init_lcd(scanout);
    if (0 != result) {
        LOG_ERR("Failed to initialize LCD_CAM (%d)", result);
        return result;
    }

    result = rgb_v2_init_frames(scanout);
    if (0 != result) {
        LOG_ERR("Failed to allocate PSRAM framebuffers (%d)", result);
        return result;
    }

    rgb_v2_frames_init(&scanout->frames);
    result = rgb_v2_dirty_init(&scanout->dirty, RGB_V2_WIDTH, RGB_V2_HEIGHT);
    if (0 != result) {
        rgb_v2_free_frames(scanout);
        return result;
    }

    k_mutex_init(&scanout->write_mutex);
    scanout->write_open = false;
    scanout->write_is_partial = false;

    result = rgb_v2_init_gdma(scanout);
    if (0 == result) {
        rgb_v2_ring_init(scanout);
        result = rgb_v2_prime_locked(scanout);
    }
    if (0 == result) {
        result = rgb_v2_init_vsync_irq(scanout);
    }
    if (0 != result) {
        LOG_ERR("Scanout setup failed (%d)", result);
        rgb_v2_free_frames(scanout);
        return result;
    }

#if defined(CONFIG_ESP32_LCD_RGB_V2_STATS)
    k_work_init_delayable(&scanout->stats_work, rgb_v2_stats_work);
#endif
    rgb_v2_start(scanout);
#if defined(CONFIG_ESP32_LCD_RGB_V2_STATS)
    (void)k_work_reschedule(&scanout->stats_work, K_MSEC(RGB_V2_STATS_PERIOD_MS));
#endif
    LOG_INF("v2 triple-frame scanout started; partial display_write() enabled");
    return 0;
}

int rgb_v2_scanout_acquire(struct rgb_v2_scanout *scanout, uint8_t *index,
                           uint16_t **pixels)
{
    k_spinlock_key_t key;
    uint8_t acquired = RGB_V2_FRAME_NO_PENDING;
    int result = 0;

    if ((NULL == scanout) || (NULL == index) || (NULL == pixels)) {
        return -EINVAL;
    }

    key = k_spin_lock(&scanout->state_lock);
    result = rgb_v2_frames_acquire(&scanout->frames, &acquired);
    k_spin_unlock(&scanout->state_lock, key);
    if (0 == result) {
        *index = acquired;
        *pixels = scanout->frame_psram[acquired];
    }

    return result;
}

int rgb_v2_scanout_submit(struct rgb_v2_scanout *scanout, uint8_t index)
{
    k_spinlock_key_t key;
    int result = 0;

    if ((NULL == scanout) || (RGB_V2_FRAME_COUNT <= index)) {
        return -EINVAL;
    }

    /* The writer owns the lease throughout the flush. Do not enter a long
     * cache operation while holding a spinlock shared with the LCD ISR.
     * The application must not submit/cancel the same lease concurrently.
     */
    key = k_spin_lock(&scanout->state_lock);
    if ((index != scanout->frames.drawing_index) ||
        (RGB_V2_FRAME_DRAWING != scanout->frames.state[index])) {
        result = -EPERM;
    } else if (RGB_V2_FRAME_NO_PENDING != scanout->frames.pending_index) {
        result = -EBUSY;
    }

    k_spin_unlock(&scanout->state_lock, key);
    if (0 != result) {
        return result;
    }

    result = sys_cache_data_flush_range(scanout->frame_psram[index], RGB_V2_FB_BYTES);
    if (0 != result) {
        return result; /* Ownership remains DRAWING; caller may retry/cancel. */
    }

    key = k_spin_lock(&scanout->state_lock);
    result = rgb_v2_frames_submit(&scanout->frames, index);
    if (0 == result) {
        if (false == scanout->write_is_partial) {
            /* Direct framebuffer submissions invalidate the whole frame. */
            rgb_v2_dirty_record_full(&scanout->dirty);
        }
        (void)rgb_v2_dirty_publish(&scanout->dirty, index);
        scanout->write_is_partial = false;
    }

    k_spin_unlock(&scanout->state_lock, key);
    return result;
}

int rgb_v2_scanout_write(struct rgb_v2_scanout *scanout, enum rgb_v2_rotation rotation,
                         uint16_t x, uint16_t y,
                         const struct display_buffer_descriptor *desc, const void *buf)
{
    const uint8_t *source = buf;
    const size_t pixel_bytes = sizeof(uint16_t);
    struct rgb_v2_rect logical = {0};
    struct rgb_v2_rect physical = {0};
    size_t stride = 0U;
    size_t row_bytes = 0U;
    size_t minimum_size = 0U;
    size_t destination_offset = 0U;
    uint16_t *pixels = NULL;
    uint16_t physical_x = 0U;
    uint16_t physical_y = 0U;
    uint32_t row = 0U;
    uint32_t col = 0U;
    int result = 0;

    if ((NULL == scanout) || (NULL == desc) || (NULL == buf) ||
        (desc->pitch < desc->width)) {
        return -EINVAL;
    }

    logical.x = x;
    logical.y = y;
    logical.width = desc->width;
    logical.height = desc->height;

    result = rgb_v2_rotation_rect(RGB_V2_WIDTH, RGB_V2_HEIGHT, rotation, &logical, &physical);
    if (0 != result) {
        return result;
    }

    stride = (size_t)desc->pitch * pixel_bytes;
    row_bytes = (size_t)desc->width * pixel_bytes;
    minimum_size = ((size_t)desc->height - 1U) * stride + row_bytes;
    if (desc->buf_size < minimum_size) {
        return -EINVAL;
    }

    result = k_mutex_lock(&scanout->write_mutex, K_MSEC(RGB_V2_WRITE_WAIT_MS));
    if (0 != result) {
        return result;
    }

    if (false == scanout->write_open) {
        result = rgb_v2_scanout_begin_partial(scanout);
        if (0 != result) {
            k_mutex_unlock(&scanout->write_mutex);
            return result;
        }
    }

    pixels = scanout->frame_psram[scanout->write_index];
    if (RGB_V2_ROTATION_0 == rotation) {
        for (row = 0U; row < desc->height; ++row) {
            destination_offset = ((size_t)y + row) * RGB_V2_WIDTH + x;
            memcpy(&pixels[destination_offset], source + ((size_t)row * stride), row_bytes);
        }
    } else {
        for (row = 0U; row < desc->height; ++row) {
            const uint16_t *src = (const uint16_t *)(const void *)(source + ((size_t)row * stride));
            for (col = 0U; col < desc->width; ++col) {
                rgb_v2_rotation_point(RGB_V2_WIDTH, RGB_V2_HEIGHT, rotation,
                                      (uint16_t)((uint32_t)x + col),
                                      (uint16_t)((uint32_t)y + row),
                                      &physical_x, &physical_y);
                pixels[((size_t)physical_y * RGB_V2_WIDTH) + physical_x] = src[col];
            }
        }
    }
    
    result = rgb_v2_dirty_record(&scanout->dirty, physical.x, physical.y,
                                 physical.width, physical.height);
    if ((0 == result) && (false == desc->frame_incomplete)) {
        result = rgb_v2_scanout_wait_for_submit(scanout);
        if (0 == result) {
            scanout->write_open = false;
        }
    }
    if (0 != result) {
        (void)rgb_v2_scanout_cancel(scanout, scanout->write_index);
        scanout->write_open = false;
    }
    
    k_mutex_unlock(&scanout->write_mutex);
    return result;
}

int rgb_v2_scanout_cancel(struct rgb_v2_scanout *scanout, uint8_t index)
{
    k_spinlock_key_t key;
    int result = 0;

    if (NULL == scanout) {
        return -EINVAL;
    }

    key = k_spin_lock(&scanout->state_lock);
    result = rgb_v2_frames_cancel(&scanout->frames, index);
    if (0 == result) {
        rgb_v2_dirty_cancel(&scanout->dirty, index);
        scanout->write_is_partial = false;
    }

    k_spin_unlock(&scanout->state_lock, key);
    return result;
}
