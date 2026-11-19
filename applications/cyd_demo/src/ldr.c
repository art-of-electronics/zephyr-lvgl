/**
 * @file ldr.c
 * @brief ADC sampling and millivolt conversion for the onboard LDR.
 */

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

#include <ldr.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(ldr, LOG_LEVEL_INF);

#define LDR_NODE DT_PATH(zephyr_user)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
static const struct adc_dt_spec ldr_adc = ADC_DT_SPEC_GET(LDR_NODE);
static int16_t ldr_sample_buffer = 0;
static struct adc_sequence ldr_sequence = {0};


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/

/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int ldr_init(void)
{
    int ret = 0;

    if (true != adc_is_ready_dt(&ldr_adc)) {
        LOG_ERR("LDR ADC device is not ready");
        return -ENODEV;
    }

    ret = adc_channel_setup_dt(&ldr_adc);
    if (0 != ret) {
        LOG_ERR("LDR ADC channel setup failed: %d", ret);
        return ret;
    }

    ldr_sequence.buffer = &ldr_sample_buffer;
    ldr_sequence.buffer_size = sizeof(ldr_sample_buffer);

    ret = adc_sequence_init_dt(&ldr_adc, &ldr_sequence);
    if (0 != ret) {
        LOG_ERR("LDR ADC sequence init failed: %d", ret);
    }

    return ret;
}

int ldr_sample(void)
{
    int ret = 0;
    int32_t voltage_mv = 0;

    ldr_sample_buffer = 0;

    ret = adc_read_dt(&ldr_adc, &ldr_sequence);
    if (0 != ret) {
        LOG_ERR("LDR ADC read failed: %d", ret);
        return ret;
    }

    voltage_mv = (int32_t)ldr_sample_buffer;
    ret = adc_raw_to_millivolts_dt(&ldr_adc, &voltage_mv);
    if (0 != ret) {
        LOG_ERR("LDR voltage conversion failed: %d", ret);
        return ret;
    }

    LOG_INF("LDR: raw=%d, voltage=%ld mV", (int)ldr_sample_buffer, (long)voltage_mv);

    return 0;
}
