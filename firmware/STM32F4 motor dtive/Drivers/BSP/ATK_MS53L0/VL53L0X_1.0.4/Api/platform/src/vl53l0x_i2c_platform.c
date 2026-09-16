/*
 * COPYRIGHT (C) STMicroelectronics 2015. All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * STMicroelectronics ("Confidential Information").  You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered into
 * with STMicroelectronics
 *
 * Programming Golden Rule: Keep it Simple!
 *
 */

/*!
 * \file   VL53L0X_platform.c
 * \brief  Code function defintions for Ewok Platform Layer
 *
 */

#include <stdio.h>    // sprintf(), vsnprintf(), printf()

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#include "vl53l0x_i2c_platform.h"
#include "vl53l0x_def.h"

#include "./BSP/ATK_MS53L0/atk_ms53l0_iic.h"
#include "./SYSTEM/delay/delay.h"

#include "vl53l0x_platform_log.h"

#ifdef VL53L0X_LOG_ENABLE
#define trace_print(level, ...) trace_print_module_function(TRACE_MODULE_PLATFORM, level, TRACE_FUNCTION_NONE, ##__VA_ARGS__)
#define trace_i2c(...) trace_print_module_function(TRACE_MODULE_NONE, TRACE_LEVEL_NONE, TRACE_FUNCTION_I2C, ##__VA_ARGS__)
#endif



char  debug_string[VL53L0X_MAX_STRING_LENGTH_PLT];


#define MIN_COMMS_VERSION_MAJOR     1
#define MIN_COMMS_VERSION_MINOR     8
#define MIN_COMMS_VERSION_BUILD     1
#define MIN_COMMS_VERSION_REVISION  0

#define STATUS_OK              0x00
#define STATUS_FAIL            0x01

int32_t VL53L0X_comms_initialise(uint8_t comms_type, uint16_t comms_speed_khz)
{
    atk_ms53l0_iic_init();
    
    return STATUS_OK;
}

int32_t VL53L0X_comms_close(void)
{
    return STATUS_OK;
}

int32_t VL53L0X_write_multi(uint8_t address, uint8_t index, uint8_t *pdata, int32_t count)
{
    int32_t i;
    
    atk_ms53l0_iic_start();
    atk_ms53l0_iic_send_byte((address << 1) | 0);
    if (atk_ms53l0_iic_wait_ack() == 1)
    {
        atk_ms53l0_iic_stop();
        return STATUS_FAIL;
    }
    atk_ms53l0_iic_send_byte(index);
    if (atk_ms53l0_iic_wait_ack() == 1)
    {
        atk_ms53l0_iic_stop();
        return STATUS_FAIL;
    }
    for (i=0; i<count; i++)
    {
        atk_ms53l0_iic_send_byte(pdata[i]);
        if (atk_ms53l0_iic_wait_ack() == 1)
        {
            atk_ms53l0_iic_stop();
            return STATUS_FAIL;
        }
    }
    atk_ms53l0_iic_stop();
    return STATUS_OK;
}

int32_t VL53L0X_read_multi(uint8_t address, uint8_t index, uint8_t *pdata, int32_t count)
{
    atk_ms53l0_iic_start();
    atk_ms53l0_iic_send_byte((address << 1) | 0);
    if (atk_ms53l0_iic_wait_ack() == 1)
    {
        atk_ms53l0_iic_stop();
        return STATUS_FAIL;
    }
    atk_ms53l0_iic_send_byte(index);
    if (atk_ms53l0_iic_wait_ack() == 1)
    {
        atk_ms53l0_iic_stop();
        return STATUS_FAIL;
    }
    atk_ms53l0_iic_start();
    atk_ms53l0_iic_send_byte((address << 1) | 1);
    if (atk_ms53l0_iic_wait_ack() == 1)
    {
        atk_ms53l0_iic_stop();
        return STATUS_FAIL;
    }
    while (count)
    {
        *pdata = atk_ms53l0_iic_read_byte((count > 1) ? 1 : 0);
        count--;
        pdata++;
    }
    atk_ms53l0_iic_stop();
    return STATUS_OK;
}


int32_t VL53L0X_write_byte(uint8_t address, uint8_t index, uint8_t data)
{
    int32_t status = STATUS_OK;
    const int32_t cbyte_count = 1;
    
    status = VL53L0X_write_multi(address, index, &data, cbyte_count);
    
    return status;
}


int32_t VL53L0X_write_word(uint8_t address, uint8_t index, uint16_t data)
{
    int32_t status = STATUS_OK;
    
    uint8_t  buffer[BYTES_PER_WORD];
    
    // Split 16-bit word into MS and LS uint8_t
    buffer[0] = (uint8_t)(data >> 8);
    buffer[1] = (uint8_t)(data &  0x00FF);
    
    status = VL53L0X_write_multi(address, index, buffer, BYTES_PER_WORD);
    
    return status;
}


int32_t VL53L0X_write_dword(uint8_t address, uint8_t index, uint32_t data)
{
    int32_t status = STATUS_OK;
    uint8_t buffer[BYTES_PER_DWORD];
    
    // Split 32-bit word into MS ... LS bytes
    buffer[0] = (uint8_t) (data >> 24);
    buffer[1] = (uint8_t)((data &  0x00FF0000) >> 16);
    buffer[2] = (uint8_t)((data &  0x0000FF00) >> 8);
    buffer[3] = (uint8_t) (data &  0x000000FF);
    
    status = VL53L0X_write_multi(address, index, buffer, BYTES_PER_DWORD);
    
    return status;
}


int32_t VL53L0X_read_byte(uint8_t address, uint8_t index, uint8_t *pdata)
{
    int32_t status = STATUS_OK;
    int32_t cbyte_count = 1;
    
    status = VL53L0X_read_multi(address, index, pdata, cbyte_count);
    
    return status;
}


int32_t VL53L0X_read_word(uint8_t address, uint8_t index, uint16_t *pdata)
{
    int32_t status = STATUS_OK;
    uint8_t buffer[BYTES_PER_WORD];
    
    status = VL53L0X_read_multi(address, index, buffer, BYTES_PER_WORD);
    *pdata = ((uint16_t)buffer[0]<<8) + (uint16_t)buffer[1];
    
    return status;

}

int32_t VL53L0X_read_dword(uint8_t address, uint8_t index, uint32_t *pdata)
{
    int32_t status = STATUS_OK;
    uint8_t buffer[BYTES_PER_DWORD];
    
    status = VL53L0X_read_multi(address, index, buffer, BYTES_PER_DWORD);
    *pdata = ((uint32_t)buffer[0]<<24) + ((uint32_t)buffer[1]<<16) + ((uint32_t)buffer[2]<<8) + (uint32_t)buffer[3];
    
    return status;

}

int32_t VL53L0X_platform_wait_us(int32_t wait_us)
{
    delay_us(wait_us);
    
    return STATUS_OK;
}


int32_t VL53L0X_wait_ms(int32_t wait_ms)
{
    delay_ms(wait_ms);
    
    return STATUS_OK;
}


int32_t VL53L0X_set_gpio(uint8_t level)
{
    return STATUS_OK;
}


int32_t VL53L0X_get_gpio(uint8_t *plevel)
{
    return STATUS_OK;
}


int32_t VL53L0X_release_gpio(void)
{
    return STATUS_OK;
}

int32_t VL53L0X_cycle_power(void)
{
    return STATUS_OK;
}


int32_t VL53L0X_get_timer_frequency(int32_t *ptimer_freq_hz)
{
    *ptimer_freq_hz = 0;
    
    return STATUS_FAIL;
}


int32_t VL53L0X_get_timer_value(int32_t *ptimer_count)
{
    *ptimer_count = 0;
    
    return STATUS_FAIL;
}
