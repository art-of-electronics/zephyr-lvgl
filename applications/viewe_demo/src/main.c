#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

#include "lcd_rgb_poc.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define BACKLIGHT_NODE DT_ALIAS(backlight_pwm)
#define BACKLIGHT_PWM_NODE DT_CHILD(BACKLIGHT_NODE, backlight)

static const struct pwm_dt_spec backlight = PWM_DT_SPEC_GET(BACKLIGHT_PWM_NODE);

static int set_brightness(uint32_t percent) {
	if (percent > 100U) {
		percent = 100U;
	}

	uint32_t pulse = (backlight.period * percent) / 100U;

	return pwm_set_pulse_dt(&backlight, pulse);
}

int main(void)
{
	LOG_INF("VIEWE RGB LCD smoke test");

	const struct device *display_dev =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(display_dev)) {
    	LOG_ERR("LCD RGB display device is not ready");
    	return -ENODEV;
	}

	struct display_capabilities caps;

	display_get_capabilities(display_dev, &caps);

	LOG_INF("Display device: %s", display_dev->name);
	LOG_INF("Resolution: %u x %u",
        caps.x_resolution,
        caps.y_resolution);
	LOG_INF("Pixel format: 0x%x", caps.current_pixel_format);

	int ret = lcd_rgb_poc_init();

	if (ret < 0) {
		LOG_ERR("LCD RGB init failed: %d", ret);
		return ret;
	}

	LOG_INF("LCD RGB started");

	if (!pwm_is_ready_dt(&backlight)) {
		LOG_ERR("Backlight PWM not ready");
		return -ENODEV;
	}

	set_brightness(35);
    LOG_INF("BACKLIGHT ON");

	while (1) {
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
