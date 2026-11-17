/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <morph_ball.h>
#include <performance.h>
#include <theme.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(morph_ball, LOG_LEVEL_INF);

#define PI_APPROX_MILLI (3142U)
#define MILLI_SCALE     (1000U)
#define MAX_DELTA_MS    (100U)

#define TIMER_PERIOD_MS (16U)
#define ANGLE_FULL_TURN (3600U)

#define FLOOR_OFFSET_PX (34U)
#define FLOOR_HEIGHT_PX (3U)
#define BALL_SIZE_PX    (64U)
#define BALL_RADIUS_PX  ((BALL_SIZE_PX) / 2U)
#define BALL_CIRCUMFERENCE_MILLI (2U * BALL_RADIUS_PX * PI_APPROX_MILLI)

#define BALL_SPEED_MIN_PX_S     (60U)
#define BALL_SPEED_MAX_PX_S     (180U)
#define BALL_ACCEL_PX_S2        (240U)
#define BALL_DECEL_PX_S2        (360U)
#define BALL_BRAKING_ZONE_PX    (96U)

#define GLOW_WIDTH_PX      (48)
#define GLOW_HEIGHT_PX     (8)
#define GLOW_OFFSET_Y_PX   (3)
#define GLOW_PULSE_STEP    (2U)
#define GLOW_OPA_MIN       (40U)
#define GLOW_OPA_MAX       (180U)

#define IMPACT_DURATION_MS (250U)
#define IMPACT_RECOLOR_MAX (180U)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
LV_IMAGE_DECLARE(morph_ball);

static lv_obj_t *ball = NULL;
static int32_t ball_x_milli = 0;
static int32_t ball_y = 0;
static int32_t direction = 1;
static int32_t angle_tenths = 0;
static uint32_t previous_ms = 0U;
static int32_t max_x = 0;
static int32_t ball_speed_milli = (int32_t)(BALL_SPEED_MIN_PX_S * MILLI_SCALE);

static lv_obj_t *glow = NULL;
static uint8_t glow_opacity = GLOW_OPA_MIN;
static int32_t glow_direction = 1;

static uint32_t impact_remaining_ms = 0U;


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static void on_touch(lv_event_t *event)
{
    ARG_UNUSED(event);
    direction = -direction;
}

static void update_ball_glow(void)
{
    int32_t glow_x = (ball_x_milli / MILLI_SCALE) + ((BALL_SIZE_PX - GLOW_WIDTH_PX) / 2U);
    int32_t glow_y = ball_y + BALL_SIZE_PX - GLOW_OFFSET_Y_PX;

    lv_obj_set_pos(glow, glow_x, glow_y);

    if (0 < glow_direction) {
        if (GLOW_OPA_MAX <= glow_opacity + GLOW_PULSE_STEP) {
            glow_opacity = GLOW_OPA_MAX;
            glow_direction = -1;
        } else {
            glow_opacity += GLOW_PULSE_STEP;
        }
    } else {
        if (GLOW_OPA_MIN >= glow_opacity - GLOW_PULSE_STEP) {
            glow_opacity = GLOW_OPA_MIN;
            glow_direction = 1;
        } else {
            glow_opacity -= GLOW_PULSE_STEP;
        }
    }

    lv_obj_set_style_bg_opa(glow, glow_opacity, 0);
}

static int32_t get_target_speed(void)
{
    int32_t distance_px = 0;
    int32_t target_speed = (int32_t)BALL_SPEED_MAX_PX_S;

    if (0 < direction) {
        distance_px = max_x - (ball_x_milli / (int32_t)MILLI_SCALE);
    } else {
        distance_px = ball_x_milli / (int32_t)MILLI_SCALE;
    }

    if (0 > distance_px) {
        distance_px = 0;
    }

    if ((int32_t)BALL_BRAKING_ZONE_PX > distance_px) {
        target_speed = (int32_t)BALL_SPEED_MIN_PX_S +
                       (((int32_t)(BALL_SPEED_MAX_PX_S - BALL_SPEED_MIN_PX_S) * distance_px) / 
                        (int32_t)BALL_BRAKING_ZONE_PX);
    }

    return target_speed * (int32_t)MILLI_SCALE;
}

static void update_ball_speed(uint32_t delta_ms)
{
    int32_t target_speed = get_target_speed();
    int32_t speed_step = 0;

    if (ball_speed_milli < target_speed) {
        speed_step = (int32_t)(BALL_ACCEL_PX_S2 * delta_ms);

        ball_speed_milli += speed_step;

        if (target_speed < ball_speed_milli) {
            ball_speed_milli = target_speed;
        }
    } else if (ball_speed_milli > target_speed) {
        speed_step = (int32_t)(BALL_DECEL_PX_S2 * delta_ms);

        ball_speed_milli -= speed_step;

        if (target_speed > ball_speed_milli) {
            ball_speed_milli = target_speed;
        }
    }
}

static void trigger_impact(void)
{
    impact_remaining_ms = IMPACT_DURATION_MS;

    lv_obj_set_style_image_recolor_opa(ball, (lv_opa_t)IMPACT_RECOLOR_MAX, 0);
}

static void update_impact_effect(uint32_t delta_ms)
{
    uint32_t opacity = 0U;

    if (0U == impact_remaining_ms) {
        return;
    }

    if (impact_remaining_ms <= delta_ms) {
        impact_remaining_ms = 0U;

        lv_obj_set_style_image_recolor_opa(ball, LV_OPA_TRANSP, 0);

        return;
    }

    impact_remaining_ms -= delta_ms;
    opacity = (IMPACT_RECOLOR_MAX * impact_remaining_ms) / IMPACT_DURATION_MS;

    lv_obj_set_style_image_recolor_opa(ball, (lv_opa_t)opacity, 0);
}


static void animate(lv_timer_t *timer)
{
    ARG_UNUSED(timer);
    performance_animation_record();
    uint32_t now = lv_tick_get();
    uint32_t delta_ms = now - previous_ms;
    previous_ms = now;
    if (MAX_DELTA_MS < delta_ms) {
        delta_ms = MAX_DELTA_MS;
    }

    update_ball_speed(delta_ms);

    int32_t dx_milli = (int32_t)(((int64_t)direction * ball_speed_milli * 
                                  (int32_t)delta_ms) / (int32_t)MILLI_SCALE);
    ball_x_milli += dx_milli;

    if (0 > ball_x_milli) {
        ball_x_milli = -ball_x_milli;
        direction = 1;
        ball_speed_milli = (int32_t)(BALL_SPEED_MIN_PX_S * MILLI_SCALE);
        trigger_impact();
    } else if ((max_x * (int32_t)MILLI_SCALE) < ball_x_milli) {
        ball_x_milli = (2 * max_x * (int32_t)MILLI_SCALE) - ball_x_milli;
        direction = -1;
        ball_speed_milli = (int32_t)(BALL_SPEED_MIN_PX_S * MILLI_SCALE);
        trigger_impact();
    }

    angle_tenths = (int32_t)(((int64_t)ball_x_milli * ANGLE_FULL_TURN) / (BALL_CIRCUMFERENCE_MILLI));
    angle_tenths %= ANGLE_FULL_TURN;
    if (0 > angle_tenths) {
        angle_tenths += ANGLE_FULL_TURN;
    }

    lv_obj_set_pos(ball, (ball_x_milli / (MILLI_SCALE)), ball_y);
    lv_image_set_rotation(ball, angle_tenths);

    update_ball_glow();
    update_impact_effect(delta_ms);
}


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int morph_ball_init(lv_obj_t *screen)
{
    if (NULL == screen) {
        return -EINVAL;
    }

    lv_display_t *display = lv_obj_get_display(screen);
    if (NULL == display) {
        return -ENODEV;
    }

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "MORPH BALL");
    lv_obj_set_style_text_color(title, lv_color_hex(SAMUS_YELLOW), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *floor = lv_obj_create(screen);
    lv_obj_set_size(floor, lv_display_get_horizontal_resolution(display), FLOOR_HEIGHT_PX);
    lv_obj_set_pos(floor, 0, lv_display_get_vertical_resolution(display) - FLOOR_OFFSET_PX);
    lv_obj_set_style_bg_color(floor, lv_color_hex(CHOZO_GOLD), 0);
    lv_obj_set_style_border_width(floor, 0, 0);
    lv_obj_set_style_radius(floor, 0, 0);
    lv_obj_set_scrollable(floor, false);

    ball = lv_image_create(screen);
    lv_image_set_src(ball, &morph_ball);
    lv_image_set_pivot(ball, BALL_RADIUS_PX, BALL_RADIUS_PX);
    lv_obj_set_clickable(ball, true);
    lv_obj_add_event_cb(ball, on_touch, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_image_recolor(ball, lv_color_hex(SAMUS_YELLOW), 0);
    lv_obj_set_style_image_recolor_opa(ball, LV_OPA_TRANSP, 0);

    glow = lv_obj_create(screen);
    lv_obj_set_size(glow, GLOW_WIDTH_PX, GLOW_HEIGHT_PX);
    lv_obj_set_style_bg_color(glow, lv_color_hex(SAMUS_GREEN), 0);
    lv_obj_set_style_bg_opa(glow, glow_opacity, 0);
    lv_obj_set_style_border_width(glow, 0, 0);
    lv_obj_set_style_outline_width(glow, 0, 0);
    lv_obj_set_style_pad_all(glow, 0, 0);
    lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_scrollable(glow, false);
    lv_obj_set_clickable(glow, false);

    return 0;
}

int morph_ball_run(void)
{
    lv_display_t *display = lv_obj_get_display(ball);
    if (NULL == display) {
        return -ENODEV;
    }

    max_x = lv_display_get_horizontal_resolution(display) - BALL_SIZE_PX;
    ball_y = lv_display_get_vertical_resolution(display) - FLOOR_OFFSET_PX - BALL_SIZE_PX;

    ball_x_milli = 0;
    previous_ms = lv_tick_get();

    lv_obj_set_pos(ball, 0, ball_y);

    lv_timer_create(animate, TIMER_PERIOD_MS, NULL);

    LOG_INF("Morph Ball demo started");
    return 0;
}
