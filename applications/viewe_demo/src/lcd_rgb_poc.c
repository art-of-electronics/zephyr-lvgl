/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lcd_rgb_poc.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/cache.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <esp_clk_tree.h>
#include <hal/dma_types.h>
#include <hal/gdma_channel.h>
#include <hal/gdma_ll.h>
#include <hal/lcd_hal.h>
#include <hal/lcd_ll.h>
#include <soc/gdma_struct.h>

LOG_MODULE_REGISTER(lcd_rgb_poc, LOG_LEVEL_INF);

#define LCD_BUS_ID                     0U

#define LCD_PERIPH_CLOCK_PRE_SCALE     2U
#define LCD_PIXEL_CLOCK_HZ             20000000U

#define LCD_HACTIVE                    800U
#define LCD_VACTIVE                    480U

#define LCD_HSYNC_LEN                  1U
#define LCD_HBACK_PORCH                42U
#define LCD_HFRONT_PORCH               20U

#define LCD_VSYNC_LEN                  10U
#define LCD_VBACK_PORCH                12U
#define LCD_VFRONT_PORCH               4U

#define LCD_DATA_WIDTH                 16U

#define LCD_DMA_BUFFER_PIXELS          4000U
#define LCD_DMA_BUFFER_SIZE            \
	(LCD_DMA_BUFFER_PIXELS * sizeof(uint16_t))

#define LCD_DMA_DESC_COUNT             2U
#define LCD_DMA_DESC_SIZE              \
	(LCD_DMA_BUFFER_SIZE / LCD_DMA_DESC_COUNT)

#define LCD_CAM_NODE                   DT_NODELABEL(lcd_cam)

#if !DT_NODE_HAS_STATUS(LCD_CAM_NODE, okay)
#error "LCD_CAM must be enabled in the board devicetree"
#endif

#define LCD_DMA_DEV                    \
	DEVICE_DT_GET(DT_DMAS_CTLR_BY_NAME(LCD_CAM_NODE, tx))

#define LCD_DMA_CHANNEL                \
	DT_DMAS_CELL_BY_NAME(LCD_CAM_NODE, tx, channel)

#define LCD_DMA_SLOT                   SOC_GDMA_TRIG_PERIPH_LCD0

/*
 * ESP32 GDMA exposes RX/TX as separate Zephyr channels.
 * The hardware channel index is therefore channel / 2.
 */
#define LCD_GDMA_HW_CHANNEL            (LCD_DMA_CHANNEL / 2U)

struct lcd_rgb_poc_data {
	lcd_hal_context_t hal;
	const struct device *dma_dev;
	uint32_t dma_channel;
};

static struct lcd_rgb_poc_data lcd_data;

static uint16_t __aligned(32)
	lcd_dma_buffer[LCD_DMA_BUFFER_PIXELS];

static dma_descriptor_t __aligned(4)
	lcd_dma_desc[LCD_DMA_DESC_COUNT];

static struct dma_block_config lcd_dma_setup_block;
static struct dma_config lcd_dma_config;


static int lcd_clock_init(struct lcd_rgb_poc_data *data)
{
	const soc_periph_lcd_clk_src_t clock_source = LCD_CLK_SRC_DEFAULT;
	uint32_t source_clock_hz;
	uint32_t lcd_clock_hz;
	uint32_t pixel_clock_hz;
	uint32_t pclk_prescale;
	int ret;

	ret = esp_clk_tree_src_get_freq_hz(
		clock_source,
		ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX,
		&source_clock_hz);

	if (ret != ESP_OK) {
		LOG_ERR("Failed to get LCD clock source frequency (%d)", ret);
		return -EINVAL;
	}

	lcd_ll_select_clk_src(data->hal.dev, clock_source);

	lcd_ll_set_group_clock_coeff(
		data->hal.dev,
		LCD_PERIPH_CLOCK_PRE_SCALE,
		0,
		0);

	lcd_clock_hz =
		source_clock_hz / LCD_PERIPH_CLOCK_PRE_SCALE;

	pclk_prescale =
		DIV_ROUND_UP(lcd_clock_hz, LCD_PIXEL_CLOCK_HZ);

	if ((pclk_prescale == 0U) ||
	    (pclk_prescale > LCD_LL_PCLK_DIV_MAX)) {
		LOG_ERR("Unsupported pixel clock divider %u",
			pclk_prescale);
		return -EINVAL;
	}

	lcd_ll_set_pixel_clock_prescale(
		data->hal.dev,
		pclk_prescale);

	pixel_clock_hz = lcd_clock_hz / pclk_prescale;

	LOG_INF("LCD clock source: %u Hz", source_clock_hz);
	LOG_INF("LCD peripheral clock: %u Hz", lcd_clock_hz);
	LOG_INF("RGB pixel clock: %u Hz", pixel_clock_hz);

	return 0;
}


static void lcd_timing_init(struct lcd_rgb_poc_data *data)
{
	lcd_cam_dev_t *dev = data->hal.dev;

	lcd_ll_set_dma_read_stride(dev, LCD_DATA_WIDTH);

	lcd_ll_set_clock_idle_level(dev, false);
	lcd_ll_set_pixel_clock_edge(dev, true);

	lcd_ll_enable_rgb_mode(dev, true);
	lcd_ll_enable_color_convert(dev, false);

	/* RGB uses data phase only. */
	lcd_ll_set_phase_cycles(dev, 0, 0, 1);

	/* Data phase length is controlled by DMA. */
	lcd_ll_enable_output_always_on(dev, true);

	/* HSYNC/VSYNC active low, DE active high. */
	lcd_ll_set_idle_level(dev, true, true, false);

	/* RGB always contains front and back porch regions. */
	lcd_ll_set_blank_cycles(dev, 1, 1);

	lcd_ll_set_horizontal_timing(
		dev,
		LCD_HSYNC_LEN,
		LCD_HBACK_PORCH,
		LCD_HACTIVE,
		LCD_HFRONT_PORCH);

	lcd_ll_set_vertical_timing(
		dev,
		LCD_VSYNC_LEN,
		LCD_VBACK_PORCH,
		LCD_VACTIVE,
		LCD_VFRONT_PORCH);

	lcd_ll_enable_output_hsync_in_porch_region(dev, true);
	lcd_ll_set_hsync_position(dev, 0);

	lcd_ll_enable_auto_next_frame(dev, true);

	lcd_ll_set_delay_ticks(dev, 0, 0, 0);
	lcd_ll_set_data_delay_ticks(dev, 0);
}


static int lcd_cam_init(struct lcd_rgb_poc_data *data)
{
	int ret;

	lcd_ll_enable_bus_clock(LCD_BUS_ID, true);
	lcd_ll_reset_register(LCD_BUS_ID);

	data->hal.dev = LCD_LL_GET_HW(LCD_BUS_ID);

	//lcd_ll_enable_clock(data->hal.dev, true);
	lcd_ll_enable_output_always_on(data->hal.dev, false);

	ret = lcd_clock_init(data);
	if (ret < 0) {
		return ret;
	}

	lcd_ll_reset(data->hal.dev);
	lcd_ll_fifo_reset(data->hal.dev);

	//lcd_ll_enable_rgb_mode(data->hal.dev, true);
	//lcd_ll_enable_color_convert(data->hal.dev, false);
	//lcd_ll_enable_output_always_on(data->hal.dev, true);

	lcd_timing_init(data);

	return 0;
}


static int lcd_dma_channel_init(struct lcd_rgb_poc_data *data)
{
	int ret;

	data->dma_dev = LCD_DMA_DEV;
	data->dma_channel = LCD_DMA_CHANNEL;

	if (!device_is_ready(data->dma_dev)) {
		LOG_ERR("LCD DMA controller is not ready");
		return -ENODEV;
	}

	memset(&lcd_dma_setup_block, 0,
	       sizeof(lcd_dma_setup_block));

	memset(&lcd_dma_config, 0,
	       sizeof(lcd_dma_config));

	/*
	 * Let Zephyr configure the physical GDMA TX channel and connect it
	 * to the LCD0 peripheral trigger.
	 *
	 * This descriptor will never be started through the Zephyr DMA API.
	 */
	lcd_dma_setup_block.source_address =
		(uintptr_t)lcd_dma_buffer;

	lcd_dma_setup_block.block_size =
		sizeof(uint32_t);

	lcd_dma_config.channel_direction =
		MEMORY_TO_PERIPHERAL;

	lcd_dma_config.dma_slot =
		LCD_DMA_SLOT;

	lcd_dma_config.block_count = 1U;

	lcd_dma_config.head_block =
		&lcd_dma_setup_block;

	ret = dma_config(
		data->dma_dev,
		data->dma_channel,
		&lcd_dma_config);

	if (ret < 0) {
		LOG_ERR("Failed to configure LCD DMA channel (%d)", ret);
		return ret;
	}

	LOG_INF("LCD GDMA configured: Zephyr channel %u, HW TX channel %u",
		data->dma_channel,
		LCD_GDMA_HW_CHANNEL);

	return 0;
}


static void lcd_dma_ring_init(void)
{
	uintptr_t buffer_addr = (uintptr_t)lcd_dma_buffer;

	for (size_t i = 0; i < ARRAY_SIZE(lcd_dma_buffer); ++i) {
		lcd_dma_buffer[i] = 0xFFFFU;
	}

	memset(lcd_dma_desc, 0, sizeof(lcd_dma_desc));

	for (size_t i = 0; i < LCD_DMA_DESC_COUNT; ++i) {
	lcd_dma_desc[i].buffer =
		(void *)(buffer_addr +
			(i * LCD_DMA_DESC_SIZE));

	lcd_dma_desc[i].dw0.size = LCD_DMA_DESC_SIZE;
	lcd_dma_desc[i].dw0.length = LCD_DMA_DESC_SIZE;
	lcd_dma_desc[i].dw0.owner =
		DMA_DESCRIPTOR_BUFFER_OWNER_DMA;

	lcd_dma_desc[i].dw0.suc_eof =
		(i == (LCD_DMA_DESC_COUNT - 1U));

	lcd_dma_desc[i].next =
		&lcd_dma_desc[
			(i + 1U) % LCD_DMA_DESC_COUNT];
}

	sys_cache_data_flush_range(
		lcd_dma_buffer,
		sizeof(lcd_dma_buffer));

	sys_cache_data_flush_range(
		lcd_dma_desc,
		sizeof(lcd_dma_desc));

	LOG_INF("LCD DMA ring prepared: %u descriptors, %u bytes",
		LCD_DMA_DESC_COUNT,
		LCD_DMA_BUFFER_SIZE);
}


static void lcd_rgb_start(struct lcd_rgb_poc_data *data)
{
	lcd_cam_dev_t *dev = data->hal.dev;

	lcd_ll_stop(dev);
	lcd_ll_reset(dev);
	lcd_ll_fifo_reset(dev);

	gdma_ll_tx_set_desc_addr(
		&GDMA,
		LCD_GDMA_HW_CHANNEL,
		(uint32_t)(uintptr_t)lcd_dma_desc);

	gdma_ll_tx_start(
		&GDMA,
		LCD_GDMA_HW_CHANNEL);

	k_busy_wait(1);

	lcd_ll_start(dev);

	LOG_INF("LCD RGB stream started");

	k_msleep(10);

	LOG_INF("GDMA: idle=%d eof=0x%08x prefetched=0x%08x",
	gdma_ll_tx_is_desc_fsm_idle(
		&GDMA, LCD_GDMA_HW_CHANNEL),
	gdma_ll_tx_get_eof_desc_addr(
		&GDMA, LCD_GDMA_HW_CHANNEL),
	gdma_ll_tx_get_prefetched_desc_addr(
		&GDMA, LCD_GDMA_HW_CHANNEL));
}


int lcd_rgb_poc_init(void)
{
	int ret;

	LOG_INF("Initializing ESP32-S3 LCD_CAM RGB proof of concept");

	ret = lcd_cam_init(&lcd_data);
	if (ret < 0) {
		return ret;
	}

	ret = lcd_dma_channel_init(&lcd_data);
	if (ret < 0) {
		return ret;
	}

	lcd_dma_ring_init();
	lcd_rgb_start(&lcd_data);

	LOG_INF("LCD_CAM RGB initialized: %ux%u",
		LCD_HACTIVE,
		LCD_VACTIVE);

	return 0;
}