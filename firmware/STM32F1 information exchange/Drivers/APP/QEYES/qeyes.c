#include "qeyes.h"
#include "./SYSTEM/usart/usart.h"

#define QEYES_W                 HUB75_PANEL_WIDTH
#define QEYES_H                 HUB75_PANEL_HEIGHT

#define LEFT_EYE_X              32
#define RIGHT_EYE_X             96
#define EYE_Y                   16

#define BLINK_CLOSED_MS         160u
#define TEAR_FRAME_MS           BLINK_CLOSED_MS
#define TEAR_CYCLE_STEPS        18u
#define TEAR_RIGHT_PHASE        9u

static uint16_t C_BG;
static uint16_t C_WHITE;
static uint16_t C_IRIS;
static uint16_t C_PUPIL;
static uint16_t C_PINK;
static uint16_t C_RED;
static uint16_t C_ORANGE;
static uint16_t C_TEAR;

static void frame_clear(uint16_t color)
{
    hub75_fill_screen(color);
}

static void pixel(int16_t x, int16_t y, uint16_t color)
{
    if ((x >= 0) && (x < (int16_t)QEYES_W) && (y >= 0) && (y < (int16_t)QEYES_H)) {
        hub75_draw_pixel565((uint16_t)x, (uint16_t)y, color);
    }
}

static void fill_rect(int16_t x0, int16_t y0, int16_t w, int16_t h, uint16_t color)
{
    for (int16_t y = y0; y < (int16_t)(y0 + h); y++) {
        for (int16_t x = x0; x < (int16_t)(x0 + w); x++) {
            pixel(x, y, color);
        }
    }
}

static void fill_ellipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color)
{
    int32_t rx2;
    int32_t ry2;
    int32_t rxy;

    if ((rx <= 0) || (ry <= 0)) {
        return;
    }

    rx2 = (int32_t)rx * rx;
    ry2 = (int32_t)ry * ry;
    rxy = rx2 * ry2;

    for (int16_t y = (int16_t)-ry; y <= ry; y++) {
        for (int16_t x = (int16_t)-rx; x <= rx; x++) {
            int32_t v = (int32_t)x * x * ry2 + (int32_t)y * y * rx2;
            if (v <= rxy) {
                pixel((int16_t)(cx + x), (int16_t)(cy + y), color);
            }
        }
    }
}

static void draw_disc(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
    fill_ellipse(cx, cy, r, r, color);
}

static void draw_thick_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t thick, uint16_t color)
{
    int16_t dx = (x1 > x0) ? (int16_t)(x1 - x0) : (int16_t)(x0 - x1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy = (y1 > y0) ? (int16_t)(y0 - y1) : (int16_t)(y1 - y0);
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = (int16_t)(dx + dy);

    while (1) {
        draw_disc(x0, y0, thick, color);
        if ((x0 == x1) && (y0 == y1)) {
            break;
        }

        int16_t e2 = (int16_t)(2 * err);
        if (e2 >= dy) {
            err = (int16_t)(err + dy);
            x0 = (int16_t)(x0 + sx);
        }

        if (e2 <= dx) {
            err = (int16_t)(err + dx);
            y0 = (int16_t)(y0 + sy);
        }
    }
}

static void draw_soft_blush(int16_t cx)
{
    fill_ellipse((int16_t)(cx - 18), 27, 3, 2, C_PINK);
    fill_ellipse((int16_t)(cx + 18), 27, 3, 2, C_PINK);
}

static void draw_heart(int16_t cx, int16_t cy, uint16_t color)
{
    int16_t y;

    fill_ellipse((int16_t)(cx - 7), (int16_t)(cy - 4), 7, 6, color);
    fill_ellipse((int16_t)(cx + 7), (int16_t)(cy - 4), 7, 6, color);

    for (y = -4; y <= 13; y++) {
        int16_t half;

        if (y < 2) {
            half = 15;
        } else {
            half = (int16_t)(17 - y);
        }

        if (half > 0) {
            fill_rect((int16_t)(cx - half), (int16_t)(cy + y), (int16_t)(half * 2 + 1), 1, color);
        }
    }

    draw_disc((int16_t)(cx - 5), (int16_t)(cy - 6), 2, C_WHITE);
}

static void draw_eye_open_amount(int16_t cx, int16_t cy, int16_t open_ry, int16_t look_x, int16_t look_y)
{
    if (open_ry <= 2) {
        draw_thick_line((int16_t)(cx - 16), cy, (int16_t)(cx + 16), cy, 2, C_WHITE);
        return;
    }

    fill_ellipse(cx, cy, 17, open_ry, C_WHITE);

    if (open_ry >= 8) {
        int16_t iris_ry = (open_ry > 10) ? 9 : (int16_t)(open_ry - 1);
        int16_t pupil_ry = (open_ry > 9) ? 6 : (int16_t)(open_ry - 3);

        fill_ellipse((int16_t)(cx + look_x), (int16_t)(cy + look_y), 8, iris_ry, C_IRIS);
        fill_ellipse((int16_t)(cx + look_x), (int16_t)(cy + look_y), 4, pupil_ry, C_PUPIL);
        draw_disc((int16_t)(cx + look_x - 3), (int16_t)(cy + look_y - 4), 2, C_WHITE);
        draw_disc((int16_t)(cx + look_x + 2), (int16_t)(cy + look_y - 1), 1, C_WHITE);
    }
}

static void draw_eye_open(int16_t cx, int16_t cy, int16_t look_x, int16_t look_y)
{
    draw_eye_open_amount(cx, cy, 13, look_x, look_y);
}

static void draw_eye_closed(int16_t cx, int16_t cy)
{
    draw_thick_line((int16_t)(cx - 16), (int16_t)(cy - 1), (int16_t)(cx - 5), (int16_t)(cy + 2), 2, C_WHITE);
    draw_thick_line((int16_t)(cx - 5), (int16_t)(cy + 2), (int16_t)(cx + 8), (int16_t)(cy + 2), 2, C_WHITE);
    draw_thick_line((int16_t)(cx + 8), (int16_t)(cy + 2), (int16_t)(cx + 16), (int16_t)(cy - 1), 2, C_WHITE);
}

static void draw_eye_squint(int16_t cx, int16_t cy)
{
    draw_thick_line((int16_t)(cx - 16), (int16_t)(cy + 4), (int16_t)(cx - 5), (int16_t)(cy - 2), 2, C_WHITE);
    draw_thick_line((int16_t)(cx - 5), (int16_t)(cy - 2), (int16_t)(cx + 6), (int16_t)(cy - 2), 2, C_WHITE);
    draw_thick_line((int16_t)(cx + 6), (int16_t)(cy - 2), (int16_t)(cx + 16), (int16_t)(cy + 4), 2, C_WHITE);
}

static void draw_angry_brow_left(int16_t cx)
{
    draw_thick_line((int16_t)(cx - 17), 6, (int16_t)(cx + 14), 13, 2, C_RED);
    draw_thick_line((int16_t)(cx - 17), 7, (int16_t)(cx + 14), 14, 1, C_ORANGE);
}

static void draw_angry_brow_right(int16_t cx)
{
    draw_thick_line((int16_t)(cx - 14), 13, (int16_t)(cx + 17), 6, 2, C_RED);
    draw_thick_line((int16_t)(cx - 14), 14, (int16_t)(cx + 17), 7, 1, C_ORANGE);
}

static void draw_sad_brow_left(int16_t cx)
{
    draw_thick_line((int16_t)(cx - 15), 9, (int16_t)(cx + 13), 5, 2, C_WHITE);
}

static void draw_sad_brow_right(int16_t cx)
{
    draw_thick_line((int16_t)(cx - 13), 5, (int16_t)(cx + 15), 9, 2, C_WHITE);
}

static void draw_falling_tear(int16_t x, uint8_t phase)
{
    int16_t drop_y;

    /* The drop starts attached to the outer eye corner, then moves down.
     * Drawing is clipped by pixel(), so it naturally disappears below the
     * screen before restarting at the corner. */
    drop_y = (int16_t)(22 + phase);

    /* Keep the eye corner visibly wet while the falling drop separates. */
    fill_ellipse(x, 20, 1, 2, C_TEAR);

    if (phase < 5u) {
        fill_rect(x, 20, 1, (int16_t)(phase + 2u), C_TEAR);
    }

    fill_ellipse(x, drop_y, 2, 3, C_TEAR);
    pixel((int16_t)(x - 1), (int16_t)(drop_y - 1), C_WHITE);
}

void qeyes_show_closed(void)
{
    frame_clear(C_BG);
    draw_eye_closed(LEFT_EYE_X, EYE_Y);
    draw_eye_closed(RIGHT_EYE_X, EYE_Y);
    draw_soft_blush(LEFT_EYE_X);
    draw_soft_blush(RIGHT_EYE_X);
}

void qeyes_show_open(void)
{
    frame_clear(C_BG);
    draw_eye_open(LEFT_EYE_X, EYE_Y, 0, 0);
    draw_eye_open(RIGHT_EYE_X, EYE_Y, 0, 0);
    draw_soft_blush(LEFT_EYE_X);
    draw_soft_blush(RIGHT_EYE_X);
}

void qeyes_show_heart(void)
{
    frame_clear(C_BG);

    draw_heart(LEFT_EYE_X, EYE_Y, C_PINK);
    draw_heart(RIGHT_EYE_X, EYE_Y, C_PINK);

    fill_ellipse(LEFT_EYE_X, 28, 5, 2, C_RED);
    fill_ellipse(RIGHT_EYE_X, 28, 5, 2, C_RED);
}

static void qeyes_show_open_look(uint32_t elapsed)
{
    int16_t look_x = 0;
    int16_t look_y = 0;

    switch ((elapsed / 900u) % 6u) {
    case 0u:
        look_x = 0; look_y = 0;
        break;
    case 1u:
        look_x = 2; look_y = 0;
        break;
    case 2u:
        look_x = 1; look_y = 1;
        break;
    case 3u:
        look_x = -1; look_y = 0;
        break;
    case 4u:
        look_x = -2; look_y = 1;
        break;
    default:
        look_x = 0; look_y = -1;
        break;
    }

    frame_clear(C_BG);
    draw_eye_open(LEFT_EYE_X, EYE_Y, look_x, look_y);
    draw_eye_open(RIGHT_EYE_X, EYE_Y, look_x, look_y);
    draw_soft_blush(LEFT_EYE_X);
    draw_soft_blush(RIGHT_EYE_X);
}


void qeyes_show_confused(void)
{
    frame_clear(C_BG);

    draw_eye_open(LEFT_EYE_X, EYE_Y, 4, -1);
    draw_eye_open(RIGHT_EYE_X, EYE_Y, -4, 1);

    draw_thick_line(14, 7, 48, 5, 2, C_WHITE);
    draw_thick_line(80, 5, 114, 9, 2, C_WHITE);


    draw_soft_blush(LEFT_EYE_X);
    draw_soft_blush(RIGHT_EYE_X);
}

static void qeyes_show_tear_frame(uint32_t elapsed)
{
    uint8_t step;
    uint8_t left_phase;
    uint8_t right_phase;

    step = (uint8_t)((elapsed / TEAR_FRAME_MS) % TEAR_CYCLE_STEPS);
    left_phase = step;
    right_phase = (uint8_t)((step + TEAR_RIGHT_PHASE) % TEAR_CYCLE_STEPS);

    frame_clear(C_BG);

    draw_eye_open_amount(LEFT_EYE_X, EYE_Y, 11, 1, 2);
    draw_eye_open_amount(RIGHT_EYE_X, EYE_Y, 11, -1, 2);
    draw_sad_brow_left(LEFT_EYE_X);
    draw_sad_brow_right(RIGHT_EYE_X);

    /* Left and right drops are deliberately staggered for continuous crying. */
    draw_falling_tear((int16_t)(LEFT_EYE_X - 18), left_phase);
    draw_falling_tear((int16_t)(RIGHT_EYE_X + 18), right_phase);
}

void qeyes_show_tear(void)
{
    qeyes_show_tear_frame(0u);
}

void qeyes_show_squint(void)
{
    frame_clear(C_BG);

    draw_eye_squint(LEFT_EYE_X, EYE_Y);
    draw_eye_squint(RIGHT_EYE_X, EYE_Y);

    fill_ellipse(LEFT_EYE_X, 27, 4, 2, C_PINK);
    fill_ellipse(RIGHT_EYE_X, 27, 4, 2, C_PINK);
}

void qeyes_show_angry(void)
{
    frame_clear(C_BG);

    draw_eye_open_amount(LEFT_EYE_X, EYE_Y + 1, 10, 3, 0);
    draw_eye_open_amount(RIGHT_EYE_X, EYE_Y + 1, 10, -3, 0);

    draw_angry_brow_left(LEFT_EYE_X);
    draw_angry_brow_right(RIGHT_EYE_X);

    fill_rect(0, 0, 5, 32, C_RED);
    fill_rect(123, 0, 5, 32, C_RED);
}

void qeyes_init(void)
{
    C_BG = HUB75_COLOR_BLACK;
    C_WHITE = HUB75_COLOR_WHITE;
    C_IRIS = hub75_color565(80, 190, 255);
    C_PUPIL = hub75_color565(5, 8, 20);
    C_PINK = hub75_color565(255, 85, 155);
    C_RED = hub75_color565(255, 40, 55);
    C_ORANGE = hub75_color565(255, 130, 30);
    C_TEAR = hub75_color565(45, 175, 255);

    qeyes_show_closed();
}

void qeyes_task(void)
{
    typedef enum {
        QEYES_MODE_CLOSED = 0u,
        QEYES_MODE_OPEN,
        QEYES_MODE_HEART,
        QEYES_MODE_SQUINT,
        QEYES_MODE_CONFUSED_TO_ANGRY,
        QEYES_MODE_ANGRY,
        QEYES_MODE_TEAR
    } qeyes_mode_t;

    static qeyes_mode_t mode = QEYES_MODE_CLOSED;
    static uint32_t mode_start = 0u;
    static uint8_t last_mode = 0xFFu;
    static uint8_t last_key = 0xFFu;
    uint32_t now;
    uint32_t elapsed;
    uint8_t cmd;
    uint8_t key;
    uint8_t cmd_applied;

    now = HAL_GetTick();
    cmd = usart_get_eye_cmd();

    if (cmd != 0u) {
        cmd_applied = 1u;

        switch (cmd) {
        case 10u:
            mode = QEYES_MODE_CLOSED;
            break;

        case 11u:
            mode = QEYES_MODE_OPEN;
            break;

        case 12u:
        case 16u:
            mode = QEYES_MODE_HEART;
            break;

        case 13u:
        case 15u:
            mode = QEYES_MODE_SQUINT;
            break;

        case 14u:
            mode = QEYES_MODE_CONFUSED_TO_ANGRY;
            break;

        case 18u:
            mode = QEYES_MODE_TEAR;
            break;

        default:
            cmd_applied = 0u;
            break;
        }

        if (cmd_applied != 0u) {
            mode_start = now;
            last_mode = 0xFFu;
            last_key = 0xFFu;
        }
    }

    elapsed = now - mode_start;

    if ((mode == QEYES_MODE_CONFUSED_TO_ANGRY) && (elapsed >= 1000u)) {
        mode = QEYES_MODE_ANGRY;
        mode_start = now;
        elapsed = 0u;
        last_mode = 0xFFu;
        last_key = 0xFFu;
    }

    key = 0u;
    if (mode == QEYES_MODE_OPEN) {
        if ((elapsed >= 5000u) && ((elapsed % 5000u) < BLINK_CLOSED_MS)) {
            key = 100u;
        } else {
            key = (uint8_t)((elapsed / 900u) % 6u);
        }
    } else if (mode == QEYES_MODE_TEAR) {
        key = (uint8_t)((elapsed / TEAR_FRAME_MS) % TEAR_CYCLE_STEPS);
    }

    if ((mode == last_mode) && (key == last_key)) {
        return;
    }

    last_mode = mode;
    last_key = key;

    switch (mode) {
    case QEYES_MODE_CLOSED:
        qeyes_show_closed();
        break;

    case QEYES_MODE_OPEN:
        if (key == 100u) {
            qeyes_show_closed();
        } else {
            qeyes_show_open_look(elapsed);
        }
        break;

    case QEYES_MODE_HEART:
        qeyes_show_heart();
        break;

    case QEYES_MODE_SQUINT:
        qeyes_show_squint();
        break;

    case QEYES_MODE_CONFUSED_TO_ANGRY:
        qeyes_show_confused();
        break;

    case QEYES_MODE_TEAR:
        qeyes_show_tear_frame(elapsed);
        break;

    case QEYES_MODE_ANGRY:
    default:
        qeyes_show_angry();
        break;
    }
}

