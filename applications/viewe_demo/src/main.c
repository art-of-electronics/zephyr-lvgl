#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "lcd_rgb_poc.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define BACKLIGHT_NODE DT_NODELABEL(lcd_backlight_en)
static const struct gpio_dt_spec backlight = GPIO_DT_SPEC_GET(BACKLIGHT_NODE, enable_gpios);


int main(void)
{
	LOG_INF("VIEWE RGB LCD smoke test");

	int ret = lcd_rgb_poc_init();

	if (ret < 0) {
		LOG_ERR("LCD RGB init failed: %d", ret);
		return ret;
	}

	LOG_INF("LCD RGB started");

    if (!gpio_is_ready_dt(&backlight)) {
        return -ENODEV;
    }

    if (gpio_pin_configure_dt(&backlight, GPIO_OUTPUT_INACTIVE) < 0) {
        return -EIO;
    }

    gpio_pin_set_dt(&backlight, 1);
    LOG_INF("BACKLIGHT ON");

	while (1) {
		k_sleep(K_SECONDS(1));
	}

	return 0;
}

/*
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define BACKLIGHT_NODE DT_NODELABEL(lcd_backlight_en)
static const struct gpio_dt_spec backlight = GPIO_DT_SPEC_GET(BACKLIGHT_NODE, enable_gpios);

#define LED_ON_STACK_SIZE   256
#define THREAD_PRIORITY     5

static void led_on_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        // gpio_pin_set_dt(&backlight, 0);
        printk("BACKLIGHT OFF\n");
        k_sleep(K_MSEC(2000));

        // gpio_pin_set_dt(&backlight, 1);
        printk("BACKLIGHT ON\n");
        k_sleep(K_MSEC(2000));
    }
}

K_THREAD_DEFINE(led_on_tid, LED_ON_STACK_SIZE, led_on_thread,
                NULL, NULL, NULL, THREAD_PRIORITY, 0, 0);

int main(void)
{
    if (!gpio_is_ready_dt(&backlight)) {
        return -ENODEV;
    }

    if (gpio_pin_configure_dt(&backlight, GPIO_OUTPUT_INACTIVE) < 0) {
        return -EIO;
    }

    return 0;
}
*/
/*
#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>

#define BACKLIGHT_NODE DT_NODELABEL(backlight_pwm)

static const struct pwm_dt_spec backlight = PWM_DT_SPEC_GET(BACKLIGHT_NODE);

static int set_brightness(uint32_t percent) {
	if (percent > 100U) {
		percent = 100U;
	}

	uint32_t pulse = (backlight.period * percent) / 100U;

	return pwm_set_pulse_dt(&backlight, pulse);
}

int main(void) {
	if (!pwm_is_ready_dt(&backlight)) {
		printk("Backlight PWM not ready\n");
		return -ENODEV;
	}

	printk("PWM period: %u ns\n", backlight.period);

	while (1) {
		for (uint32_t brightness = 0; brightness <= 100; ++brightness) {
			set_brightness(brightness);
			k_sleep(K_MSEC(15));
		}

		k_sleep(K_MSEC(300));

		for (int brightness = 100; brightness >= 0; --brightness) {
			set_brightness((uint32_t)brightness);
			k_sleep(K_MSEC(15));
		}

		k_sleep(K_MSEC(300));
	}

	return 0;
}
*/