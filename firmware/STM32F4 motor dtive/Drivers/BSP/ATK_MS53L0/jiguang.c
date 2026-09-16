#include "jiguang.h"
#include "./BSP/ATK_MS53L0/atk_ms53l0.h"
#include "./BSP/ATK_MS53L0/atk_ms53l0_iic.h"
#include "./SYSTEM/usart/usart.h"
#include <stdio.h>

#define JIGUANG_IIC_ADDR                    ATK_MS53L0_IIC_ADDR
#define JIGUANG_DEVICE_MODE                 VL53L0X_DEVICEMODE_CONTINUOUS_TIMED_RANGING
#define JIGUANG_BUDGET_TIME_US              (33U * 1000U)
#define JIGUANG_INTER_MEASUREMENT_MS        JIGUANG_SAMPLE_INTERVAL_MS

typedef struct
{
    uint8_t active;
    uint8_t sample_count;
    uint8_t query_mode;
    uint8_t result_ready;
    uint32_t last_sample_tick;
    uint16_t threshold;
    uint16_t average_result;
    uint8_t greater_count;
    uint8_t less_count;
    uint8_t equal_count;
    uint16_t samples[JIGUANG_AVERAGE_SAMPLE_COUNT];
} JiguangQueryRuntime;

typedef enum
{
    JIGUANG_QUERY_NONE = 0,
    JIGUANG_QUERY_UART_AVERAGE,
    JIGUANG_QUERY_SILENT_AVERAGE,
    JIGUANG_QUERY_SILENT_THRESHOLD,
} JiguangQueryMode;

static VL53L0X_Dev_t s_jiguang_dev =
{
    .I2cDevAddr = JIGUANG_IIC_ADDR,
};

static uint8_t s_jiguang_initialized = 0U;
static JiguangQueryRuntime s_jiguang_query = {0};

static uint8_t jiguang_detect_device(void)
{
    uint16_t module_id = 0U;

    VL53L0X_RdWord(&s_jiguang_dev, VL53L0X_REG_IDENTIFICATION_MODEL_ID, &module_id);
    if (module_id != ATK_MS53L0_MODULE_ID)
    {
        printf("ATK-MS53L0 Detect Failed! id=0x%04X\r\n", module_id);
        return 1U;
    }

    VL53L0X_DataInit(&s_jiguang_dev);
    return 0U;
}

static void jiguang_config_device(void)
{
    uint8_t vhvsettings;
    uint8_t phasecal;

    VL53L0X_StaticInit(&s_jiguang_dev);
    VL53L0X_PerformRefCalibration(&s_jiguang_dev, &vhvsettings, &phasecal);
    VL53L0X_SetDeviceMode(&s_jiguang_dev, JIGUANG_DEVICE_MODE);
    VL53L0X_SetMeasurementTimingBudgetMicroSeconds(&s_jiguang_dev, JIGUANG_BUDGET_TIME_US);
    VL53L0X_SetInterMeasurementPeriodMilliSeconds(&s_jiguang_dev, JIGUANG_INTER_MEASUREMENT_MS);
}

static void jiguang_stop_active_measurement(void)
{
    if (s_jiguang_query.active != 0U)
    {
        VL53L0X_StopMeasurement(&s_jiguang_dev);
        VL53L0X_ClearInterruptMask(&s_jiguang_dev, 0U);
    }

    s_jiguang_query.active = 0U;
    s_jiguang_query.sample_count = 0U;
    s_jiguang_query.last_sample_tick = 0U;
}

void jiguang_cancel_query(void)
{
    jiguang_stop_active_measurement();
    s_jiguang_query.query_mode = JIGUANG_QUERY_NONE;
    s_jiguang_query.result_ready = 0U;
    s_jiguang_query.threshold = 0U;
    s_jiguang_query.average_result = 0U;
    s_jiguang_query.greater_count = 0U;
    s_jiguang_query.less_count = 0U;
    s_jiguang_query.equal_count = 0U;
}

uint8_t jiguang_init(void)
{
    s_jiguang_initialized = 0U;
    jiguang_cancel_query();
    s_jiguang_dev.I2cDevAddr = JIGUANG_IIC_ADDR;

    atk_ms53l0_hw_init();
    VL53L0X_comms_initialise(0U, 0U);

    if (jiguang_detect_device() != 0U)
    {
        return 1U;
    }

    jiguang_config_device();
    s_jiguang_initialized = 1U;
    printf("ATK-MS53L0 Single Init OK, interval=50ms\r\n");
    return 0U;
}

uint16_t jiguang_average_10(const uint16_t samples[JIGUANG_AVERAGE_SAMPLE_COUNT])
{
    uint8_t i;
    uint32_t sum = 0U;

    if (samples == 0)
    {
        return 0U;
    }

    for (i = 0U; i < JIGUANG_AVERAGE_SAMPLE_COUNT; i++)
    {
        sum += samples[i];
    }

    return (uint16_t)(sum / JIGUANG_AVERAGE_SAMPLE_COUNT);
}

static uint8_t jiguang_start_query(JiguangQueryMode query_mode, uint16_t threshold)
{
    if (s_jiguang_initialized == 0U)
    {
        return 1U;
    }

    jiguang_cancel_query();
    s_jiguang_query.active = 1U;
    s_jiguang_query.sample_count = 0U;
    s_jiguang_query.query_mode = (uint8_t)query_mode;
    s_jiguang_query.threshold = threshold;
    s_jiguang_query.last_sample_tick = HAL_GetTick();

    VL53L0X_ClearInterruptMask(&s_jiguang_dev, 0U);
    if (VL53L0X_StartMeasurement(&s_jiguang_dev) != VL53L0X_ERROR_NONE)
    {
        jiguang_cancel_query();
        return 1U;
    }

    return 0U;
}

uint8_t jiguang_request_average(void)
{
    return jiguang_start_query(JIGUANG_QUERY_UART_AVERAGE, 0U);
}

uint8_t jiguang_request_average_silent(void)
{
    return jiguang_start_query(JIGUANG_QUERY_SILENT_AVERAGE, 0U);
}

uint8_t jiguang_request_threshold_count(uint16_t threshold)
{
    return jiguang_start_query(JIGUANG_QUERY_SILENT_THRESHOLD, threshold);
}

uint8_t jiguang_take_average_result(uint16_t *average)
{
    if ((average == 0) ||
        (s_jiguang_query.result_ready == 0U) ||
        (s_jiguang_query.query_mode != JIGUANG_QUERY_SILENT_AVERAGE))
    {
        return 0U;
    }

    *average = s_jiguang_query.average_result;
    s_jiguang_query.result_ready = 0U;
    s_jiguang_query.query_mode = JIGUANG_QUERY_NONE;
    return 1U;
}

uint8_t jiguang_take_threshold_result(uint8_t *greater_count,
                                      uint8_t *less_count,
                                      uint8_t *equal_count)
{
    if ((greater_count == 0) || (less_count == 0) || (equal_count == 0) ||
        (s_jiguang_query.result_ready == 0U) ||
        (s_jiguang_query.query_mode != JIGUANG_QUERY_SILENT_THRESHOLD))
    {
        return 0U;
    }

    *greater_count = s_jiguang_query.greater_count;
    *less_count = s_jiguang_query.less_count;
    *equal_count = s_jiguang_query.equal_count;
    s_jiguang_query.result_ready = 0U;
    s_jiguang_query.query_mode = JIGUANG_QUERY_NONE;
    return 1U;
}

void jiguang_query_process(void)
{
    uint8_t ready = 0U;
    uint8_t i;
    uint8_t query_mode;
    uint8_t greater_count = 0U;
    uint8_t less_count = 0U;
    uint8_t equal_count = 0U;
    uint16_t average;
    uint16_t threshold;
    uint32_t now;
    VL53L0X_RangingMeasurementData_t data;

    if (s_jiguang_query.active == 0U)
    {
        return;
    }

    now = HAL_GetTick();
    if ((now - s_jiguang_query.last_sample_tick) < JIGUANG_SAMPLE_INTERVAL_MS)
    {
        return;
    }

    if (VL53L0X_GetMeasurementDataReady(&s_jiguang_dev, &ready) != VL53L0X_ERROR_NONE)
    {
        return;
    }

    if (ready == 0U)
    {
        return;
    }

    if (VL53L0X_GetRangingMeasurementData(&s_jiguang_dev, &data) != VL53L0X_ERROR_NONE)
    {
        VL53L0X_ClearInterruptMask(&s_jiguang_dev, 0U);
        return;
    }
    VL53L0X_ClearInterruptMask(&s_jiguang_dev, 0U);

    s_jiguang_query.samples[s_jiguang_query.sample_count] = data.RangeMilliMeter;
    s_jiguang_query.sample_count++;
    s_jiguang_query.last_sample_tick = now;

    if (s_jiguang_query.sample_count < JIGUANG_AVERAGE_SAMPLE_COUNT)
    {
        return;
    }

    average = jiguang_average_10(s_jiguang_query.samples);
    query_mode = s_jiguang_query.query_mode;
    threshold = s_jiguang_query.threshold;

    if (query_mode == JIGUANG_QUERY_SILENT_THRESHOLD)
    {
        for (i = 0U; i < JIGUANG_AVERAGE_SAMPLE_COUNT; i++)
        {
            if (s_jiguang_query.samples[i] > threshold)
            {
                greater_count++;
            }
            else if (s_jiguang_query.samples[i] < threshold)
            {
                less_count++;
            }
            else
            {
                equal_count++;
            }
        }
    }

    jiguang_stop_active_measurement();

    if (query_mode == JIGUANG_QUERY_UART_AVERAGE)
    {
        s_jiguang_query.query_mode = JIGUANG_QUERY_NONE;
        printf("%u\r\n", (unsigned int)average);
    }
    else if (query_mode == JIGUANG_QUERY_SILENT_AVERAGE)
    {
        s_jiguang_query.average_result = average;
        s_jiguang_query.result_ready = 1U;
    }
    else if (query_mode == JIGUANG_QUERY_SILENT_THRESHOLD)
    {
        s_jiguang_query.greater_count = greater_count;
        s_jiguang_query.less_count = less_count;
        s_jiguang_query.equal_count = equal_count;
        s_jiguang_query.result_ready = 1U;
    }
    else
    {
        s_jiguang_query.query_mode = JIGUANG_QUERY_NONE;
        s_jiguang_query.result_ready = 0U;
    }
}
