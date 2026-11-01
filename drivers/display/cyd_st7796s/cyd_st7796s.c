#include "cyd_st7796s.h"

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cyd_st7796s, LOG_LEVEL_INF);

#define LCD_NODE            DT_CHOSEN(zephyr_display)

#define LCD_CMD_COLMOD      (0x3AU)
#define LCD_CMD_MADCTL      (0x36U)
#define LCD_COLMOD_RGB565   (0x05U)
#define LCD_CMD_PGC         (0xE0U)
#define LCD_CMD_NGC         (0xE1U)
#define LCD_CMD_CSCON       (0xF0U)
#define LCD_UNLOCK_1        (0xC3U)
#define LCD_UNLOCK_2        (0x96U)
#define LCD_LOCK_1          (0x3CU)
#define LCD_LOCK_2          (0x69U)
#define LCD_CMD_INVON       (0x21U)
#define LCD_CMD_INVOFF      (0x20U)
#define LCD_CMD_FRMCTR1     (0xB1U)
#define LCD_CMD_BPC         (0xB5U)
#define LCD_CMD_DFC         (0xB6U)
#define LCD_CMD_PWR1        (0xC0U)
#define LCD_CMD_PWR2        (0xC1U)
#define LCD_CMD_PWR3        (0xC2U)
#define LCD_CMD_VCMPCTL     (0xC5U)
#define LCD_CMD_FRMCTR2     (0xB2U)
#define LCD_CMD_FRMCTR3     (0xB3U)
#define LCD_CMD_DOCA        (0xE8U)
#define LCD_CMD_DIC         (0xB4U)

static const struct device *const lcd_dbi = DEVICE_DT_GET(DT_PARENT(LCD_NODE));
static const struct mipi_dbi_config lcd_dbi_config = MIPI_DBI_CONFIG_DT(LCD_NODE, SPI_OP_MODE_CONTROLLER | SPI_WORD_SET(8), 0);
static const uint8_t lcd_pgc[] = DT_PROP(LCD_NODE, pgc);
static const uint8_t lcd_ngc[] = DT_PROP(LCD_NODE, ngc);
static const uint8_t lcd_bpc[] = DT_PROP(LCD_NODE, bpc);
static const uint8_t lcd_dfc[] = DT_PROP(LCD_NODE, dfc);
static const uint8_t lcd_pwr1[] = DT_PROP(LCD_NODE, pwr1);
static const uint8_t lcd_pwr2 = (uint8_t)DT_PROP(LCD_NODE, pwr2);
static const uint8_t lcd_pwr3 = (uint8_t)DT_PROP(LCD_NODE, pwr3);
static const uint8_t lcd_vcmpctl = (uint8_t)DT_PROP(LCD_NODE, vcmpctl);
static const uint8_t lcd_dic = (uint8_t)DT_ENUM_IDX(LCD_NODE, invert_mode);
static const uint8_t lcd_frmctl1[] = DT_PROP(LCD_NODE, frmctl1);
static const uint8_t lcd_frmctl2[] = DT_PROP(LCD_NODE, frmctl2);
static const uint8_t lcd_frmctl3[] = DT_PROP(LCD_NODE, frmctl3);
static const uint8_t lcd_doca[] = DT_PROP(LCD_NODE, doca);

static const uint8_t lcd_unlock_1 = LCD_UNLOCK_1;
static const uint8_t lcd_unlock_2 = LCD_UNLOCK_2;
static const uint8_t lcd_lock_1 = LCD_LOCK_1;
static const uint8_t lcd_lock_2 = LCD_LOCK_2;
static const uint8_t lcd_colmod = LCD_COLMOD_RGB565;
const uint8_t lcd_madctl = (uint8_t)DT_PROP(LCD_NODE, madctl);

struct lcd_init_command {
    uint8_t command;
    const uint8_t *data;
    size_t length;
};

#define LCD_INIT_ARRAY(cmd, data_array) \
    { (cmd), (data_array), sizeof(data_array) }

#define LCD_INIT_VALUE(cmd, value) \
    { (cmd), &(value), sizeof(value) }

#define LCD_INIT_NO_DATA(cmd) \
    { (cmd), NULL, 0U }

static const struct lcd_init_command lcd_init_sequence[] = {
    LCD_INIT_VALUE(LCD_CMD_CSCON, lcd_unlock_1),
    LCD_INIT_VALUE(LCD_CMD_CSCON, lcd_unlock_2),
    LCD_INIT_VALUE(LCD_CMD_DIC, lcd_dic),
    LCD_INIT_ARRAY(LCD_CMD_FRMCTR1, lcd_frmctl1),
    LCD_INIT_ARRAY(LCD_CMD_FRMCTR2, lcd_frmctl2),
    LCD_INIT_ARRAY(LCD_CMD_FRMCTR3, lcd_frmctl3),
    LCD_INIT_ARRAY(LCD_CMD_BPC, lcd_bpc),
    LCD_INIT_ARRAY(LCD_CMD_DFC, lcd_dfc),
    LCD_INIT_ARRAY(LCD_CMD_PWR1, lcd_pwr1),
    LCD_INIT_VALUE(LCD_CMD_PWR2, lcd_pwr2),
    LCD_INIT_VALUE(LCD_CMD_PWR3, lcd_pwr3),
    LCD_INIT_VALUE(LCD_CMD_VCMPCTL, lcd_vcmpctl),
    LCD_INIT_ARRAY(LCD_CMD_DOCA, lcd_doca),
    LCD_INIT_ARRAY(LCD_CMD_PGC, lcd_pgc),
    LCD_INIT_ARRAY(LCD_CMD_NGC, lcd_ngc),
    LCD_INIT_VALUE(LCD_CMD_CSCON, lcd_lock_1),
    LCD_INIT_VALUE(LCD_CMD_CSCON, lcd_lock_2),
    LCD_INIT_NO_DATA((true == DT_PROP(LCD_NODE, color_invert)) ? (LCD_CMD_INVON) : (LCD_CMD_INVOFF)),
    LCD_INIT_VALUE(LCD_CMD_COLMOD, lcd_colmod),
    LCD_INIT_VALUE(LCD_CMD_MADCTL, lcd_madctl),
};

static int cyd_st7796s_send_cmd(uint8_t cmd, const uint8_t *data, size_t len)
{
    return mipi_dbi_command_write(lcd_dbi, &lcd_dbi_config, cmd, data, len);
}

int cyd_st7796s_configure(void)
{
    int ret = 0;

    for (size_t index = 0U; index < (sizeof(lcd_init_sequence) / sizeof(lcd_init_sequence[0])); ++index) {
        const struct lcd_init_command *const entry = &lcd_init_sequence[index];

        ret = cyd_st7796s_send_cmd(entry->command, entry->data, entry->length);
        if (0 != ret) {
            LOG_ERR("Panel init failed at step %u, command 0x%02X: %d", 
                    (unsigned int)index, (unsigned int)entry->command, ret);
            return ret;
        }
    }

    LOG_INF("ST7796S panel configured");
    return 0;
}
