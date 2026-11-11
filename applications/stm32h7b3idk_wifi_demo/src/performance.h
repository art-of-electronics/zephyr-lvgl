/**
 * @file performance.h
 * @brief LVGL performance monitoring and reporting.
 *
 * Provides functions for measuring LVGL handler execution time,
 * tracking rendered frames and animation updates, and displaying
 * performance statistics.
 */

#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include <stdbool.h>

#include <lvgl.h>

/**
 * @brief Initialize the performance monitor.
 *
 * @param[in] screen Parent LVGL object.
 * @param[in] enable_print_report Enable periodic performance logging.
 * 
 * @retval 0 Initialization completed successfully.
 * @retval -EINVAL Invalid screen pointer.
 * @retval -ENOMEM Failed to create the performance label.
 */
int performance_init(lv_obj_t *screen, bool enable_print_report);

/**
 * @brief Record LVGL timer handler execution time.
 *
 * @param[in] elapsed_cycles Number of CPU cycles spent in the handler.
 */
void performance_record_lvgl_handler(uint32_t elapsed_cycles);

/**
 * @brief Update and optionally log performance statistics.
 */
void performance_report(void);

/**
 * @brief Record a completed display frame.
 */
void performance_frame_record(void);

/**
 * @brief Record an animation update.
 */
void performance_animation_record(void);

#endif // PERFORMANCE_H
