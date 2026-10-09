#include <errno.h>
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
#include <hal/dma_types.h>
#include <hal/gdma_channel.h>
#include <hal/gdma_ll.h>
#include <hal/lcd_hal.h>
#include <hal/lcd_ll.h>
#include <soc/gdma_struct.h>
#include <zephyr/multi_heap/shared_multi_heap.h>

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

#define LCD_BOUNCE_LINES            (10U)
#define LCD_BOUNCE_COUNT            (2U)
#define LCD_BOUNCE_SIZE             (LCD_HACTIVE * LCD_BOUNCE_LINES * sizeof(uint16_t))
#define LCD_DMA_DESC_SIZE           (3200U)
#define LCD_DESCS_PER_BOUNCE        (LCD_BOUNCE_SIZE / LCD_DMA_DESC_SIZE)
#define LCD_DMA_DESC_COUNT          (LCD_DESCS_PER_BOUNCE * LCD_BOUNCE_COUNT)

#define LCD_GDMA_PSRAM_BURST_SIZE   (32U)
#define LCD_GDMA_CACHE_ALIGNMENT    (32U)

#define LCD_COLOR_RED               (0xF800U)
#define LCD_COLOR_GREEN             (0x07E0U)
#define LCD_COLOR_BLUE              (0x001FU)
#define LCD_COLOR_WHITE             (0xFFFFU)
#define LCD_COLOR_BLACK             (0x0000U)

BUILD_ASSERT(0U < LCD_DMA_DESC_COUNT, "DMA descriptor count must be nonzero");
BUILD_ASSERT(0U == (LCD_BOUNCE_SIZE % LCD_DMA_DESC_SIZE), "Bounce size must match DMA blocks");
BUILD_ASSERT(0U == (LCD_VACTIVE % LCD_BOUNCE_LINES), "Display height must match bounce lines");
BUILD_ASSERT(0U == (LCD_BOUNCE_SIZE % LCD_GDMA_CACHE_ALIGNMENT), "Bounce buffer must be cache aligned");
BUILD_ASSERT(LCD_DMA_DESC_SIZE <= 4095U, "GDMA descriptor payload exceeds 12-bit size field");
BUILD_ASSERT(0U == (LCD_DMA_DESC_SIZE % LCD_GDMA_PSRAM_BURST_SIZE), "DMA descriptor size must be aligned to PSRAM block size");
BUILD_ASSERT(0U == (LCD_FRAMEBUFFER_SIZE % LCD_GDMA_CACHE_ALIGNMENT), "Framebuffer size must be cache aligned");
BUILD_ASSERT(0U < LCD_PERIPH_CLOCK_PRE_SCALE, "LCD peripheral clock prescaler must be nonzero");

#if !DT_NODE_HAS_STATUS(LCD_CAM_NODE, okay)
#error "LCD_CAM must be enabled in the board devicetree"
#endif

#define LCD_DMA_DEV                 DEVICE_DT_GET(DT_DMAS_CTLR_BY_NAME(LCD_CAM_NODE, tx))
#define LCD_DMA_CHANNEL             DT_DMAS_CELL_BY_NAME(LCD_CAM_NODE, tx, channel)
#define LCD_DMA_SLOT                SOC_GDMA_TRIG_PERIPH_LCD0

#define LCD_GDMA_HW_CHANNEL         (LCD_DMA_CHANNEL / 2U)

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

struct esp32_lcd_cam_rgb_data {
	enum display_pixel_format pixel_format;
	enum display_orientation orientation;

	lcd_hal_context_t hal;

	const struct device *dma_dev;
	uint32_t dma_channel;

	uint16_t *framebuffer;
	uint8_t bounce[LCD_BOUNCE_COUNT][LCD_BOUNCE_SIZE] __aligned(32);
	size_t next_line;
	volatile uint32_t bounce_eof_count;
	volatile uint32_t bounce_error_count;

	dma_descriptor_t dma_desc[LCD_DMA_DESC_COUNT] __aligned(4);

	struct dma_block_config dma_setup_block;
	struct dma_config dma_config;
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

static void lcd_dma_callback(const struct device *dev, void *user_data,
			     uint32_t channel, int status);

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

static int lcd_framebuffer_init(struct esp32_lcd_cam_rgb_data *data)
{
	static const uint16_t colors[] = {
		LCD_COLOR_RED,
		LCD_COLOR_GREEN,
		LCD_COLOR_BLUE,
		LCD_COLOR_WHITE,
		LCD_COLOR_BLACK,
	};

	data->framebuffer = shared_multi_heap_alloc(
		SMH_REG_ATTR_EXTERNAL, LCD_FRAMEBUFFER_SIZE);

	if (NULL == data->framebuffer) {
		LOG_ERR("Failed to allocate framebuffer in PSRAM");
		return -ENOMEM;
	}

	if (0U != ((uintptr_t)data->framebuffer % LCD_GDMA_PSRAM_BURST_SIZE)) {
		LOG_ERR("Framebuffer is not aligned to %u bytes",
			LCD_GDMA_PSRAM_BURST_SIZE);

		shared_multi_heap_free(data->framebuffer);
		data->framebuffer = NULL;

		return -EINVAL;
	}

	for (size_t y = 0U; y < LCD_VACTIVE; ++y) {
		for (size_t x = 0U; x < LCD_HACTIVE; ++x) {
			size_t color_index = (x * ARRAY_SIZE(colors)) / LCD_HACTIVE;

			data->framebuffer[(y * LCD_HACTIVE) + x] = colors[color_index];
		}
	}

	sys_cache_data_flush_range(data->framebuffer, LCD_FRAMEBUFFER_SIZE);

	LOG_INF("PSRAM framebuffer initialized: %u bytes at %p",
		(unsigned int)LCD_FRAMEBUFFER_SIZE,
		(void *)data->framebuffer);

	return 0;
}

/* Copy one 10-line slice from PSRAM into internal SRAM. */
static void lcd_bounce_fill(struct esp32_lcd_cam_rgb_data *data, size_t index)
{
	size_t offset = data->next_line * LCD_HACTIVE * sizeof(uint16_t);

	memcpy(data->bounce[index], (const uint8_t *)data->framebuffer + offset,
	       LCD_BOUNCE_SIZE);
	sys_cache_data_flush_range(data->bounce[index], LCD_BOUNCE_SIZE);
	data->next_line = (data->next_line + LCD_BOUNCE_LINES) % LCD_VACTIVE;
}

/* Invoked by Zephyr's existing GDMA ISR; no additional IRQ registration. */
static void lcd_dma_callback(const struct device *dev, void *user_data,
			     uint32_t channel, int status)
{
	struct esp32_lcd_cam_rgb_data *data = user_data;
	uint32_t eof_addr = 0U;
	size_t index = 0U;

	ARG_UNUSED(dev);
	ARG_UNUSED(channel);

	if (0 != status) {
		data->bounce_error_count++;
		return;
	}

	eof_addr = gdma_ll_tx_get_eof_desc_addr(&GDMA, LCD_GDMA_HW_CHANNEL);

	if (eof_addr == (uint32_t)(uintptr_t)&data->dma_desc[LCD_DESCS_PER_BOUNCE - 1U]) {
		index = 0U;
	} else if (eof_addr ==
		   (uint32_t)(uintptr_t)&data->dma_desc[LCD_DMA_DESC_COUNT - 1U]) {
		index = 1U;
	} else {
		data->bounce_error_count++;
		return;
	}

	lcd_bounce_fill(data, index);
	data->bounce_eof_count++;
}

static void lcd_dma_ring_init(struct esp32_lcd_cam_rgb_data *data)
{
	memset(data->dma_desc, 0, sizeof(data->dma_desc));
	data->next_line = 0U;
	data->bounce_eof_count = 0U;
	data->bounce_error_count = 0U;

	lcd_bounce_fill(data, 0U);
	lcd_bounce_fill(data, 1U);

	for (size_t i = 0U; i < LCD_DMA_DESC_COUNT; ++i) {
		size_t bounce_index = i / LCD_DESCS_PER_BOUNCE;
		size_t bounce_offset = (i % LCD_DESCS_PER_BOUNCE) * LCD_DMA_DESC_SIZE;

		data->dma_desc[i].buffer = &data->bounce[bounce_index][bounce_offset];
		data->dma_desc[i].dw0.size = LCD_DMA_DESC_SIZE;
		data->dma_desc[i].dw0.length = LCD_DMA_DESC_SIZE;
		data->dma_desc[i].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
		data->dma_desc[i].dw0.suc_eof =
			((i % LCD_DESCS_PER_BOUNCE) == (LCD_DESCS_PER_BOUNCE - 1U));
		data->dma_desc[i].next = &data->dma_desc[(i + 1U) % LCD_DMA_DESC_COUNT];
	}

	sys_cache_data_flush_range(data->dma_desc, sizeof(data->dma_desc));
	LOG_INF("SRAM bounce DMA ring: %u descriptors, %u bytes per buffer",
		LCD_DMA_DESC_COUNT, LCD_BOUNCE_SIZE);
}

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

static void lcd_dma_debug(struct esp32_lcd_cam_rgb_data *data)
{
	uint32_t status = gdma_ll_tx_get_interrupt_status(&GDMA, LCD_GDMA_HW_CHANNEL, true);

	LOG_INF("Bounce TX EOF callbacks: %u", data->bounce_eof_count);
	LOG_INF("Bounce callback errors: %u", data->bounce_error_count);
	LOG_INF("GDMA TX raw status: 0x%08x", status);
	LOG_INF("LCD VSYNC_END: %u",
		(0U != (data->hal.dev->lc_dma_int_raw.val & LCD_LL_EVENT_VSYNC_END)) ? 1U : 0U);
}

static int esp32_lcd_cam_rgb_init(const struct device *dev)
{
	struct esp32_lcd_cam_rgb_data *data = dev->data;
	const struct esp32_lcd_cam_rgb_config *config = dev->config;
	int ret = -1;

	data->pixel_format = PIXEL_FORMAT_RGB_565;
	data->orientation = DISPLAY_ORIENTATION_NORMAL;

	LOG_INF("Initializing ESP32-S3 LCD_CAM RGB display driver");

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

		shared_multi_heap_free(data->framebuffer);
		data->framebuffer = NULL;

		return ret;
	}

	lcd_dma_ring_init(data);
	lcd_rgb_start(data);

	k_msleep(1000);
	lcd_dma_debug(data);

	LOG_INF("LCD_CAM RGB initialized: %ux%u", config->width, config->height);
	return 0;
}

static int esp32_lcd_cam_rgb_write(const struct device *dev, const uint16_t x, const uint16_t y, 
								   const struct display_buffer_descriptor *desc, const void *buf)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(x);
	ARG_UNUSED(y);
	ARG_UNUSED(desc);
	ARG_UNUSED(buf);

	return -ENOTSUP;
}

static void esp32_lcd_cam_rgb_get_capabilities(const struct device *dev, struct display_capabilities *capabilities)
{
	const struct esp32_lcd_cam_rgb_data *data = dev->data;
	const struct esp32_lcd_cam_rgb_config *config = dev->config;

	memset(capabilities, 0, sizeof(*capabilities));

	capabilities->x_resolution = config->width;
	capabilities->y_resolution = config->height;

	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
	capabilities->current_pixel_format = data->pixel_format;
	capabilities->current_orientation = data->orientation;
}

static int esp32_lcd_cam_rgb_set_pixel_format(const struct device *dev, const enum display_pixel_format pixel_format)
{
	struct esp32_lcd_cam_rgb_data *data = dev->data;

	if (PIXEL_FORMAT_RGB_565 != pixel_format) {
		return -ENOTSUP;
	}

	data->pixel_format = pixel_format;

	return 0;
}

static int esp32_lcd_cam_rgb_set_orientation(const struct device *dev, const enum display_orientation orientation)
{
	struct esp32_lcd_cam_rgb_data *data = dev->data;

	if (DISPLAY_ORIENTATION_NORMAL != orientation) {
		return -ENOTSUP;
	}

	data->orientation = orientation;

	return 0;
}

static DEVICE_API(display, esp32_lcd_cam_rgb_api) = {
	.write = esp32_lcd_cam_rgb_write,
	.get_capabilities = esp32_lcd_cam_rgb_get_capabilities,
	.set_pixel_format = esp32_lcd_cam_rgb_set_pixel_format,
	.set_orientation = esp32_lcd_cam_rgb_set_orientation,
};

DEVICE_DT_INST_DEFINE(0, esp32_lcd_cam_rgb_init, NULL, &lcd_data, &lcd_config, POST_KERNEL,
			          CONFIG_DISPLAY_INIT_PRIORITY, &esp32_lcd_cam_rgb_api);
