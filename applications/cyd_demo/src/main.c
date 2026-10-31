#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <demo_rgb.h>
#include <demo_lcd.h>
#include <demo_touch.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cyd_demo, LOG_LEVEL_INF);

static struct k_thread demo_rgb_thread_data = {0};
static struct k_thread demo_lcd_thread_data = {0};

K_THREAD_STACK_DEFINE(demo_rgb_stack, THREAD_DEMO_RGB_STACK_SIZE);
K_THREAD_STACK_DEFINE(demo_lcd_stack, THREAD_DEMO_LCD_STACK_SIZE);

static void demo_rgb_task(void *arg1, void*arg2, void *arg3) {
    (void)arg1;
    (void)arg2;
    (void)arg3;

    int ret = 0;

    ret = demo_rgb_init();
    if (0 != ret) {
        LOG_ERR("RGB Led init() fail %d", ret);
        return;
    }

    ret = demo_rgb_set_rgb(0U, 0U, 0U);
    if (0 != ret) {
        LOG_ERR("RGB Led set_rgb() fail %d", ret);
        return;
    }

    while (1) {
        demo_rgb_loop();
    }
}

static void demo_lcd_task(void *arg1, void*arg2, void *arg3) {
    (void)arg1;
    (void)arg2;
    (void)arg3;

    int ret = 0;

    ret = demo_lcd_pwm_init();
    if (0 != ret) {
        LOG_ERR("LCD pwm_init() fail %d", ret);
        return;
    }

    ret = demo_lcd_set_brightness(35U);
    if (0 != ret) {
        LOG_ERR("LCD set_brightness() fail %d", ret);
        return;
    }

    ret = demo_lcd_init_lcd();
    if (0 != ret) {
        LOG_ERR("LCD init() fail %d", ret);
        return;
    }

    ret = demo_touch_init();
    if (0 != ret) {
        LOG_ERR("Touch init() fail %d", ret);
        return;
    }

    while (1) {
        demo_lcd_loop();
    }
}

int demo_rgb_start(void) {
	k_tid_t tid;

	tid = k_thread_create(&demo_rgb_thread_data, demo_rgb_stack,
			              K_THREAD_STACK_SIZEOF(demo_rgb_stack), demo_rgb_task, 
                          NULL, NULL, NULL, THREAD_DEMO_RGB_PRIORITY, 0, K_NO_WAIT);

	if (NULL == tid) {
		return -EIO;
	}

	k_thread_name_set(tid, "demo_rgb");

	return 0;
}

int demo_lcd_start(void) {
	k_tid_t tid;

	tid = k_thread_create(&demo_lcd_thread_data, demo_lcd_stack,
			              K_THREAD_STACK_SIZEOF(demo_lcd_stack), demo_lcd_task, 
                          NULL, NULL, NULL, THREAD_DEMO_LCD_PRIORITY, 0, K_NO_WAIT);

	if (NULL == tid) {
		return -EIO;
	}

	k_thread_name_set(tid, "demo_lcd");

	return 0;
}


int main(void)
{
    (void)demo_rgb_start();
    (void)demo_lcd_start();

    while (true) {
        k_sleep(K_SECONDS(1));
    }
}
