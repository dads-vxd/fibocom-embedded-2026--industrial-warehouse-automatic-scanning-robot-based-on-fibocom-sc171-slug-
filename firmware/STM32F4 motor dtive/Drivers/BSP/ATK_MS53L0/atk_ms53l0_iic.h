/**
 ****************************************************************************************************
 * @file        atk_ms53l0_iic.h
 * @brief       ATK-MS53L0 single software IIC interface driver header
 ****************************************************************************************************
 */

#ifndef __ATK_MS53L0_IIC_H
#define __ATK_MS53L0_IIC_H

#include "./SYSTEM/sys/sys.h"

/* Single opposite-facing laser: SCL = PB11, SDA = PB10. */
#define ATK_MS53L0_IIC_SCL_GPIO_PORT             GPIOB
#define ATK_MS53L0_IIC_SCL_GPIO_PIN              GPIO_PIN_11
#define ATK_MS53L0_IIC_SCL_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)
#define ATK_MS53L0_IIC_SDA_GPIO_PORT             GPIOB
#define ATK_MS53L0_IIC_SDA_GPIO_PIN              GPIO_PIN_10
#define ATK_MS53L0_IIC_SDA_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

void atk_ms53l0_iic_start(void);
void atk_ms53l0_iic_stop(void);
uint8_t atk_ms53l0_iic_wait_ack(void);
void atk_ms53l0_iic_ack(void);
void atk_ms53l0_iic_nack(void);
void atk_ms53l0_iic_send_byte(uint8_t dat);
uint8_t atk_ms53l0_iic_read_byte(uint8_t ack);
void atk_ms53l0_iic_init(void);

#endif
