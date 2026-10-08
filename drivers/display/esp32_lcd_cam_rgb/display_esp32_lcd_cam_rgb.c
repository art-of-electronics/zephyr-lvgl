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

#define DT_DRV_COMPAT espressif_esp32_lcd_cam_rgb
#define LCD_CAM_NODE DT_NODELABEL(lcd_cam)
#define LCD_DISPLAY_NODE DT_INST(0, DT_DRV_COMPAT)
#define LCD_TIMINGS_NODE DT_CHILD(LCD_DISPLAY_NODE, display_timings)

#define LCD_TEST_PATTERN_RGB565 (0xFFFFU)

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

#define LCD_DMA_BUFFER_PIXELS   	(4000U)
#define LCD_DMA_BUFFER_SIZE         (LCD_DMA_BUFFER_PIXELS * sizeof(uint16_t))

#define LCD_DMA_DESC_COUNT          (2U)
#define LCD_DMA_DESC_SIZE           (LCD_DMA_BUFFER_SIZE / LCD_DMA_DESC_COUNT)

BUILD_ASSERT((LCD_DMA_DESC_COUNT > 0U), "DMA descriptor count must be nonzero");
BUILD_ASSERT(0U == (LCD_DMA_BUFFER_SIZE % LCD_DMA_DESC_COUNT), "DMA buffer must be divisible by descriptor count");
BUILD_ASSERT(LCD_DMA_DESC_SIZE <= 4095U, "GDMA descriptor payload exceeds 12-bit size field");
BUILD_ASSERT(0U == (LCD_DMA_DESC_SIZE % sizeof(uint32_t)), "GDMA descriptor payload must be word-aligned");
BUILD_ASSERT(LCD_PERIPH_CLOCK_PRE_SCALE > 0U, "LCD peripheral clock prescaler must be nonzero");

#if !DT_NODE_HAS_STATUS(LCD_CAM_NODE, okay)
#error "LCD_CAM must be enabled in the board devicetree"
#endif

#define LCD_DMA_DEV                 DEVICE_DT_GET(DT_DMAS_CTLR_BY_NAME(LCD_CAM_NODE, tx))
#define LCD_DMA_CHANNEL             DT_DMAS_CELL_BY_NAME(LCD_CAM_NODE, tx, channel)
#define LCD_DMA_SLOT                SOC_GDMA_TRIG_PERIPH_LCD0

/*
 * ESP32 GDMA exposes RX/TX as separate Zephyr channels.
 * The hardware channel index is therefore channel / 2.
 */
#define LCD_GDMA_HW_CHANNEL         (LCD_DMA_CHANNEL / 2U)

/**
 * @brief Immutable RGB display panel configuration.
 *
 * Stores panel-specific parameters retrieved from Devicetree, including
 * display resolution, pixel clock frequency, and horizontal and vertical
 * synchronization timings.
 *
 * The configuration is initialized at compile time and remains unchanged
 * throughout the device lifetime.
 */
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
 * @brief Runtime state of the ESP32-S3 LCD_CAM RGB display driver.
 *
 * Stores the LCD_CAM HAL context, DMA device and channel configuration,
 * DMA transfer buffers and descriptors, and the current display state.
 *
 * Unlike the immutable device configuration, this structure contains
 * resources and parameters that may be modified during driver operation.
 *
 * Each driver instance maintains its own runtime state.
 */
struct esp32_lcd_cam_rgb_data {
	enum display_pixel_format pixel_format;
	enum display_orientation orientation;
	lcd_hal_context_t hal;
	const struct device *dma_dev;
	uint32_t dma_channel;

	/* DMA resources belong to the display device and must remain stable. */
	uint16_t dma_buffer[LCD_DMA_BUFFER_PIXELS] __aligned(32);
	dma_descriptor_t dma_desc[LCD_DMA_DESC_COUNT] __aligned(4);
	struct dma_block_config dma_setup_block;
	struct dma_config dma_config;
};

/*
 * DMA buffers and descriptors must reside in DMA-accessible memory.
 * Cache alignment and explicit cache flushing are required to ensure
 * data coherency before starting GDMA transfers.
 */
static struct esp32_lcd_cam_rgb_data lcd_data __aligned(32);

/*
 * This driver implementation supports a single LCD_CAM controller instance
 * (LCD_CAM0). Multi-instance support is not implemented.
 */
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

/**
 * @brief Initialize LCD_CAM pin configuration.
 *
 * Applies the default pinctrl state defined in Devicetree for the LCD_CAM
 * peripheral. This configures the GPIO pins required by the RGB interface,
 * including pixel data lines, pixel clock, and synchronization signals.
 *
 * @retval 0 Pin configuration applied successfully.
 * @return Negative error code returned by the pinctrl subsystem on failure.
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
 * @brief Initialize the LCD_CAM clock source and pixel clock divider.
 *
 * Retrieves the LCD clock source frequency and configures the LCD_CAM
 * peripheral clock using a fixed prescaler.
 *
 * Calculates and applies the pixel clock divider based on the frequency
 * specified in Devicetree. The resulting clock drives the RGB interface
 * and determines the pixel transmission rate.
 *
 * Validates the calculated divider against hardware limitations before
 * applying the clock configuration.
 *
 * @param data Pointer to the driver runtime data.
 * @param config Pointer to the immutable display configuration.
 *
 * @retval 0 Clock configuration completed successfully.
 * @return Negative error code if clock initialization fails.
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
 * @brief Configure the LCD_CAM RGB interface and panel synchronization timings.
 *
 * Configures the LCD_CAM peripheral for parallel RGB output, including
 * pixel data width, clock polarity, synchronization signal levels, and
 * horizontal and vertical timing parameters.
 *
 * Applies the HSYNC and VSYNC pulse widths, front and back porch intervals,
 * and active display resolution defined in Devicetree.
 *
 * Enables continuous RGB output and automatic frame restart to support
 * uninterrupted display refresh.
 *
 * @param data Pointer to the driver runtime data.
 * @param config Pointer to the immutable display configuration.
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
 * @brief Initialize and configure the LCD_CAM peripheral.
 *
 * Enables the LCD_CAM peripheral clock and resets the controller to ensure
 * a known initial hardware state.
 *
 * Initializes the LCD clock source and pixel clock divider, then configures
 * the RGB interface and panel synchronization timings according to the
 * parameters provided in the device configuration.
 *
 * Resets the LCD_CAM controller and its FIFO before applying the RGB timing
 * configuration.
 *
 * This function prepares the LCD_CAM peripheral for operation but does not
 * start pixel data transmission or the GDMA engine.
 *
 * @param data Pointer to the driver runtime data.
 * @param config Pointer to the immutable display configuration.
 *
 * @retval 0 LCD_CAM peripheral initialized successfully.
 * @return Negative error code if clock configuration or initialization fails.
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
 * @brief Initialize the GDMA transmit channel for LCD_CAM.
 *
 * Retrieves the DMA controller and channel configuration from Devicetree
 * and verifies that the DMA device is ready for operation.
 *
 * Configures the GDMA channel for memory-to-peripheral transfers and
 * assigns the LCD0 peripheral trigger to the selected channel.
 *
 * Translates the Zephyr DMA channel number into the corresponding
 * hardware GDMA TX channel index.
 *
 * The Zephyr DMA API is used to configure the channel and peripheral
 * routing. The actual DMA transfer is started separately using the
 * ESP32 GDMA low-level interface to support circular descriptor chains.
 *
 * @param data Pointer to the driver runtime data.
 *
 * @retval 0 GDMA transmit channel configured successfully.
 * @return Negative error code if the DMA device is unavailable,
 *         the channel configuration is invalid, or DMA setup fails.
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

	/*
	 * Let Zephyr configure the physical GDMA TX channel and connect it
	 * to the LCD0 peripheral trigger.
	 *
	 * The Zephyr DMA configuration only sets up the channel/trigger.
	 * Streaming uses the custom circular GDMA descriptor chain below;
	 * dma_start() is intentionally not called.
	 */
	data->dma_setup_block.source_address = (uintptr_t)data->dma_buffer;
	data->dma_setup_block.block_size = sizeof(uint32_t);
	data->dma_config.channel_direction = MEMORY_TO_PERIPHERAL;
	data->dma_config.dma_slot = LCD_DMA_SLOT;
	data->dma_config.block_count = 1U;
	data->dma_config.head_block = &data->dma_setup_block;

	ret = dma_config(data->dma_dev, data->dma_channel, &data->dma_config);
	if (ret < 0) {
		LOG_ERR("Failed to configure LCD DMA channel (%d)", ret);
		return ret;
	}

	LOG_INF("LCD GDMA configured: Zephyr channel %u, HW TX channel %u", data->dma_channel, LCD_GDMA_HW_CHANNEL);
	return 0;
}

/**
 * @brief Initialize the RGB565 test pattern and circular DMA descriptor chain.
 *
 * Fills the DMA buffer with a predefined RGB565 pixel pattern used for
 * display initialization and hardware verification.
 *
 * Divides the buffer into fixed-size blocks and initializes the corresponding
 * GDMA descriptors. Each descriptor specifies the source buffer address,
 * transfer size, buffer ownership, and the address of the next descriptor.
 *
 * Links the last descriptor back to the first one, creating a circular
 * descriptor chain for continuous pixel data transmission.
 *
 * Sets the successful EOF flag on the last descriptor to mark the end
 * of the DMA buffer sequence.
 *
 * Flushes the DMA buffer and descriptor memory from the CPU cache to ensure
 * that the GDMA controller accesses the initialized data.
 *
 * This function only prepares the DMA resources. It does not start
 * the GDMA engine or LCD_CAM pixel transmission.
 *
 * @param data Pointer to the driver runtime data.
 */
static void lcd_dma_ring_init(struct esp32_lcd_cam_rgb_data *data)
{
	uintptr_t buffer_addr = (uintptr_t)data->dma_buffer;

	for (size_t i = 0; i < ARRAY_SIZE(data->dma_buffer); ++i) {
		data->dma_buffer[i] = LCD_TEST_PATTERN_RGB565;
	}

	memset(data->dma_desc, 0, sizeof(data->dma_desc));

	for (size_t i = 0; i < LCD_DMA_DESC_COUNT; ++i) {
		data->dma_desc[i].buffer = (void *)(buffer_addr + (i * LCD_DMA_DESC_SIZE));
		data->dma_desc[i].dw0.size = LCD_DMA_DESC_SIZE;
		data->dma_desc[i].dw0.length = LCD_DMA_DESC_SIZE;
		data->dma_desc[i].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
		data->dma_desc[i].dw0.suc_eof = (i == (LCD_DMA_DESC_COUNT - 1U));
		data->dma_desc[i].next = &data->dma_desc[(i + 1U) % LCD_DMA_DESC_COUNT];
	}

	sys_cache_data_flush_range(data->dma_buffer, sizeof(data->dma_buffer));
	sys_cache_data_flush_range(data->dma_desc, sizeof(data->dma_desc));

	LOG_INF("LCD DMA ring prepared: %u descriptors, %u bytes", LCD_DMA_DESC_COUNT, LCD_DMA_BUFFER_SIZE);
}

/**
 * @brief Start continuous GDMA transfers and RGB signal generation.
 *
 * Stops the LCD_CAM controller and resets its internal state and FIFO
 * before starting a new pixel transmission sequence.
 *
 * Configures the GDMA transmit channel with the address of the first
 * descriptor in the circular DMA chain and starts the DMA engine.
 *
 * Ensures that the GDMA engine is started before enabling the LCD_CAM
 * controller, maintaining the required hardware initialization sequence.
 *
 * Once started, GDMA continuously supplies RGB565 pixel data to LCD_CAM,
 * while the peripheral generates the pixel clock and synchronization
 * signals required by the display panel.
 *
 * @param data Pointer to the driver runtime data.
 */
static void lcd_rgb_start(struct esp32_lcd_cam_rgb_data *data)
{
	lcd_cam_dev_t *dev = data->hal.dev;

	lcd_ll_stop(dev);
	lcd_ll_reset(dev);
	lcd_ll_fifo_reset(dev);

	gdma_ll_tx_set_desc_addr(&GDMA, LCD_GDMA_HW_CHANNEL, (uint32_t)(uintptr_t)data->dma_desc);
	gdma_ll_tx_start(&GDMA, LCD_GDMA_HW_CHANNEL);

	k_busy_wait(1);

	lcd_ll_start(dev);

	LOG_INF("LCD RGB stream started");
}

/**
 * @brief Initialize the ESP32-S3 LCD_CAM RGB display driver.
 *
 * Initializes the LCD_CAM peripheral and GDMA subsystem, configures the
 * display interface, and starts continuous transmission of the RGB565
 * test pattern.
 *
 * The initialization sequence is executed in a controlled order:
 * - Configure LCD_CAM GPIO pins using the Devicetree pinctrl settings.
 * - Initialize the LCD_CAM clock source and RGB synchronization timings.
 * - Configure the GDMA transmit channel and LCD0 peripheral trigger.
 * - Prepare the RGB565 test buffer and circular DMA descriptor chain.
 * - Start GDMA transfers and enable the LCD_CAM RGB output.
 *
 * Each initialization stage is executed only after the preceding stage
 * has completed successfully. Any reported error terminates initialization
 * and is propagated to the caller.
 *
 * The display backlight is not controlled by this function.
 *
 * @param dev Pointer to the display device instance.
 *
 * @retval 0 Display driver initialized successfully.
 * @return Negative error code if initialization fails.
 */
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

	ret = lcd_dma_channel_init(data);
	if (ret < 0) {
		LOG_ERR("GDMA initialization failed: %d", ret);
		return ret;
	}

	lcd_dma_ring_init(data);
	lcd_rgb_start(data);

	LOG_INF("LCD_CAM RGB initialized: %ux%u", config->width, config->height);
	return 0;
}

/**
 * @brief Write pixel data to the display.
 *
 * This function is currently a placeholder for the framebuffer-based
 * display update mechanism.
 *
 * @param dev Pointer to the display device instance.
 * @param x Horizontal coordinate of the target region.
 * @param y Vertical coordinate of the target region.
 * @param desc Pointer to the display buffer descriptor.
 * @param buf Pointer to the source pixel data.
 *
 * @retval -ENOTSUP Framebuffer writes are not implemented yet.
 *
 * @todo Implement framebuffer updates with RGB565 pixel data,
 *       DMA synchronization, and frame-safe buffer switching.
 */
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

/**
 * @brief Retrieve the display capabilities.
 *
 * Reports the display resolution defined in Devicetree and the pixel
 * formats and orientation supported by the driver.
 *
 * The current implementation supports RGB565 pixel format and normal
 * display orientation only.
 *
 * This function does not modify the display configuration or hardware state.
 *
 * @param dev Pointer to the display device instance.
 * @param capabilities Pointer to the structure receiving display capabilities.
 */
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

/**
 * @brief Set the display pixel format.
 *
 * Configures the pixel format used by the display driver.
 *
 * The current implementation supports RGB565 only. Requests for other
 * pixel formats are rejected without modifying the driver state.
 *
 * @param dev Pointer to the display device instance.
 * @param pixel_format Requested display pixel format.
 *
 * @retval 0 Pixel format accepted successfully.
 * @retval -ENOTSUP Requested pixel format is not supported.
 */
static int esp32_lcd_cam_rgb_set_pixel_format(const struct device *dev, const enum display_pixel_format pixel_format)
{
	struct esp32_lcd_cam_rgb_data *data = dev->data;

	if (PIXEL_FORMAT_RGB_565 != pixel_format) {
		return -ENOTSUP;
	}

	data->pixel_format = pixel_format;

	return 0;
}

/**
 * @brief Set the display orientation.
 *
 * Validates the requested display orientation against the orientations
 * supported by the driver.
 *
 * The current implementation supports normal orientation only.
 * Requests for other orientations are rejected without modifying
 * the driver state.
 *
 * @param dev Pointer to the display device instance.
 * @param orientation Requested display orientation.
 *
 * @retval 0 Display orientation accepted successfully.
 * @retval -ENOTSUP Requested orientation is not supported.
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

static DEVICE_API(display, esp32_lcd_cam_rgb_api) = {
	.write = esp32_lcd_cam_rgb_write,
	.get_capabilities = esp32_lcd_cam_rgb_get_capabilities,
	.set_pixel_format = esp32_lcd_cam_rgb_set_pixel_format,
	.set_orientation = esp32_lcd_cam_rgb_set_orientation,
};

DEVICE_DT_INST_DEFINE(0, esp32_lcd_cam_rgb_init, NULL, &lcd_data, &lcd_config, POST_KERNEL,
			          CONFIG_DISPLAY_INIT_PRIORITY, &esp32_lcd_cam_rgb_api);
