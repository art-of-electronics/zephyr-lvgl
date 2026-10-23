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
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <esp_clk_tree.h>
#include <esp_intr_alloc.h>
#include <soc/interrupts.h>
#include <hal/dma_types.h>
#include <hal/gdma_channel.h>
#include <hal/gdma_ll.h>
#include <hal/lcd_hal.h>
#include <hal/lcd_ll.h>
#include <soc/gdma_struct.h>
#include <zephyr/multi_heap/shared_multi_heap.h>

#include <zephyr/sys/atomic.h>

/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
#define DT_DRV_COMPAT espressif_esp32_lcd_cam_rgb
#define LCD_CAM_NODE DT_NODELABEL(lcd_cam)
#define LCD_DISPLAY_NODE DT_INST(0, DT_DRV_COMPAT)
#define LCD_TIMINGS_NODE DT_CHILD(LCD_DISPLAY_NODE, display_timings)

PINCTRL_DT_DEFINE(LCD_CAM_NODE);

LOG_MODULE_REGISTER(esp32_lcd_cam_rgb, CONFIG_DISPLAY_LOG_LEVEL);

#define LCD_BUS_ID                  (0U)

#define LCD_PERIPH_CLOCK_PRE_SCALE	(2U)
#define LCD_PIXEL_CLOCK_HZ          DT_PROP(LCD_TIMINGS_NODE, clock_frequency)

#define LCD_HACTIVE                 DT_PROP(LCD_DISPLAY_NODE, width)
#define LCD_VACTIVE                 DT_PROP(LCD_DISPLAY_NODE, height)

#define LCD_HSYNC_LEN               DT_PROP(LCD_TIMINGS_NODE, hsync_len)
#define LCD_HBACK_PORCH             DT_PROP(LCD_TIMINGS_NODE, hback_porch)
#define LCD_HFRONT_PORCH            DT_PROP(LCD_TIMINGS_NODE, hfront_porch)

#define LCD_VSYNC_LEN               DT_PROP(LCD_TIMINGS_NODE, vsync_len)
#define LCD_VBACK_PORCH             DT_PROP(LCD_TIMINGS_NODE, vback_porch)
#define LCD_VFRONT_PORCH            DT_PROP(LCD_TIMINGS_NODE, vfront_porch)

#define LCD_DATA_WIDTH              (16U)

#define LCD_FRAMEBUFFER_PIXELS      (LCD_HACTIVE * LCD_VACTIVE)
#define LCD_FRAMEBUFFER_SIZE        (LCD_FRAMEBUFFER_PIXELS * sizeof(uint16_t))
#define LCD_FRAMEBUFFER_COUNT       (CONFIG_ESP32_LCD_RGB_FRAMEBUFFER_COUNT)//(2U)

#define LCD_BOUNCE_LINES            (10U)
#define LCD_BOUNCE_COUNT            (2U)
#define LCD_BOUNCE_SIZE             (LCD_HACTIVE * LCD_BOUNCE_LINES * sizeof(uint16_t))
#define LCD_DMA_DESC_SIZE           (3200U)
#define LCD_DESCS_PER_BOUNCE        (LCD_BOUNCE_SIZE / LCD_DMA_DESC_SIZE)
#define LCD_DMA_DESC_COUNT          (LCD_DESCS_PER_BOUNCE * LCD_BOUNCE_COUNT)
#define LCD_FIFO_SKIP_BYTES         ((LCD_LL_FIFO_DEPTH + 1U) * sizeof(uint16_t))

#define LCD_GDMA_PSRAM_BURST_SIZE   (32U)
#define LCD_GDMA_CACHE_ALIGNMENT    (32U)

#if !DT_NODE_HAS_STATUS(LCD_CAM_NODE, okay)
#error "LCD_CAM must be enabled in the board devicetree"
#endif

#define LCD_DMA_DEV                 DEVICE_DT_GET(DT_DMAS_CTLR_BY_NAME(LCD_CAM_NODE, tx))
#define LCD_DMA_CHANNEL             DT_DMAS_CELL_BY_NAME(LCD_CAM_NODE, tx, channel)
#define LCD_DMA_SLOT                SOC_GDMA_TRIG_PERIPH_LCD0

#define LCD_GDMA_HW_CHANNEL         (LCD_DMA_CHANNEL / 2U)

#define LCD_STATS_INTERVAL_MS       (1000U)
#define LCD_EXPECTED_EOF_PER_FRAME  (LCD_VACTIVE / LCD_BOUNCE_LINES)
#define LCD_WRITE_STATS_SAMPLES     (100U)
#define LCD_PRESENT_PREFETCH_EOF    (LCD_EXPECTED_EOF_PER_FRAME - LCD_BOUNCE_COUNT)
#define LCD_WRITE_WAIT_TIMEOUT_MS   (1000U)

BUILD_ASSERT(LCD_FIFO_SKIP_BYTES < LCD_DMA_DESC_SIZE, "FIFO skip must fit one DMA descriptor");
BUILD_ASSERT(0U < LCD_DMA_DESC_COUNT, "DMA descriptor count must be nonzero");
BUILD_ASSERT(0U == (LCD_BOUNCE_SIZE % LCD_DMA_DESC_SIZE), "Bounce size must match DMA blocks");
BUILD_ASSERT(0U == (LCD_VACTIVE % LCD_BOUNCE_LINES), "Display height must match bounce lines");
BUILD_ASSERT(0U == (LCD_BOUNCE_SIZE % LCD_GDMA_CACHE_ALIGNMENT), "Bounce buffer must be cache aligned");
BUILD_ASSERT(LCD_DMA_DESC_SIZE <= 4095U, "GDMA descriptor payload exceeds 12-bit size field");
BUILD_ASSERT(0U == (LCD_DMA_DESC_SIZE % LCD_GDMA_PSRAM_BURST_SIZE), "DMA descriptor size must be aligned to PSRAM block size");
BUILD_ASSERT(0U == (LCD_FRAMEBUFFER_SIZE % LCD_GDMA_CACHE_ALIGNMENT), "Framebuffer size must be cache aligned");
BUILD_ASSERT(0U < LCD_PERIPH_CLOCK_PRE_SCALE, "LCD peripheral clock prescaler must be nonzero");
BUILD_ASSERT(LCD_EXPECTED_EOF_PER_FRAME > LCD_BOUNCE_COUNT, "At least two bounce transfers are required per frame");
BUILD_ASSERT(CONFIG_ESP_SPIRAM_HEAP_SIZE >= (LCD_FRAMEBUFFER_COUNT * LCD_FRAMEBUFFER_SIZE), "Insufficient PSRAM heap for LCD framebuffers");

/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
struct esp32_lcd_cam_rgb_config {
	uint16_t width;
	uint16_t height;
	uint32_t pixel_clock;
	uint16_t hsync_len;
	uint16_t hback_porch;
	uint16_t hfront_porch;
	uint16_t vsync_len;
	uint16_t vback_porch;
	uint16_t vfront_porch;
};

/**
 * @brief Bounding rectangle of framebuffer modifications.
 *
 * @note End coordinates are exclusive. The rectangle is owned by the
 *       display-write thread until a completed frame is presented.
 */
struct lcd_dirty_region {
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    bool valid;
};

/**
 * @brief Runtime state of the ESP32-S3 LCD_CAM RGB driver.
 *
 * @note The front buffer is scanned through two SRAM bounce buffers and a
 *       continuous GDMA descriptor ring. The back buffer receives LVGL writes.
 */
struct esp32_lcd_cam_rgb_data {
	enum display_pixel_format pixel_format;
	enum display_orientation orientation;

	lcd_hal_context_t hal;

	const struct device *dma_dev;
	uint32_t dma_channel;

	uint16_t *framebuffer[LCD_FRAMEBUFFER_COUNT];
	uint8_t front_idx;
	uint8_t back_idx;

	/* VSYNC DMA prefetch switches the front buffer; display_write() uses back. */
	atomic_t buffer_lock;
	atomic_t sync_pending;
	atomic_t sync_retry_needed;
	bool frame_ready;
	struct k_work sync_work;
	struct lcd_dirty_region dirty;
	struct lcd_dirty_region sync_region;

	atomic_t frame_swap_count;
	atomic_t swap_deferred_count;
	atomic_t sync_submit_error_count;

	uint8_t bounce[LCD_BOUNCE_COUNT][LCD_BOUNCE_SIZE] __aligned(32);
	size_t next_line; 		// Next framebuffer line to copy into a bounce buffer.
	intr_handle_t lcd_intr_handle;

	dma_descriptor_t restart_desc __aligned(4);
	dma_descriptor_t dma_desc[LCD_DMA_DESC_COUNT] __aligned(4);

	struct dma_block_config dma_setup_block;
	struct dma_config dma_config;

	atomic_t fill_max_cycles;
	atomic_t dma_eof_count;
	atomic_t dma_unexpected_eof_count;
	atomic_t dma_error_count;
	atomic_t vsync_count;

	uint32_t last_stats_ms;
	atomic_t debug_eof_addr;
	size_t bb_eof_count;

	uint32_t write_count;
	uint32_t write_max_us;
	uint64_t write_total_us;
	uint64_t write_total_bytes;
	atomic_t vsync_restart_count;
};

static struct esp32_lcd_cam_rgb_data lcd_data __aligned(32);

static const struct esp32_lcd_cam_rgb_config lcd_config = {
	.width = LCD_HACTIVE,
	.height = LCD_VACTIVE,
	.pixel_clock = LCD_PIXEL_CLOCK_HZ,
	.hsync_len = LCD_HSYNC_LEN,
	.hback_porch = LCD_HBACK_PORCH,
	.hfront_porch = LCD_HFRONT_PORCH,
	.vsync_len = LCD_VSYNC_LEN,
	.vback_porch = LCD_VBACK_PORCH,
	.vfront_porch = LCD_VFRONT_PORCH,
};


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/
static void lcd_dma_callback(const struct device *dev, void *user_data, uint32_t channel, int status);
static void lcd_framebuffer_sync_work_handler(struct k_work *work);


/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
/**
 * @brief Initialize the LCD_CAM pin configuration.
 * @return 0 on success, negative error code on failure.
 */
static int lcd_pinctrl_init(void)
{
	const struct pinctrl_dev_config *pcfg = PINCTRL_DT_DEV_CONFIG_GET(LCD_CAM_NODE);
	int ret = pinctrl_apply_state(pcfg, PINCTRL_STATE_DEFAULT);

	if (ret < 0) {
		LOG_ERR("Failed to apply LCD_CAM pinctrl: %d", ret);
		return ret;
	}

	LOG_INF("LCD_CAM RGB pinctrl applied");
	return 0;
}

/**
 * @brief Initialize the LCD_CAM clock configuration.
 * @param [in] data 	Pointer to the LCD driver runtime data.
 * @param [in] config 	Pointer to the LCD driver configuration.
 * @return 0 on success, negative error code on failure.
 */
static int lcd_clock_init(struct esp32_lcd_cam_rgb_data *data, const struct esp32_lcd_cam_rgb_config *config)
{
	const soc_periph_lcd_clk_src_t clock_source = LCD_CLK_SRC_DEFAULT;
	uint32_t source_clock_hz = 0U;
	uint32_t lcd_clock_hz = 0U;
	uint32_t pixel_clock_hz = 0U;
	uint32_t pclk_prescale = 0U;
	int ret = -1;

	ret = esp_clk_tree_src_get_freq_hz(clock_source, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &source_clock_hz);
	if (ESP_OK != ret) {
    	LOG_ERR("Failed to get LCD clock source frequency (%d)", ret);
		return -EINVAL;
	}

	if (0U == config->pixel_clock) {
		LOG_ERR("Pixel clock must be nonzero");
		return -EINVAL;
	}

	lcd_clock_hz = source_clock_hz / LCD_PERIPH_CLOCK_PRE_SCALE;
	pclk_prescale = DIV_ROUND_UP(lcd_clock_hz, config->pixel_clock);

	if ((0U == pclk_prescale) || (pclk_prescale > LCD_LL_PCLK_DIV_MAX)) {
		LOG_ERR("Unsupported pixel clock divider %u", pclk_prescale);
		return -EINVAL;
	}

	/* Apply clock configuration only after successful validation. */
	lcd_ll_select_clk_src(data->hal.dev, clock_source);
	lcd_ll_set_group_clock_coeff(data->hal.dev, LCD_PERIPH_CLOCK_PRE_SCALE, 0, 0);
	lcd_ll_set_pixel_clock_prescale(data->hal.dev, pclk_prescale);
	
	pixel_clock_hz = lcd_clock_hz / pclk_prescale;

	LOG_INF("LCD clock source: %u Hz", source_clock_hz);
	LOG_INF("LCD peripheral clock: %u Hz", lcd_clock_hz);
	LOG_INF("RGB pixel clock: %u Hz", pixel_clock_hz);
	return 0;
}

/**
 * @brief Configure the LCD_CAM RGB synchronization timings.
 * @param [in] data 	Pointer to the LCD driver runtime data.
 * @param [in] config 	Pointer to the LCD driver configuration.
 */
static void lcd_timing_init(struct esp32_lcd_cam_rgb_data *data, const struct esp32_lcd_cam_rgb_config *config)
{
	lcd_cam_dev_t *dev = data->hal.dev;

	lcd_ll_set_dma_read_stride(dev, LCD_DATA_WIDTH);
	lcd_ll_set_clock_idle_level(dev, false);
	lcd_ll_set_pixel_clock_edge(dev, true);
	lcd_ll_enable_rgb_mode(dev, true);
	lcd_ll_enable_color_convert(dev, false);

	lcd_ll_set_phase_cycles(dev, 0, 0, 1); 			// RGB uses data phase only.
	lcd_ll_enable_output_always_on(dev, true);		// Data phase length is controlled by DMA.
	lcd_ll_set_idle_level(dev, true, true, false); 	// HSYNC/VSYNC active low, DE active high.
	lcd_ll_set_blank_cycles(dev, 1, 1);				// RGB always contains front and back porch regions.

	lcd_ll_set_horizontal_timing(dev, config->hsync_len, config->hback_porch, config->width, config->hfront_porch);
	lcd_ll_set_vertical_timing(dev, config->vsync_len, config->vback_porch, config->height, config->vfront_porch);

	lcd_ll_enable_output_hsync_in_porch_region(dev, true);
	lcd_ll_set_hsync_position(dev, 0);
	lcd_ll_enable_auto_next_frame(dev, true);
	lcd_ll_set_delay_ticks(dev, 0, 0, 0);
	lcd_ll_set_data_delay_ticks(dev, 0);
}

/**
 * @brief Initialize the LCD_CAM peripheral and configure RGB output.
 * @param [in,out] data  Pointer to the LCD driver runtime data.
 * @param [in] config    Pointer to the LCD driver configuration.
 * @return 0 on success, negative error code on failure. 
 */
static int lcd_cam_init(struct esp32_lcd_cam_rgb_data *data, const struct esp32_lcd_cam_rgb_config *config)
{
	int ret = -1;

	lcd_ll_enable_bus_clock(LCD_BUS_ID, true);
	lcd_ll_reset_register(LCD_BUS_ID);

	data->hal.dev = LCD_LL_GET_HW(LCD_BUS_ID);
	lcd_ll_enable_output_always_on(data->hal.dev, false);

	ret = lcd_clock_init(data, config);
	if (ret < 0) {
		return ret;
	}

	lcd_ll_reset(data->hal.dev);
	lcd_ll_fifo_reset(data->hal.dev);
	lcd_timing_init(data, config);

	return 0;
}

/**
 * @brief Initialize and configure the LCD GDMA transmit channel.
 * @param [in,out] data Pointer to the LCD driver runtime data.
 * @return 0 on success, negative error code on failure.
 *
 * @note The LCD requires an odd-numbered Zephyr DMA channel for transmission. 
 */
static int lcd_dma_channel_init(struct esp32_lcd_cam_rgb_data *data)
{
  int ret = -1;

	data->dma_dev = LCD_DMA_DEV;
	data->dma_channel = LCD_DMA_CHANNEL;

	/* Zephyr uses even channels for RX and odd channels for TX. */
	if (0U == (data->dma_channel & 1U)) {
		LOG_ERR("Expected a GDMA TX channel, got %u", data->dma_channel);
		return -EINVAL;
	}

	if (!device_is_ready(data->dma_dev)) {
		LOG_ERR("LCD DMA controller is not ready");
		return -ENODEV;
	}

	memset(&data->dma_setup_block, 0, sizeof(data->dma_setup_block));
	memset(&data->dma_config, 0, sizeof(data->dma_config));

	data->dma_setup_block.source_address = (uintptr_t)&data->bounce[0][0];
	data->dma_setup_block.block_size = sizeof(uint32_t);
	data->dma_config.channel_direction = MEMORY_TO_PERIPHERAL;
	data->dma_config.dma_slot = LCD_DMA_SLOT;
	data->dma_config.block_count = 1U;
	data->dma_config.head_block = &data->dma_setup_block;
	data->dma_config.dma_callback = lcd_dma_callback;
	data->dma_config.user_data = data;

	ret = dma_config(data->dma_dev, data->dma_channel, &data->dma_config);
	if (ret < 0) {
		LOG_ERR("Failed to configure LCD DMA channel (%d)", ret);
		return ret;
	}

	LOG_INF("LCD GDMA configured: Zephyr channel %u, HW TX channel %u", data->dma_channel, LCD_GDMA_HW_CHANNEL);
	return 0;
}

/**
 * @brief Release both PSRAM framebuffers on initialization failure.
 * @param[in,out] data Pointer to the LCD driver runtime state.
 */
static void lcd_framebuffer_deinit(struct esp32_lcd_cam_rgb_data *data)
{
	for (size_t index = 0U; index < LCD_FRAMEBUFFER_COUNT; ++index) {
		if (NULL != data->framebuffer[index]) {
			shared_multi_heap_free(data->framebuffer[index]);
			data->framebuffer[index] = NULL;
		}
	}
}

/**
 * @brief Allocate two equally sized and aligned RGB565X PSRAM framebuffers.
 * @param[in,out] data Pointer to the LCD driver runtime state.
 * @return 0 on success, or a negative errno value.
 */
static int lcd_framebuffer_init(struct esp32_lcd_cam_rgb_data *data)
{
	for (size_t index = 0U; index < LCD_FRAMEBUFFER_COUNT; ++index) {
		data->framebuffer[index] = shared_multi_heap_aligned_alloc(SMH_REG_ATTR_EXTERNAL, 
										LCD_GDMA_CACHE_ALIGNMENT, LCD_FRAMEBUFFER_SIZE);
		if (NULL == data->framebuffer[index]) {
			LOG_ERR("Failed to allocate PSRAM framebuffer %u", (unsigned int)index);
			lcd_framebuffer_deinit(data);
			return -ENOMEM;
		}
		
		if (0U != ((uintptr_t)data->framebuffer[index] % LCD_GDMA_CACHE_ALIGNMENT)) {
			LOG_ERR("Framebuffer %u is not %u-byte aligned", (unsigned int)index, LCD_GDMA_CACHE_ALIGNMENT);
			lcd_framebuffer_deinit(data);
			return -EINVAL;
		}

        memset(data->framebuffer[index], 0, LCD_FRAMEBUFFER_SIZE);
        sys_cache_data_flush_range(data->framebuffer[index], LCD_FRAMEBUFFER_SIZE);
	}
/*
    data->front_idx = 0U;
    data->back_idx = 1U;

    LOG_INF("Front framebuffer: %u bytes at %p", (unsigned int)LCD_FRAMEBUFFER_SIZE, 
			(void *)data->framebuffer[data->front_idx]);
    LOG_INF("Back framebuffer: %u bytes at %p", (unsigned int)LCD_FRAMEBUFFER_SIZE, 
			(void *)data->framebuffer[data->back_idx]);
    return 0;
*/
	data->front_idx = 0U;
	data->back_idx = (1U < LCD_FRAMEBUFFER_COUNT) ? (1U) : (0U);

	LOG_INF("Framebuffer count: %u", LCD_FRAMEBUFFER_COUNT);

	for (size_t index = 0U; index < LCD_FRAMEBUFFER_COUNT; ++index) {
		LOG_INF("Framebuffer %u: %u bytes at %p", (unsigned int)index, 
			    (unsigned int)LCD_FRAMEBUFFER_SIZE, (void *)data->framebuffer[index]);
	}

	return 0;	
}

/**
 * @brief Merge a completed write rectangle with the current dirty bounds.
 *
 * @note Must be called with buffer_lock held from thread context.
 */
static void lcd_dirty_region_add(struct esp32_lcd_cam_rgb_data *data, uint16_t x, uint16_t y, 
								 uint16_t width, uint16_t height)
{
    const uint16_t x2 = (uint16_t)((uint32_t)x + (uint32_t)width);
    const uint16_t y2 = (uint16_t)((uint32_t)y + (uint32_t)height);

    if (false == data->dirty.valid) {
        data->dirty.x1 = x;
        data->dirty.y1 = y;
        data->dirty.x2 = x2;
        data->dirty.y2 = y2;
        data->dirty.valid = true;
        return;
    }

    data->dirty.x1 = MIN(data->dirty.x1, x);
    data->dirty.y1 = MIN(data->dirty.y1, y);
    data->dirty.x2 = MAX(data->dirty.x2, x2);
    data->dirty.y2 = MAX(data->dirty.y2, y2);
}

/**
 * @brief Synchronize the presented dirty rectangle to the newly inactive buffer.
 * @param[in] work Work item embedded in the display driver runtime state.
 *
 * @note sync_pending prevents writes and further flips while this copy runs.
 *       The scanned front framebuffer is never modified by this worker.
 */
static void lcd_framebuffer_sync_work_handler(struct k_work *work)
{
    struct esp32_lcd_cam_rgb_data *data = CONTAINER_OF(work, struct esp32_lcd_cam_rgb_data, sync_work);
    const struct lcd_dirty_region region = data->sync_region;
    const uint16_t *front = data->framebuffer[data->front_idx];
    uint16_t *back = data->framebuffer[data->back_idx];
    size_t row_bytes = 0U;

    if (true == region.valid) {
        row_bytes = (size_t)(region.x2 - region.x1) * sizeof(uint16_t);

        for (uint16_t y = region.y1; y < region.y2; ++y) {
            const size_t offset = (size_t)y * LCD_HACTIVE + region.x1;

            memcpy(&back[offset], &front[offset], row_bytes);
            sys_cache_data_flush_range(&back[offset], row_bytes);
        }
    }

    /* Publish synchronized back-buffer contents to the next display_write(). */
    atomic_set(&data->sync_pending, 0);
}

/**
 * @brief Acquire exclusive drawing access to the back framebuffer.
 * @param[in,out] data Pointer to the LCD driver runtime state.
 * @return 0 when acquired, or -ETIMEDOUT if the display is not progressing.
 *
 * @note A complete, not-yet-presented frame is immutable until presentation.
 *       Calls are from thread context only; the ISR never waits for this lock.
 */
static int lcd_drawing_buffer_lock(struct esp32_lcd_cam_rgb_data *data)
{
    const uint32_t start_ms = k_uptime_get_32();

    while ((uint32_t)(k_uptime_get_32() - start_ms) < LCD_WRITE_WAIT_TIMEOUT_MS) {
        if (0 != atomic_get(&data->sync_pending)) {
            k_sleep(K_MSEC(1U));
            continue;
        }

        if (true == atomic_cas(&data->buffer_lock, 0, 1)) {
            if ((0 == atomic_get(&data->sync_pending)) && (false == data->frame_ready)) {
                return 0;
            }

            atomic_set(&data->buffer_lock, 0);
        }

        k_sleep(K_MSEC(1U));
    }

    return -ETIMEDOUT;
}

/**
 * @brief Publish the completed back frame before next-frame DMA prefetch begins.
 * @param[in,out] data Pointer to the LCD driver runtime state.
 *
 * @note At EOF (48 - 2), the last two scan segments have already been
 *       prefetched from the old front. The subsequent EOF refills preload
 *       the new front's first 20 lines for the next physical frame.
 *       The expensive synchronization work is only queued at VSYNC.
 */
static void lcd_present_ready_frame(struct esp32_lcd_cam_rgb_data *data)
{
    uint8_t old_front = 0U;

	if (1U == LCD_FRAMEBUFFER_COUNT) {
		return;
	}
    if (0 != atomic_get(&data->sync_pending)) {
        atomic_inc(&data->swap_deferred_count);
        return;
    }
    if (false == atomic_cas(&data->buffer_lock, 0, 1)) {
        atomic_inc(&data->swap_deferred_count);
        return;
    }
    if ((false == data->frame_ready) || (false == data->dirty.valid) || (0 != atomic_get(&data->sync_pending))) {
        atomic_set(&data->buffer_lock, 0);
        return;
    }

    old_front = data->front_idx;
    data->front_idx = data->back_idx;
    data->back_idx = old_front;

    data->sync_region = data->dirty;
    data->dirty.valid = false;
    data->frame_ready = false;
    atomic_set(&data->sync_pending, 1);
    atomic_set(&data->sync_retry_needed, 1);
    atomic_inc(&data->frame_swap_count);

    atomic_set(&data->buffer_lock, 0);
}

/**
 * @brief Queue a pending framebuffer synchronization after VSYNC handling.
 * @param[in,out] data Pointer to the LCD driver runtime state.
 *
 * @note Submission is deferred from the time-critical DMA EOF ISR. Once
 *       submitted, the work item is not queued again until the next swap.
 *       If submission fails, the next VSYNC retries it.
 */
static void lcd_queue_sync_work(struct esp32_lcd_cam_rgb_data *data)
{
    int ret = 0;

    if ((0 == atomic_get(&data->sync_retry_needed)) || (0 == atomic_get(&data->sync_pending))) {
        return;
    }

    atomic_set(&data->sync_retry_needed, 0);
    ret = k_work_submit(&data->sync_work);
    if (0 > ret) {
        atomic_inc(&data->sync_submit_error_count);
        atomic_set(&data->sync_retry_needed, 1);
    }
}

/**
 * @brief Fill the selected DMA bounce buffer with framebuffer data.
 * @param [in,out] data Pointer to the LCD driver runtime data.
 * @param [in] index     Index of the destination bounce buffer.
 *
 * @note Copies a slice from external PSRAM to internal SRAM for GDMA transfer.
 */
static void lcd_bounce_fill(struct esp32_lcd_cam_rgb_data *data, size_t index)
{
    size_t offset = data->next_line * LCD_HACTIVE * sizeof(uint16_t);
    uint32_t start_cycles = 0U;
    uint32_t elapsed_cycles = 0U;
    atomic_val_t max_cycles = 0;

    start_cycles = k_cycle_get_32();

    memcpy(data->bounce[index], (const uint8_t *)data->framebuffer[data->front_idx] + offset, LCD_BOUNCE_SIZE);

    elapsed_cycles = k_cycle_get_32() - start_cycles;
    max_cycles = atomic_get(&data->fill_max_cycles);
    while ((atomic_val_t)elapsed_cycles > max_cycles) {
        if (true == atomic_cas(&data->fill_max_cycles, max_cycles, (atomic_val_t)elapsed_cycles)) {
            break;
        }
        max_cycles = atomic_get(&data->fill_max_cycles);
    }

    data->next_line = (data->next_line + LCD_BOUNCE_LINES) % LCD_VACTIVE;
}

/**
 * @brief Refill a completed bounce buffer and handle next-frame prefetch.
 * @param[in] dev Zephyr GDMA device.
 * @param[in,out] user_data Pointer to the LCD driver runtime state.
 * @param[in] channel GDMA channel identifier.
 * @param[in] status DMA transfer status.
 *
 * @note Preserve the proven alternating-buffer refill and EOF bookkeeping.
 */
static void lcd_dma_callback(const struct device *dev, void *user_data, uint32_t channel, int status)
{
    struct esp32_lcd_cam_rgb_data *data = user_data;
    size_t index = 0U;

    ARG_UNUSED(dev);
    ARG_UNUSED(channel);

    if (0 != status) {
        atomic_inc(&data->dma_error_count);
        return;
    }

    index = data->bb_eof_count % LCD_BOUNCE_COUNT;
    data->bb_eof_count++;
    atomic_inc(&data->dma_eof_count);

    /* Complete the current frame's final refill before swapping the source. */
    lcd_bounce_fill(data, index);

    /* The next two EOF refills populate the first lines of the next frame. */
    if ((LCD_PRESENT_PREFETCH_EOF == data->bb_eof_count) && (0U == data->next_line)) {
        lcd_present_ready_frame(data);
    }
}

/**
 * @brief Initialize the GDMA descriptor used to restart RGB transmission.
 * @param [in,out] data Pointer to the LCD driver runtime data.
 *
 * @note Based on the ESP-IDF esp_lcd_panel_rgb.c restart mechanism.
 * 		 The restart descriptor skips the LCD_CAM FIFO depth plus one pixel
 *       to preserve FIFO contents during transmission restart. Subsequent
 *       descriptors continue through the regular DMA descriptor ring.
 */
static void lcd_restart_desc_init(struct esp32_lcd_cam_rgb_data *data)
{
	memset(&data->restart_desc, 0, sizeof(data->restart_desc));

	data->restart_desc.buffer = &data->bounce[0][LCD_FIFO_SKIP_BYTES];
	data->restart_desc.dw0.size = LCD_DMA_DESC_SIZE - LCD_FIFO_SKIP_BYTES;
	data->restart_desc.dw0.length = LCD_DMA_DESC_SIZE - LCD_FIFO_SKIP_BYTES;
	data->restart_desc.dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
	data->restart_desc.dw0.suc_eof = 0;
	data->restart_desc.next = &data->dma_desc[1];

	sys_cache_data_flush_range(&data->restart_desc, sizeof(data->restart_desc));
}

/**
 * @brief Initialize the circular GDMA descriptor ring for RGB transmission.
 * @param [in,out] data Pointer to the LCD driver runtime data.
 *
 * @note The descriptor ring alternates between two SRAM bounce buffers.
 *       The final descriptor links back to the first, enabling continuous
 *       DMA transmission.
 */
static void lcd_dma_ring_init(struct esp32_lcd_cam_rgb_data *data)
{
	memset(data->dma_desc, 0, sizeof(data->dma_desc));
	data->next_line = 0U;

	lcd_bounce_fill(data, 0U);
	lcd_bounce_fill(data, 1U);

	for (size_t i = 0U; i < LCD_DMA_DESC_COUNT; ++i) {
		size_t bounce_index = i / LCD_DESCS_PER_BOUNCE;
		size_t bounce_offset = (i % LCD_DESCS_PER_BOUNCE) * LCD_DMA_DESC_SIZE;

		data->dma_desc[i].buffer = &data->bounce[bounce_index][bounce_offset];
		data->dma_desc[i].dw0.size = LCD_DMA_DESC_SIZE;
		data->dma_desc[i].dw0.length = LCD_DMA_DESC_SIZE;
		data->dma_desc[i].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
		data->dma_desc[i].dw0.suc_eof = ((i % LCD_DESCS_PER_BOUNCE) == (LCD_DESCS_PER_BOUNCE - 1U));
		data->dma_desc[i].next = &data->dma_desc[(i + 1U) % LCD_DMA_DESC_COUNT];
	}

	sys_cache_data_flush_range(data->dma_desc, sizeof(data->dma_desc));
	lcd_restart_desc_init(data);
	LOG_INF("SRAM bounce DMA ring: %u descriptors, %u bytes per buffer", LCD_DMA_DESC_COUNT, LCD_BOUNCE_SIZE);
}

/**
 * @brief Start continuous LCD_CAM RGB transmission using GDMA.
 * @param [in] data Pointer to the LCD driver runtime data.
 *
 * @note The GDMA TX interrupt is managed by Zephyr. No additional
 *       interrupt registration is required.
 */
static void lcd_rgb_start(struct esp32_lcd_cam_rgb_data *data)
{
	lcd_cam_dev_t *dev = data->hal.dev;

	lcd_ll_stop(dev);
	lcd_ll_reset(dev);
	lcd_ll_fifo_reset(dev);

	lcd_ll_clear_interrupt_status(dev, LCD_LL_EVENT_VSYNC_END);
	/* Enable TX_EOF only; Zephyr owns and dispatches the hardware IRQ. */
	gdma_ll_tx_clear_interrupt_status(&GDMA, LCD_GDMA_HW_CHANNEL, GDMA_LL_EVENT_TX_EOF);
	gdma_ll_tx_enable_interrupt(&GDMA, LCD_GDMA_HW_CHANNEL, GDMA_LL_EVENT_TX_EOF, true);
	gdma_ll_tx_enable_data_burst(&GDMA, LCD_GDMA_HW_CHANNEL, false);
	gdma_ll_tx_set_desc_addr(&GDMA, LCD_GDMA_HW_CHANNEL, (uint32_t)(uintptr_t)data->dma_desc);
	gdma_ll_tx_start(&GDMA, LCD_GDMA_HW_CHANNEL);

	k_busy_wait(1);

	lcd_ll_start(dev);

	LOG_INF("LCD RGB stream started");
}

/**
 * @brief Handle LCD_CAM VSYNC and conditionally recover GDMA transmission.
 * @param [in,out] arg Pointer to the LCD driver runtime data.
 *
 * @note Implements the ESP32-S3 VSYNC recovery mechanism based on ESP-IDF
 *       v5.4.2. The restart preserves the required FIFO pixel offset
 *       and synchronizes bounce buffer refilling with frame boundaries.
 * @note The LCD FIFO must be reset before the GDMA channel is restarted.
 *       Do not stop the LCD timing engine or invoke Zephyr DMA API
 *       functions from this interrupt handler.
 */
static void lcd_vsync_isr(void *arg)
{
	struct esp32_lcd_cam_rgb_data *data = arg;
	lcd_cam_dev_t *dev = data->hal.dev;
	const uint32_t status = lcd_ll_get_interrupt_status(dev);
	size_t frame_eof_count = 0U;

	lcd_ll_clear_interrupt_status(dev, status);
	if (0U == (status & LCD_LL_EVENT_VSYNC_END)) {
		return;
	}

	atomic_inc(&data->vsync_count);

	/* ESP-IDF: position beyond the first two bounce buffers means we
	 * have not preloaded the upcoming frame yet. Start at its origin.
	 */
	if (data->next_line > (LCD_BOUNCE_COUNT * LCD_BOUNCE_LINES)) {
		data->next_line = 0U;
	}

	if (data->next_line < LCD_BOUNCE_LINES) {
		lcd_bounce_fill(data, 0U);
	}

	frame_eof_count = data->bb_eof_count;
	data->bb_eof_count = 0U;
	if (frame_eof_count >= LCD_EXPECTED_EOF_PER_FRAME) {
		lcd_queue_sync_work(data);
		return;
	}
	atomic_inc(&data->vsync_restart_count);

	/* ESP32-S3: restart from a dedicated descriptor that skips the LCD
	 * FIFO-preserved pixels. Follow ESP-IDF: reset FIFO, reset GDMA,
	 * and restart descriptors without stopping the LCD timing engine.
	 * Do not invoke Zephyr dma_stop()/dma_start() from this interrupt.
	 */
	lcd_ll_fifo_reset(dev);
	gdma_ll_tx_reset_channel(&GDMA, LCD_GDMA_HW_CHANNEL);
	gdma_ll_tx_set_desc_addr(&GDMA, LCD_GDMA_HW_CHANNEL, (uint32_t)(uintptr_t)&data->restart_desc);
	gdma_ll_tx_start(&GDMA, LCD_GDMA_HW_CHANNEL);

	if (data->next_line < (LCD_BOUNCE_COUNT * LCD_BOUNCE_LINES)) {
		lcd_bounce_fill(data, 1U);
	}

	lcd_queue_sync_work(data);
}

/**
 * @brief Register and enable the LCD_CAM VSYNC interrupt handler.
 * @param [in,out] data Pointer to the LCD driver runtime data.
 * @return 0 on success, negative error code on failure.
 *
 * @note The interrupt is allocated in a disabled state and enabled
 *       only after configuring the LCD_CAM VSYNC interrupt source.
 */
static int lcd_vsync_irq_init(struct esp32_lcd_cam_rgb_data *data)
{
	int ret = esp_intr_alloc(ETS_LCD_CAM_INTR_SOURCE, ESP_INTR_FLAG_INTRDISABLED, lcd_vsync_isr, data,
							 &data->lcd_intr_handle);
	if (ESP_OK != ret) {
		LOG_ERR("LCD_CAM IRQ allocation failed (%d)", ret);
		return -EIO;
	}

	lcd_ll_clear_interrupt_status(data->hal.dev, LCD_LL_EVENT_VSYNC_END);
	lcd_ll_enable_interrupt(data->hal.dev, LCD_LL_EVENT_VSYNC_END, true);
	ret = esp_intr_enable(data->lcd_intr_handle);
	if (ESP_OK != ret) {
		lcd_ll_enable_interrupt(data->hal.dev, LCD_LL_EVENT_VSYNC_END, false);
		(void)esp_intr_free(data->lcd_intr_handle);
		data->lcd_intr_handle = NULL;

		LOG_ERR("LCD_CAM IRQ enable failed (%d)", ret);
		return -EIO;
	}

	return 0;
}

/**
 * @brief Initialize the ESP32-S3 LCD_CAM RGB display driver.
 * @param [in] dev Pointer to the display device instance.
 * @return 0 on success, negative error code on failure.
 *
 * @note Initializes the LCD_CAM peripheral, allocates the framebuffer,
 *       configures GDMA and VSYNC interrupts, and starts RGB transmission.
 */
static int esp32_lcd_cam_rgb_init(const struct device *dev)
{
	struct esp32_lcd_cam_rgb_data *data = dev->data;
	const struct esp32_lcd_cam_rgb_config *config = dev->config;
	int ret = -1;
/*
	data->pixel_format = PIXEL_FORMAT_RGB_565X;
	data->orientation = DISPLAY_ORIENTATION_NORMAL;
	data->front_idx = 0U;
	data->back_idx = 1U;
	data->frame_ready = false;
	data->dirty.valid = false;
	atomic_set(&data->buffer_lock, 0);
	atomic_set(&data->sync_pending, 0);
	atomic_set(&data->sync_retry_needed, 0);
	k_work_init(&data->sync_work, lcd_framebuffer_sync_work_handler);

	LOG_INF("Initializing ESP32-S3 LCD_CAM RGB display driver (double framebuffer)");
*/
	data->pixel_format = PIXEL_FORMAT_RGB_565X;
	data->orientation = DISPLAY_ORIENTATION_NORMAL;
	data->front_idx = 0U;
	data->back_idx = (1U < LCD_FRAMEBUFFER_COUNT) ? 1U : 0U;
	data->frame_ready = false;
	data->dirty.valid = false;

	atomic_set(&data->buffer_lock, 0);
	atomic_set(&data->sync_pending, 0);
	atomic_set(&data->sync_retry_needed, 0);

	k_work_init(&data->sync_work, lcd_framebuffer_sync_work_handler);

	LOG_INF("Initializing ESP32-S3 LCD_CAM RGB display driver (%u framebuffer(s))", LCD_FRAMEBUFFER_COUNT);

	ret = lcd_pinctrl_init();
	if (ret < 0) {
		return ret;
	}

	ret = lcd_cam_init(data, config);
	if (ret < 0) {
		LOG_ERR("LCD_CAM initialization failed: %d", ret);
		return ret;
	}

	ret = lcd_framebuffer_init(data);
	if (ret < 0) {
		LOG_ERR("Framebuffer initialization failed: %d", ret);
		return ret;
	}

	ret = lcd_dma_channel_init(data);
	if (ret < 0) {
		LOG_ERR("GDMA initialization failed: %d", ret);
		lcd_framebuffer_deinit(data);
		return ret;
	}

	lcd_dma_ring_init(data);
	ret = lcd_vsync_irq_init(data);
	if (ret < 0) {
		lcd_framebuffer_deinit(data);
		return ret;
	}
	lcd_rgb_start(data);

	LOG_INF("LCD_CAM RGB initialized: %ux%u", config->width, config->height);
	return 0;
}

/**
 * @brief Write RGB565X pixels to the inactive PSRAM framebuffer.
 * @param[in] dev Display device instance.
 * @param[in] x Destination X coordinate.
 * @param[in] y Destination Y coordinate.
 * @param[in] desc Source buffer layout and frame-completion information.
 * @param[in] buf Source pixel data.
 * @return 0 on success or negative errno on failure.
 *
 * @note Dirty regions from partial LVGL flushes are merged. Only the last
 *       flush (frame_incomplete == false) marks the back buffer ready for
 *       presentation. Synchronization never modifies the scanned front.
 */
static int esp32_lcd_cam_rgb_write(const struct device *dev, uint16_t x, uint16_t y,
                                   const struct display_buffer_descriptor *desc, const void *buf)
{
    struct esp32_lcd_cam_rgb_data *data = dev->data;
    const struct esp32_lcd_cam_rgb_config *config = dev->config;
    size_t source_stride = 0U;
    size_t row_bytes = 0U;
    uint16_t *back = NULL;
    uint32_t start_cycles = 0U;
    uint32_t elapsed_cycles = 0U;
    uint32_t elapsed_us = 0U;
    uint32_t now_ms = 0U;
    bool log_write = false;
    uint32_t write_avg_us = 0U;
    uint32_t write_max_us = 0U;
    uint32_t write_avg_bytes = 0U;
    int ret = 0;
/*
    if ((NULL == desc) || (NULL == buf) || (NULL == data->framebuffer[0]) || (NULL == data->framebuffer[1])) {
        return -EINVAL;
    }
*/
	if ((NULL == desc) || (NULL == buf) || (NULL == data->framebuffer[data->back_idx])) {
		return -EINVAL;
	}
    if ((0U == desc->width) || (0U == desc->height) || (desc->pitch < desc->width)) {
        return -EINVAL;
    }
    if (((uint32_t)x + desc->width > config->width) || ((uint32_t)y + desc->height > config->height)) {
        return -EINVAL;
    }

    source_stride = (size_t)desc->pitch * sizeof(uint16_t);
    row_bytes = (size_t)desc->width * sizeof(uint16_t);

    if ((source_stride > (SIZE_MAX / desc->height)) ||
        (desc->buf_size < (((size_t)desc->height - 1U) * source_stride + row_bytes))) {
        return -EINVAL;
    }

    ret = lcd_drawing_buffer_lock(data);
    if (0 != ret) {
        LOG_ERR("Back framebuffer unavailable: %d (VSYNC=%d, swaps=%d, sync=%d)",
                ret, (int)atomic_get(&data->vsync_count),
                (int)atomic_get(&data->frame_swap_count),
                (int)atomic_get(&data->sync_pending));
        return ret;
    }

    back = data->framebuffer[data->back_idx];
    start_cycles = k_cycle_get_32();

    for (size_t row = 0U; row < desc->height; ++row) {
        uint16_t *dst = &back[((size_t)y + row) * config->width + x];
        const uint8_t *src = (const uint8_t *)buf + row * source_stride;

        memcpy(dst, src, row_bytes);
        sys_cache_data_flush_range(dst, row_bytes);
    }

    elapsed_cycles = k_cycle_get_32() - start_cycles;
    elapsed_us = k_cyc_to_us_floor32(elapsed_cycles);
/*
    lcd_dirty_region_add(data, x, y, desc->width, desc->height);
    if (false == desc->frame_incomplete) {
        data->frame_ready = true;
    }
*/
	if (1U < LCD_FRAMEBUFFER_COUNT) {
		lcd_dirty_region_add(data, x, y, desc->width, desc->height);
		if (false == desc->frame_incomplete) {
			data->frame_ready = true;
		}
	}

    data->write_count++;
    data->write_total_us += (uint64_t)elapsed_us;
    data->write_total_bytes += (uint64_t)row_bytes * (uint64_t)desc->height;

    if (data->write_max_us < elapsed_us) {
        data->write_max_us = elapsed_us;
    }

    if (LCD_WRITE_STATS_SAMPLES <= data->write_count) {
        write_avg_us = (uint32_t)(data->write_total_us / data->write_count);
        write_max_us = data->write_max_us;
        write_avg_bytes = (uint32_t)(data->write_total_bytes / data->write_count);
        log_write = true;
        data->write_count = 0U;
        data->write_max_us = 0U;
        data->write_total_us = 0U;
        data->write_total_bytes = 0U;
    }

    atomic_set(&data->buffer_lock, 0);

    if (true == log_write) {
        LOG_INF("LCD write: avg=%u us, max=%u us, avg_bytes=%u, samples=%u",
                write_avg_us, write_max_us, write_avg_bytes, LCD_WRITE_STATS_SAMPLES);
    }

    now_ms = k_uptime_get_32();
    if ((now_ms - data->last_stats_ms) >= LCD_STATS_INTERVAL_MS) {
        const uint32_t max_cycles = (uint32_t)atomic_set(&data->fill_max_cycles, 0);
        const uint32_t max_us = k_cyc_to_us_floor32(max_cycles);

        data->last_stats_ms = now_ms;
        LOG_INF("LCD: fill_max=%u us, EOF=%d, VSYNC=%d, restarts=%d, errors=%d, swaps=%d, defer=%d, sync_err=%d",
                max_us, (int)atomic_get(&data->dma_eof_count),
                (int)atomic_get(&data->vsync_count),
                (int)atomic_get(&data->vsync_restart_count),
                (int)atomic_get(&data->dma_error_count),
                (int)atomic_get(&data->frame_swap_count),
                (int)atomic_get(&data->swap_deferred_count),
                (int)atomic_get(&data->sync_submit_error_count));
    }

    return 0;
}

/**
 * @brief Retrieve the LCD display capabilities.
 * @param [in] dev           	Pointer to the display device instance.
 * @param [out] capabilities	Pointer to the display capabilities structure.
 */
static void esp32_lcd_cam_rgb_get_capabilities(const struct device *dev, struct display_capabilities *capabilities)
{
	const struct esp32_lcd_cam_rgb_data *data = dev->data;
	const struct esp32_lcd_cam_rgb_config *config = dev->config;

	memset(capabilities, 0, sizeof(*capabilities));

	capabilities->x_resolution = config->width;
	capabilities->y_resolution = config->height;

	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565X;
	capabilities->current_pixel_format = data->pixel_format;
	capabilities->current_orientation = data->orientation;
}

/**
 * @brief Set the LCD display pixel format.
 * @param [in] dev          Pointer to the display device instance.
 * @param [in] pixel_format	Requested pixel format.
 * @return 0 on success, -ENOTSUP if the requested format is unsupported.
 *
 * @note Only RGB565 pixel format is supported.
 */
static int esp32_lcd_cam_rgb_set_pixel_format(const struct device *dev, const enum display_pixel_format pixel_format)
{
	struct esp32_lcd_cam_rgb_data *data = dev->data;

	if (PIXEL_FORMAT_RGB_565X != pixel_format) {
		return -ENOTSUP;
	}

	data->pixel_format = pixel_format;

	return 0;
}

/**
 * @brief Set the LCD display orientation.
 * @param [in] dev         	Pointer to the display device instance.
 * @param [in] orientation	Requested display orientation.
 * @return 0 on success, -ENOTSUP if the requested orientation is unsupported.
 *
 * @note Only the normal display orientation is supported.
 */
static int esp32_lcd_cam_rgb_set_orientation(const struct device *dev, const enum display_orientation orientation)
{
	struct esp32_lcd_cam_rgb_data *data = dev->data;

	if (DISPLAY_ORIENTATION_NORMAL != orientation) {
		return -ENOTSUP;
	}

	data->orientation = orientation;

	return 0;
}

/*****************************************************************************
                            DEVICE REGISTRATION
*****************************************************************************/

static DEVICE_API(display, esp32_lcd_cam_rgb_api) = {
	.write = esp32_lcd_cam_rgb_write,
	.get_capabilities = esp32_lcd_cam_rgb_get_capabilities,
	.set_pixel_format = esp32_lcd_cam_rgb_set_pixel_format,
	.set_orientation = esp32_lcd_cam_rgb_set_orientation,
};

DEVICE_DT_INST_DEFINE(0, esp32_lcd_cam_rgb_init, NULL, &lcd_data, &lcd_config, 
					  POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY, &esp32_lcd_cam_rgb_api);
