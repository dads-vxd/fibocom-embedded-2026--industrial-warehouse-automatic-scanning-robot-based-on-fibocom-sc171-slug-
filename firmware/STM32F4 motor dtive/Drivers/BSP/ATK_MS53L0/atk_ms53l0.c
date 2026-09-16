/**
 ****************************************************************************************************
 * @file        atk_ms53l0.c
 * @brief       ATK-MS53L0 / VL53L0X single module hardware control
 ****************************************************************************************************
 */

#include "./BSP/ATK_MS53L0/atk_ms53l0.h"
#include "./SYSTEM/delay/delay.h"

void atk_ms53l0_hw_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    ATK_MS53L0_XSH_GPIO_CLK_ENABLE();
    gpio_init_struct.Pin = ATK_MS53L0_XSH_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ATK_MS53L0_XSH_GPIO_PORT, &gpio_init_struct);

    ATK_MS53L0_XSH(0);
    delay_ms(30);
    ATK_MS53L0_XSH(1);
    delay_ms(30);
}
