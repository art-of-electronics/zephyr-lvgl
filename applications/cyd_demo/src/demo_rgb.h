#ifndef DEMO_RGB_H
#define DEMO_RGB_H

#include <stdint.h>

#define THREAD_DEMO_RGB_STACK_SIZE  (512)
#define THREAD_DEMO_RGB_PRIORITY    (5)

int demo_rgb_init(void);

int demo_rgb_set_rgb(uint32_t red, uint32_t green, uint32_t blue);

void demo_rgb_loop(void);

#endif //DEMO_RGB_H
