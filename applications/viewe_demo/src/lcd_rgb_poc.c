/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP32-S3 LCD_CAM RGB proof-of-concept implementation.
 *
 * This module provides an experimental initialization path for the
 * ESP32-S3 LCD_CAM peripheral operating in RGB mode.
 *
 * It is intentionally kept outside the board definition and the Zephyr
 * display subsystem until the RGB timing and DMA operation have been
 * validated on real hardware.
 */

#include "lcd_rgb_poc.h"

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <esp_clk_tree.h>
#include <hal/gdma_channel.h>
#include <hal/lcd_hal.h>
#include <hal/lcd_ll.h>

LOG_MODULE_REGISTER(lcd_rgb_poc, LOG_LEVEL_INF);

/** LCD_CAM peripheral index. ESP32-S3 provides a single LCD_CAM block. */
#define LCD_BUS_ID 0

/** LCD_CAM peripheral clock pre-divider used by the existing Zephyr driver. */
#define LCD_PERIPH_CLOCK_PRE_SCALE 2U

/** Target RGB pixel clock frequency. */
#define LCD_PIXEL_CLOCK_HZ 20000000U

/** Active display width in pixels. */
#define LCD_HACTIVE 800U

/** Active display height in pixels. */
#define LCD_VACTIVE 480U

/** Horizontal synchronization pulse width. */
#define LCD_HSYNC_LEN 1U

/** Horizontal back porch. */
#define LCD_HBACK_PORCH 42U

/** Horizontal front porch. */
#define LCD_HFRONT_PORCH 20U

/** Vertical synchronization pulse width. */
#define LCD_VSYNC_LEN 10U

/** Vertical back porch. */
#define LCD_VBACK_PORCH 12U

/** Vertical front porch. */
#define LCD_VFRONT_PORCH 4U

/** RGB565 bus width in bits. */
#define LCD_DATA_WIDTH 16U

#define LCD_CAM_NODE DT_NODELABEL(lcd_cam)

#if !DT_NODE_HAS_STATUS(LCD_CAM_NODE, okay)
#error "LCD_CAM must be enabled in the board devicetree"
#endif

/**
 * @brief Runtime state of the RGB proof of concept.
 */
struct lcd_rgb_poc_data {
	lcd_hal_context_t hal;
	uint32_t source_clock_hz;
	uint32_t lcd_clock_hz;
	uint32_t pixel_clock_hz;
};

static struct lcd_rgb_poc_data lcd_data;

/**
 * @brief Configure the LCD_CAM peripheral clock.
 *
 * The clock setup follows the same basic sequence as Zephyr's ESP32
 * MIPI DBI LCD_CAM driver. The peripheral source clock is divided first
 * by the LCD_CAM group clock pre-divider and then by the pixel clock
 * divider.
 *
 * @param data RGB proof-of-concept runtime data.
 *
 * @retval 0 Clock configured successfully.
 * @retval -EINVAL Clock source frequency could not be obtained or the
 *                 requested pixel clock cannot be generated.
 */
static int lcd_rgb_configure_clock(struct lcd_rgb_poc_data *data)
{
	const soc_periph_lcd_clk_src_t clock_source = LCD_CLK_SRC_DEFAULT;
	uint32_t pclk_prescale;
	int ret;

	ret = esp_clk_tree_src_get_freq_hz(
		clock_source,
		ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX,
		&data->source_clock_hz);
	if (ret != ESP_OK) {
		LOG_ERR("Failed to get LCD clock source frequency (%d)", ret);
		return -EINVAL;
	}

	lcd_ll_select_clk_src(data->hal.dev, clock_source);
	lcd_ll_set_group_clock_coeff(data->hal.dev,
				     LCD_PERIPH_CLOCK_PRE_SCALE,
				     0,
				     0);

	data->lcd_clock_hz =
		data->source_clock_hz / LCD_PERIPH_CLOCK_PRE_SCALE;

	pclk_prescale =
		DIV_ROUND_UP(data->lcd_clock_hz, LCD_PIXEL_CLOCK_HZ);

	if ((pclk_prescale == 0U) ||
	    (pclk_prescale > LCD_LL_PCLK_DIV_MAX)) {
		LOG_ERR("Unsupported pixel clock divider %u", pclk_prescale);
		return -EINVAL;
	}

	lcd_ll_set_pixel_clock_prescale(data->hal.dev, pclk_prescale);

	data->pixel_clock_hz = data->lcd_clock_hz / pclk_prescale;

	LOG_INF("LCD clock source: %u Hz", data->source_clock_hz);
	LOG_INF("LCD peripheral clock: %u Hz", data->lcd_clock_hz);
	LOG_INF("RGB pixel clock: %u Hz", data->pixel_clock_hz);

	return 0;
}

/**
 * @brief Configure RGB signal timing.
 *
 * Timing values are taken from the VIEWE UEDX80480050E-WB-A panel
 * configuration:
 *
 * - 800 x 480 active area
 * - HSYNC: 1 pixel clock
 * - HBP: 42 pixel clocks
 * - HFP: 20 pixel clocks
 * - VSYNC: 10 lines
 * - VBP: 12 lines
 * - VFP: 4 lines
 *
 * HSYNC and VSYNC are active low. DE is active high.
 *
 * @param data RGB proof-of-concept runtime data.
 */
static void lcd_rgb_configure_timing(struct lcd_rgb_poc_data *data)
{
	lcd_hal_context_t *hal = &data->hal;

	lcd_ll_set_dma_read_stride(hal->dev, LCD_DATA_WIDTH);
	lcd_ll_set_data_wire_width(hal->dev, LCD_DATA_WIDTH);

	lcd_ll_set_horizontal_timing(hal->dev,
				     LCD_HSYNC_LEN,
				     LCD_HBACK_PORCH,
				     LCD_HACTIVE,
				     LCD_HFRONT_PORCH);

	lcd_ll_set_vertical_timing(hal->dev,
				   LCD_VSYNC_LEN,
				   LCD_VBACK_PORCH,
				   LCD_VACTIVE,
				   LCD_VFRONT_PORCH);

	/*
	 * HSYNC and VSYNC are active low, therefore their idle state is high.
	 * DE is active high, therefore its idle state is low.
	 */
	lcd_ll_set_idle_level(hal->dev, true, true, false);

	/*
	 * Vendor configuration uses the negative pixel clock edge.
	 * Keep this explicit during hardware validation.
	 */
	lcd_ll_set_pixel_clock_edge(hal->dev, true);

	lcd_ll_set_delay_ticks(hal->dev, 0, 0, 0);
	lcd_ll_set_data_delay_ticks(hal->dev, 0);
}

/**
 * @brief Initialize the LCD_CAM hardware block.
 *
 * @param data RGB proof-of-concept runtime data.
 *
 * @retval 0 Hardware initialized successfully.
 * @retval <0 Clock configuration failed.
 */
static int lcd_rgb_init_peripheral(struct lcd_rgb_poc_data *data)
{
	int ret;

	lcd_ll_enable_bus_clock(LCD_BUS_ID, true);
	lcd_ll_reset_register(LCD_BUS_ID);

//	lcd_hal_init(&data->hal, LCD_BUS_ID);
    data->hal.dev = LCD_LL_GET_HW(LCD_BUS_ID);

	lcd_ll_enable_clock(data->hal.dev, true);

	ret = lcd_rgb_configure_clock(data);
	if (ret < 0) {
		return ret;
	}

	lcd_ll_reset(data->hal.dev);
	lcd_ll_fifo_reset(data->hal.dev);

	lcd_ll_enable_rgb_mode(data->hal.dev, true);
	lcd_ll_enable_color_convert(data->hal.dev, false);

	/*
	 * Keep RGB timing signals running continuously between frame
	 * transfers.
	 */
	lcd_ll_enable_output_always_on(data->hal.dev, true);

	lcd_rgb_configure_timing(data);

	return 0;
}

int lcd_rgb_poc_init(void)
{
	int ret;

	LOG_INF("Initializing ESP32-S3 LCD_CAM RGB proof of concept");

	ret = lcd_rgb_init_peripheral(&lcd_data);
	if (ret < 0) {
		return ret;
	}

	LOG_INF("RGB timing configured: %ux%u", LCD_HACTIVE, LCD_VACTIVE);
	LOG_INF("LCD_CAM RGB peripheral initialized");
	LOG_WRN("RGB DMA stream is not started yet");

	return 0;
}
