/**
 ****************************************************************************************************
 * @file        atk_ms53l0_iic.c
 * @brief       ATK-MS53L0 single software IIC interface driver
 ****************************************************************************************************
 */

#include "./BSP/ATK_MS53L0/atk_ms53l0_iic.h"
#include "./SYSTEM/delay/delay.h"

static inline void atk_ms53l0_iic_delay(void)
{
    delay_us(2);
}

static void atk_ms53l0_iic_scl_write(uint8_t state)
{
    HAL_GPIO_WritePin(ATK_MS53L0_IIC_SCL_GPIO_PORT,
                      ATK_MS53L0_IIC_SCL_GPIO_PIN,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void atk_ms53l0_iic_sda_write(uint8_t state)
{
    HAL_GPIO_WritePin(ATK_MS53L0_IIC_SDA_GPIO_PORT,
                      ATK_MS53L0_IIC_SDA_GPIO_PIN,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static GPIO_PinState atk_ms53l0_iic_sda_read(void)
{
    return HAL_GPIO_ReadPin(ATK_MS53L0_IIC_SDA_GPIO_PORT,
                            ATK_MS53L0_IIC_SDA_GPIO_PIN);
}

void atk_ms53l0_iic_start(void)
{
    atk_ms53l0_iic_sda_write(1);
    atk_ms53l0_iic_scl_write(1);
    atk_ms53l0_iic_delay();
    atk_ms53l0_iic_sda_write(0);
    atk_ms53l0_iic_delay();
    atk_ms53l0_iic_scl_write(0);
    atk_ms53l0_iic_delay();
}

void atk_ms53l0_iic_stop(void)
{
    atk_ms53l0_iic_sda_write(0);
    atk_ms53l0_iic_delay();
    atk_ms53l0_iic_scl_write(1);
    atk_ms53l0_iic_delay();
    atk_ms53l0_iic_sda_write(1);
    atk_ms53l0_iic_delay();
}

uint8_t atk_ms53l0_iic_wait_ack(void)
{
    uint8_t waittime = 0;
    uint8_t rack = 0;

    atk_ms53l0_iic_sda_write(1);
    atk_ms53l0_iic_delay();
    atk_ms53l0_iic_scl_write(1);
    atk_ms53l0_iic_delay();

    while (atk_ms53l0_iic_sda_read())
    {
        waittime++;
        if (waittime > 250)
        {
            atk_ms53l0_iic_stop();
            rack = 1;
            break;
        }
    }

    atk_ms53l0_iic_scl_write(0);
    atk_ms53l0_iic_delay();
    return rack;
}

void atk_ms53l0_iic_ack(void)
{
    atk_ms53l0_iic_sda_write(0);
    atk_ms53l0_iic_delay();
    atk_ms53l0_iic_scl_write(1);
    atk_ms53l0_iic_delay();
    atk_ms53l0_iic_scl_write(0);
    atk_ms53l0_iic_delay();
    atk_ms53l0_iic_sda_write(1);
    atk_ms53l0_iic_delay();
}

void atk_ms53l0_iic_nack(void)
{
    atk_ms53l0_iic_sda_write(1);
    atk_ms53l0_iic_delay();
    atk_ms53l0_iic_scl_write(1);
    atk_ms53l0_iic_delay();
    atk_ms53l0_iic_scl_write(0);
    atk_ms53l0_iic_delay();
}

void atk_ms53l0_iic_send_byte(uint8_t dat)
{
    uint8_t t;

    for (t = 0; t < 8; t++)
    {
        atk_ms53l0_iic_sda_write((dat & 0x80) >> 7);
        atk_ms53l0_iic_delay();
        atk_ms53l0_iic_scl_write(1);
        atk_ms53l0_iic_delay();
        atk_ms53l0_iic_scl_write(0);
        dat <<= 1;
    }

    atk_ms53l0_iic_sda_write(1);
}

uint8_t atk_ms53l0_iic_read_byte(uint8_t ack)
{
    uint8_t i;
    uint8_t dat = 0;

    for (i = 0; i < 8; i++)
    {
        dat <<= 1;
        atk_ms53l0_iic_scl_write(1);
        atk_ms53l0_iic_delay();
        if (atk_ms53l0_iic_sda_read())
        {
            dat++;
        }
        atk_ms53l0_iic_scl_write(0);
        atk_ms53l0_iic_delay();
    }

    if (ack == 0)
    {
        atk_ms53l0_iic_nack();
    }
    else
    {
        atk_ms53l0_iic_ack();
    }

    return dat;
}

static void atk_ms53l0_iic_gpio_init(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    gpio_init_struct.Pin = pin;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(port, &gpio_init_struct);
}

void atk_ms53l0_iic_init(void)
{
    ATK_MS53L0_IIC_SCL_GPIO_CLK_ENABLE();
    ATK_MS53L0_IIC_SDA_GPIO_CLK_ENABLE();

    atk_ms53l0_iic_gpio_init(ATK_MS53L0_IIC_SCL_GPIO_PORT,
                             ATK_MS53L0_IIC_SCL_GPIO_PIN);
    atk_ms53l0_iic_gpio_init(ATK_MS53L0_IIC_SDA_GPIO_PORT,
                             ATK_MS53L0_IIC_SDA_GPIO_PIN);
    atk_ms53l0_iic_stop();
}
