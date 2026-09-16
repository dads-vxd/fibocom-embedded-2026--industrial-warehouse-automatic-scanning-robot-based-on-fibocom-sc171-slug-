/**
 ****************************************************************************************************
 * @file        atk_ms53l0.h
 * @brief       ATK-MS53L0 / VL53L0X single module hardware control header
 ****************************************************************************************************
 */

#ifndef __ATK_MS53L0_H
#define __ATK_MS53L0_H

#include "./SYSTEM/sys/sys.h"
#include "vl53l0x_api.h"

/* Single opposite-facing laser XSH / XSHUT: PC0. */
#define ATK_MS53L0_XSH_GPIO_PORT             GPIOC
#define ATK_MS53L0_XSH_GPIO_PIN              GPIO_PIN_0
#define ATK_MS53L0_XSH_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)

#define ATK_MS53L0_XSH(x)                    do{ x ?                                                                                      \
                                                  HAL_GPIO_WritePin(ATK_MS53L0_XSH_GPIO_PORT, ATK_MS53L0_XSH_GPIO_PIN, GPIO_PIN_SET) :    \
                                                  HAL_GPIO_WritePin(ATK_MS53L0_XSH_GPIO_PORT, ATK_MS53L0_XSH_GPIO_PIN, GPIO_PIN_RESET);   \
                                              }while(0)

#define ATK_MS53L0_IIC_ADDR                  0x29
#define ATK_MS53L0_MODULE_ID                 0xEEAA

void atk_ms53l0_hw_init(void);

#endif
