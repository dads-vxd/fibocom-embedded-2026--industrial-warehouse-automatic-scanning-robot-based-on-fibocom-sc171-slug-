#include "./BSP/HCSR04/hcsr04.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/CONTROL/control.h"

volatile uint16_t g_hcsr04_distance_mm = 0U;
volatile uint8_t g_hcsr04_measure_ok = 0U;
volatile uint8_t g_hcsr04_near_count = 0U;

static uint32_t s_hcsr04_last_measure_tick = 0U;

typedef enum
{
    HCSR04_RESULT_INVALID = 0,
    HCSR04_RESULT_VALID,
    HCSR04_RESULT_NO_ECHO,
} Hcsr04Result;

static void hcsr04_gpio_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio_init_struct.Pin = HCSR04_TRIG_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(HCSR04_TRIG_GPIO_PORT, &gpio_init_struct);
    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_PORT, HCSR04_TRIG_GPIO_PIN, GPIO_PIN_RESET);

    gpio_init_struct.Pin = HCSR04_ECHO_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(HCSR04_ECHO_GPIO_PORT, &gpio_init_struct);
}

static uint32_t hcsr04_wait_echo(GPIO_PinState state, uint32_t timeout_us)
{
    uint32_t used_us = 0U;

    while (HAL_GPIO_ReadPin(HCSR04_ECHO_GPIO_PORT, HCSR04_ECHO_GPIO_PIN) != state)
    {
        if (used_us >= timeout_us)
        {
            return 0xFFFFFFFFU;
        }

        delay_us(1U);
        used_us++;
    }

    return used_us;
}

static Hcsr04Result hcsr04_read_result(uint16_t *distance_mm)
{
    uint32_t pulse_us = 0U;

    if (distance_mm == 0)
    {
        return HCSR04_RESULT_INVALID;
    }

    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_PORT, HCSR04_TRIG_GPIO_PIN, GPIO_PIN_RESET);
    delay_us(2U);

    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_PORT, HCSR04_TRIG_GPIO_PIN, GPIO_PIN_SET);
    delay_us(10U);
    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_PORT, HCSR04_TRIG_GPIO_PIN, GPIO_PIN_RESET);

    if (hcsr04_wait_echo(GPIO_PIN_SET, HCSR04_TIMEOUT_US) == 0xFFFFFFFFU)
    {
        /* ECHO remained low: normally there is no reflecting obstacle. */
        return HCSR04_RESULT_NO_ECHO;
    }

    while (HAL_GPIO_ReadPin(HCSR04_ECHO_GPIO_PORT, HCSR04_ECHO_GPIO_PIN) == GPIO_PIN_SET)
    {
        if (pulse_us >= HCSR04_TIMEOUT_US)
        {
            /* ECHO stuck high is a sensor fault, not a clear path. */
            return HCSR04_RESULT_INVALID;
        }

        delay_us(1U);
        pulse_us++;
    }

    *distance_mm = (uint16_t)((pulse_us * 10U + 29U) / 58U);
    return HCSR04_RESULT_VALID;
}

uint8_t hcsr04_read_mm(uint16_t *distance_mm)
{
    return (hcsr04_read_result(distance_mm) == HCSR04_RESULT_VALID) ? 1U : 0U;
}

void hcsr04_parking_reset(void)
{
    g_hcsr04_distance_mm = 0U;
    g_hcsr04_measure_ok = 0U;
    g_hcsr04_near_count = 0U;
    g_ultrasonic_stop_flag = 0U;

    /* After command 10, begin a fresh 50 ms sampling window. */
    s_hcsr04_last_measure_tick = HAL_GetTick();
}

void hcsr04_init(void)
{
    hcsr04_gpio_init();
    hcsr04_parking_reset();
}

void hcsr04_service(void)
{
    uint32_t now = HAL_GetTick();
    uint16_t distance_mm = 0U;
    Hcsr04Result result;
    uint8_t clear_sample;

    if (car_ultrasonic_monitor_enabled() == 0U)
    {
        /* Do not carry a stale near/clear result into a later eligible command. */
        g_hcsr04_distance_mm = 0U;
        g_hcsr04_measure_ok = 0U;
        g_hcsr04_near_count = 0U;
        g_ultrasonic_stop_flag = 0U;
        s_hcsr04_last_measure_tick = now;
        return;
    }

    if ((uint32_t)(now - s_hcsr04_last_measure_tick) < HCSR04_MEASURE_INTERVAL_MS)
    {
        return;
    }

    /* Update the start time before measurement so scan starts are about 50 ms apart. */
    s_hcsr04_last_measure_tick = now;
    result = hcsr04_read_result(&distance_mm);

    g_hcsr04_measure_ok = (result == HCSR04_RESULT_VALID) ? 1U : 0U;
    if (result == HCSR04_RESULT_VALID)
    {
        g_hcsr04_distance_mm = distance_mm;
    }
    else
    {
        g_hcsr04_distance_mm = 0U;
    }

    clear_sample = (((result == HCSR04_RESULT_VALID) &&
                     (distance_mm > HCSR04_NEAR_THRESHOLD_MM)) ||
                    (result == HCSR04_RESULT_NO_ECHO)) ? 1U : 0U;

    if ((result == HCSR04_RESULT_VALID) &&
        (distance_mm <= HCSR04_NEAR_THRESHOLD_MM))
    {
        if (g_hcsr04_near_count < HCSR04_NEAR_CONFIRM_COUNT)
        {
            g_hcsr04_near_count++;
        }

        if (g_hcsr04_near_count >= HCSR04_NEAR_CONFIRM_COUNT)
        {
            g_ultrasonic_stop_flag = 1U;
        }
    }
    else if (clear_sample != 0U)
    {
        /* Parking is held only while the sensor keeps the confirmed
         * three-consecutive-samples <= 150 mm condition. A valid far sample
         * or no echo breaks that condition immediately and requests resume.
         */
        g_hcsr04_near_count = 0U;
        g_ultrasonic_stop_flag = 0U;
    }
    else
    {
        /* A sensor-fault sample is neither near nor a confirmed clear path.
         * Keep the current parking decision, but restart near confirmation.
         */
        g_hcsr04_near_count = 0U;
    }
}
