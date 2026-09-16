#include "./BSP/HUB75/hub75.h"

/* 3 bit-plane buffer. One byte stores upper/lower RGB data for one column. */
static uint8_t g_hub75_buff[HUB75_PANEL_WIDTH * (HUB75_PANEL_HEIGHT / 2u) * 3u];
static uint16_t g_hub75_brightness = 90u;

#define HUB75_DATA_PORTA_MASK    (HUB75_R1_PIN | HUB75_B2_PIN)
#define HUB75_DATA_PORTB_MASK    (HUB75_G1_PIN | HUB75_R2_PIN | HUB75_B1_PIN | HUB75_G2_PIN)
#define HUB75_ADDR_PORTA_MASK    (HUB75_A_PIN | HUB75_D_PIN | HUB75_E_PIN)
#define HUB75_ADDR_PORTB_MASK    (HUB75_B_PIN | HUB75_C_PIN)

#define HUB75_OE_H()             (HUB75_OE_PORT->BSRR  = HUB75_OE_PIN)
#define HUB75_OE_L()             (HUB75_OE_PORT->BSRR  = ((uint32_t)HUB75_OE_PIN  << 16u))
#define HUB75_CLK_H()            (HUB75_CLK_PORT->BSRR = HUB75_CLK_PIN)
#define HUB75_CLK_L()            (HUB75_CLK_PORT->BSRR = ((uint32_t)HUB75_CLK_PIN << 16u))
#define HUB75_LAT_H()            (HUB75_LAT_PORT->BSRR = HUB75_LAT_PIN)
#define HUB75_LAT_L()            (HUB75_LAT_PORT->BSRR = ((uint32_t)HUB75_LAT_PIN << 16u))

static void hub75_delay_nop(uint16_t n)
{
    while (n-- != 0u) {
        __NOP();
    }
}

uint16_t hub75_color565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r & 0xF8u) << 8) |
                      ((uint16_t)(g & 0xFCu) << 3) |
                      ((uint16_t)b >> 3));
}

static uint16_t hub75_map_x(uint16_t x)
{
    uint16_t panel;
    uint16_t col;

    if (x >= HUB75_PANEL_WIDTH) {
        return x;
    }

    panel = (uint16_t)(x / HUB75_SINGLE_PANEL_WIDTH);
    col = (uint16_t)(x % HUB75_SINGLE_PANEL_WIDTH);

#if (HUB75_MIRROR_X_IN_PANEL != 0u)
    col = (uint16_t)((HUB75_SINGLE_PANEL_WIDTH - 1u) - col);
#endif

#if (HUB75_SWAP_PANELS != 0u)
    panel = (uint16_t)((HUB75_CHAIN_PANELS - 1u) - panel);
#endif

    return (uint16_t)(panel * HUB75_SINGLE_PANEL_WIDTH + col);
}

static void hub75_gpio_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB3 and PB4 are used by HUB75, so disable JTAG and keep SWD download. */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    HAL_GPIO_WritePin(GPIOA,
                      HUB75_CLK_PIN | HUB75_E_PIN | HUB75_D_PIN | HUB75_B2_PIN |
                      HUB75_A_PIN | HUB75_R1_PIN,
                      GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB,
                      HUB75_B_PIN | HUB75_G1_PIN | HUB75_R2_PIN | HUB75_B1_PIN |
                      HUB75_C_PIN | HUB75_OE_PIN | HUB75_LAT_PIN | HUB75_G2_PIN,
                      GPIO_PIN_RESET);
    HAL_GPIO_WritePin(HUB75_OE_PORT, HUB75_OE_PIN, GPIO_PIN_SET);

    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio_init_struct.Pin = HUB75_CLK_PIN | HUB75_E_PIN | HUB75_D_PIN | HUB75_B2_PIN |
                           HUB75_A_PIN | HUB75_R1_PIN;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);

    gpio_init_struct.Pin = HUB75_B_PIN | HUB75_G1_PIN | HUB75_R2_PIN | HUB75_B1_PIN |
                           HUB75_C_PIN | HUB75_OE_PIN | HUB75_LAT_PIN | HUB75_G2_PIN;
    HAL_GPIO_Init(GPIOB, &gpio_init_struct);
}

static void hub75_set_scan_line(uint16_t line)
{
    uint32_t bsrr_a = ((uint32_t)HUB75_ADDR_PORTA_MASK << 16u);
    uint32_t bsrr_b = ((uint32_t)HUB75_ADDR_PORTB_MASK << 16u);

    if ((line & 0x01u) != 0u) {
        bsrr_a |= HUB75_A_PIN;
    }
    if ((line & 0x02u) != 0u) {
        bsrr_b |= HUB75_B_PIN;
    }
    if ((line & 0x04u) != 0u) {
        bsrr_b |= HUB75_C_PIN;
    }
    if ((line & 0x08u) != 0u) {
        bsrr_a |= HUB75_D_PIN;
    }

    GPIOA->BSRR = bsrr_a;
    GPIOB->BSRR = bsrr_b;
}

static void hub75_write_panel_pixel(uint16_t x, uint16_t y, uint8_t r8, uint8_t g8, uint8_t b8)
{
    uint16_t row_in_pair;
    uint16_t plane_stride;
    uint8_t *p0;
    uint8_t *p1;
    uint8_t *p2;

    if ((x >= HUB75_PANEL_WIDTH) || (y >= HUB75_PANEL_HEIGHT)) {
        return;
    }

    row_in_pair = (uint16_t)(y % (HUB75_PANEL_HEIGHT / 2u));
    plane_stride = (uint16_t)(HUB75_PANEL_WIDTH * (HUB75_PANEL_HEIGHT / 2u));

    p0 = &g_hub75_buff[(uint32_t)row_in_pair * HUB75_PANEL_WIDTH + x + (uint32_t)plane_stride * 0u];
    p1 = &g_hub75_buff[(uint32_t)row_in_pair * HUB75_PANEL_WIDTH + x + (uint32_t)plane_stride * 1u];
    p2 = &g_hub75_buff[(uint32_t)row_in_pair * HUB75_PANEL_WIDTH + x + (uint32_t)plane_stride * 2u];

    if (y < (HUB75_PANEL_HEIGHT / 2u)) {
        if (((r8 >> 6) & 0x01u) != 0u) { *p0 |=  (1u << 5); } else { *p0 &= (uint8_t)~(1u << 5); }
        if (((r8 >> 5) & 0x01u) != 0u) { *p1 |=  (1u << 5); } else { *p1 &= (uint8_t)~(1u << 5); }
        if (((r8 >> 4) & 0x01u) != 0u) { *p2 |=  (1u << 5); } else { *p2 &= (uint8_t)~(1u << 5); }

        if (((g8 >> 6) & 0x01u) != 0u) { *p0 |=  (1u << 6); } else { *p0 &= (uint8_t)~(1u << 6); }
        if (((g8 >> 5) & 0x01u) != 0u) { *p1 |=  (1u << 6); } else { *p1 &= (uint8_t)~(1u << 6); }
        if (((g8 >> 4) & 0x01u) != 0u) { *p2 |=  (1u << 6); } else { *p2 &= (uint8_t)~(1u << 6); }

        if (((b8 >> 6) & 0x01u) != 0u) { *p0 |=  (1u << 7); } else { *p0 &= (uint8_t)~(1u << 7); }
        if (((b8 >> 5) & 0x01u) != 0u) { *p1 |=  (1u << 7); } else { *p1 &= (uint8_t)~(1u << 7); }
        if (((b8 >> 4) & 0x01u) != 0u) { *p2 |=  (1u << 7); } else { *p2 &= (uint8_t)~(1u << 7); }
    } else {
        if (((r8 >> 6) & 0x01u) != 0u) { *p0 |=  (1u << 2); } else { *p0 &= (uint8_t)~(1u << 2); }
        if (((r8 >> 5) & 0x01u) != 0u) { *p1 |=  (1u << 2); } else { *p1 &= (uint8_t)~(1u << 2); }
        if (((r8 >> 4) & 0x01u) != 0u) { *p2 |=  (1u << 2); } else { *p2 &= (uint8_t)~(1u << 2); }

        if (((g8 >> 6) & 0x01u) != 0u) { *p0 |=  (1u << 3); } else { *p0 &= (uint8_t)~(1u << 3); }
        if (((g8 >> 5) & 0x01u) != 0u) { *p1 |=  (1u << 3); } else { *p1 &= (uint8_t)~(1u << 3); }
        if (((g8 >> 4) & 0x01u) != 0u) { *p2 |=  (1u << 3); } else { *p2 &= (uint8_t)~(1u << 3); }

        if (((b8 >> 6) & 0x01u) != 0u) { *p0 |=  (1u << 4); } else { *p0 &= (uint8_t)~(1u << 4); }
        if (((b8 >> 5) & 0x01u) != 0u) { *p1 |=  (1u << 4); } else { *p1 &= (uint8_t)~(1u << 4); }
        if (((b8 >> 4) & 0x01u) != 0u) { *p2 |=  (1u << 4); } else { *p2 &= (uint8_t)~(1u << 4); }
    }
}

void hub75_init(void)
{
    hub75_gpio_init();
    hub75_clear();
    HUB75_OE_H();
    HUB75_CLK_L();
    HUB75_LAT_L();
    hub75_set_scan_line(0u);
}

void hub75_set_brightness(uint16_t brightness)
{
    if (brightness > 500u) {
        brightness = 500u;
    }
    g_hub75_brightness = brightness;
}

void hub75_clear(void)
{
    memset(g_hub75_buff, 0, sizeof(g_hub75_buff));
}

void hub75_draw_pixel565(uint16_t x, uint16_t y, uint16_t color)
{
    uint16_t x_mapped;
    uint16_t y_mapped;
    uint8_t r5;
    uint8_t g6;
    uint8_t b5;
    uint8_t r8;
    uint8_t g8;
    uint8_t b8;

    if ((x >= HUB75_PANEL_WIDTH) || (y >= HUB75_PANEL_HEIGHT)) {
        return;
    }

    x_mapped = hub75_map_x(x);
    y_mapped = y;

#if (HUB75_FLIP_Y != 0u)
    y_mapped = (uint16_t)((HUB75_PANEL_HEIGHT - 1u) - y_mapped);
#endif

    r5 = (uint8_t)((color >> 11) & 0x1Fu);
    g6 = (uint8_t)((color >> 5) & 0x3Fu);
    b5 = (uint8_t)(color & 0x1Fu);

    r8 = (uint8_t)((r5 << 3) | (r5 >> 2));
    g8 = (uint8_t)((g6 << 2) | (g6 >> 4));
    b8 = (uint8_t)((b5 << 3) | (b5 >> 2));

    hub75_write_panel_pixel(x_mapped, y_mapped, r8, g8, b8);
}

void hub75_fill_screen(uint16_t color)
{
    if (color == HUB75_COLOR_BLACK) {
        hub75_clear();
        return;
    }

    for (uint16_t y = 0u; y < HUB75_PANEL_HEIGHT; y++) {
        for (uint16_t x = 0u; x < HUB75_PANEL_WIDTH; x++) {
            hub75_draw_pixel565(x, y, color);
        }
    }
}

void hub75_draw_rgb565_frame(const uint16_t *frame, uint16_t width, uint16_t height)
{
    uint16_t x_max;
    uint16_t y_max;

    if ((frame == NULL) || (width == 0u) || (height == 0u)) {
        hub75_clear();
        return;
    }

    x_max = (width > HUB75_PANEL_WIDTH) ? HUB75_PANEL_WIDTH : width;
    y_max = (height > HUB75_PANEL_HEIGHT) ? HUB75_PANEL_HEIGHT : height;

    hub75_clear();

    for (uint16_t y = 0u; y < y_max; y++) {
        for (uint16_t x = 0u; x < x_max; x++) {
            uint16_t pixel = frame[(uint32_t)y * width + x];
            hub75_draw_pixel565(x, y, pixel);
        }
    }
}

void hub75_display_scan(void)
{
    static const uint8_t plane_repeat[3] = {4u, 2u, 1u};
    static uint16_t plane = 0u;
    static uint16_t row = 0u;
    static uint8_t repeat_left = 4u;

    uint16_t plane_stride = (uint16_t)(HUB75_PANEL_WIDTH * (HUB75_PANEL_HEIGHT / 2u));
    const uint8_t *plane_data = &g_hub75_buff[(uint32_t)plane_stride * plane + (uint32_t)row * HUB75_PANEL_WIDTH];

    HUB75_OE_H();

    for (uint16_t i = 0u; i < HUB75_PANEL_WIDTH; i++) {
        uint8_t byte = plane_data[i];
        uint32_t bsrr_a = ((uint32_t)HUB75_DATA_PORTA_MASK << 16u);
        uint32_t bsrr_b = ((uint32_t)HUB75_DATA_PORTB_MASK << 16u);

        if ((byte & (1u << 5)) != 0u) { bsrr_a |= HUB75_R1_PIN; }
        if ((byte & (1u << 4)) != 0u) { bsrr_a |= HUB75_B2_PIN; }
        if ((byte & (1u << 6)) != 0u) { bsrr_b |= HUB75_G1_PIN; }
        if ((byte & (1u << 7)) != 0u) { bsrr_b |= HUB75_B1_PIN; }
        if ((byte & (1u << 2)) != 0u) { bsrr_b |= HUB75_R2_PIN; }
        if ((byte & (1u << 3)) != 0u) { bsrr_b |= HUB75_G2_PIN; }

        GPIOA->BSRR = bsrr_a;
        GPIOB->BSRR = bsrr_b;

        HUB75_CLK_L();
        HUB75_CLK_H();
    }

    hub75_set_scan_line(row);
    HUB75_LAT_H();
    __NOP();
    HUB75_LAT_L();

    HUB75_OE_L();
    hub75_delay_nop(g_hub75_brightness);
    HUB75_OE_H();

    row++;
    if (row >= HUB75_SCAN_ROWS) {
        row = 0u;
        if (repeat_left > 1u) {
            repeat_left--;
        } else {
            plane++;
            if (plane >= 3u) {
                plane = 0u;
            }
            repeat_left = plane_repeat[plane];
        }
    }
}
