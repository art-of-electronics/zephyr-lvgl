#ifndef DEMO_LCD_H
#define DEMO_LCD_H

#include <stdint.h>

#define THREAD_DEMO_LCD_STACK_SIZE  (8192)
#define THREAD_DEMO_LCD_PRIORITY    (5)

int demo_lcd_pwm_init(void);

int demo_lcd_set_brightness(uint32_t brightness);

int demo_lcd_init_lcd(void);

void demo_lcd_loop(void);

#endif //DEMO_LCD_H
