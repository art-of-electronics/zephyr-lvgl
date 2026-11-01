#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <demo_rgb.h>
#include <demo_lcd.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cyd_demo, LOG_LEVEL_INF);

#define LDR_NODE DT_PATH(zephyr_user)

static const struct adc_dt_spec ldr_adc = ADC_DT_SPEC_GET(LDR_NODE);;

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
    int ret = 0;
    int16_t sample = 0;
    struct adc_sequence sequence = {0};

    if (false == adc_is_ready_dt(&ldr_adc)) {
        LOG_ERR("LDR ADC device is not ready");
        return -ENODEV;
    }

    ret = adc_channel_setup_dt(&ldr_adc);
    if (0 != ret) {
        LOG_ERR("LDR ADC channel setup failed: %d", ret);
        return ret;
    }

    sequence.buffer = &sample;
    sequence.buffer_size = sizeof(sample);

    ret = adc_sequence_init_dt(&ldr_adc, &sequence);
    if (0 != ret) {
        LOG_ERR("LDR ADC sequence init failed: %d", ret);
        return ret;
    }

    (void)demo_rgb_start();
    (void)demo_lcd_start();

    while (true) {
        sample = 0;

        ret = adc_read_dt(&ldr_adc, &sequence);
        if (0 == ret) {
            int32_t voltage_mv = (int32_t)sample;

            ret = adc_raw_to_millivolts_dt(&ldr_adc, &voltage_mv);
            if (0 == ret) {
                LOG_INF("LDR: raw=%d, voltage=%ld mV", (int)sample, (long)voltage_mv);
            } else {
                LOG_ERR("LDR voltage conversion failed: %d", ret);
            }
        } else {
            LOG_ERR("LDR ADC read failed: %d", ret);
        }
        
        k_sleep(K_SECONDS(1));
    }
}
