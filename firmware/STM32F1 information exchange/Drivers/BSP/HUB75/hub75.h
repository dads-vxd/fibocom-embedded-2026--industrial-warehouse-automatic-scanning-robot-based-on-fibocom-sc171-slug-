#ifndef __HUB75_H
#define __HUB75_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

/*
 * Waveshare RGB-Matrix-P2.5-64x32, HUB75, 1/16 scan.
 * Target MCU: STM32F103C8T6 Blue Pill / ATK HAL project.
 *
 * Two 64x32 panels are cascaded horizontally:
 * MCU -> Panel 1 IN, Panel 1 OUT -> Panel 2 IN.
 *
 * Logical canvas: 128x32.
 * One eye is drawn on each 64x32 panel.
 *
 * Pin map:
 * R1  PA10    G1  PB3     B1  PB5
 * R2  PB4     G2  PB10    B2  PA8
 * A   PA9     B   PB0     C   PB6     D   PA7     E   PA6, kept low
 * CLK PA5     LAT PB9     OE  PB8
 */

#define HUB75_SINGLE_PANEL_WIDTH       64u
#define HUB75_CHAIN_PANELS             2u
#define HUB75_PANEL_WIDTH              (HUB75_SINGLE_PANEL_WIDTH * HUB75_CHAIN_PANELS)
#define HUB75_PANEL_HEIGHT             32u
#define HUB75_SCAN_ROWS                16u

/* If the left and right eyes are swapped after cascading, change this to 1. */
#define HUB75_SWAP_PANELS              0u

/* If each panel is mirrored left-right after cascading, change this to 1. */
#define HUB75_MIRROR_X_IN_PANEL        0u

/* The installed panels are upside down vertically. Mirror all logical Y coordinates
 * here so every directional element (brows, blush, hearts, closed/squint eyes,
 * confused eyes and tears) is corrected consistently. */
#define HUB75_FLIP_Y                   1u

#define HUB75_COLOR_BLACK              0x0000u
#define HUB75_COLOR_WHITE              0xFFFFu
#define HUB75_COLOR_RED                0xF800u
#define HUB75_COLOR_GREEN              0x07E0u
#define HUB75_COLOR_BLUE               0x001Fu
#define HUB75_COLOR_YELLOW             0xFFE0u
#define HUB75_COLOR_CYAN               0x07FFu
#define HUB75_COLOR_MAGENTA            0xF81Fu

#define HUB75_CLK_PORT                 GPIOA
#define HUB75_CLK_PIN                  GPIO_PIN_5
#define HUB75_E_PORT                   GPIOA
#define HUB75_E_PIN                    GPIO_PIN_6
#define HUB75_D_PORT                   GPIOA
#define HUB75_D_PIN                    GPIO_PIN_7
#define HUB75_B2_PORT                  GPIOA
#define HUB75_B2_PIN                   GPIO_PIN_8
#define HUB75_A_PORT                   GPIOA
#define HUB75_A_PIN                    GPIO_PIN_9
#define HUB75_R1_PORT                  GPIOA
#define HUB75_R1_PIN                   GPIO_PIN_10

#define HUB75_B_PORT                   GPIOB
#define HUB75_B_PIN                    GPIO_PIN_0
#define HUB75_G1_PORT                  GPIOB
#define HUB75_G1_PIN                   GPIO_PIN_3
#define HUB75_R2_PORT                  GPIOB
#define HUB75_R2_PIN                   GPIO_PIN_4
#define HUB75_B1_PORT                  GPIOB
#define HUB75_B1_PIN                   GPIO_PIN_5
#define HUB75_C_PORT                   GPIOB
#define HUB75_C_PIN                    GPIO_PIN_6
#define HUB75_OE_PORT                  GPIOB
#define HUB75_OE_PIN                   GPIO_PIN_8
#define HUB75_LAT_PORT                 GPIOB
#define HUB75_LAT_PIN                  GPIO_PIN_9
#define HUB75_G2_PORT                  GPIOB
#define HUB75_G2_PIN                   GPIO_PIN_10

void hub75_init(void);
void hub75_set_brightness(uint16_t brightness);
void hub75_clear(void);
void hub75_display_scan(void);

void hub75_draw_rgb565_frame(const uint16_t *frame, uint16_t width, uint16_t height);
void hub75_draw_pixel565(uint16_t x, uint16_t y, uint16_t color);
void hub75_fill_screen(uint16_t color);

uint16_t hub75_color565(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif
