#include "demo.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LED/led.h"
#include "./BSP/ENCODER/encoder.h"
#include "./BSP/MOTOR/motor.h"
#include "./BSP/CONTROL/control.h"
#include "./BSP/HCSR04/hcsr04.h"
#include "./BSP/ATK_MS53L0/jiguang.h"

TIM_HandleTypeDef htim2;
volatile uint8_t g_motion_update_req = 0U;
volatile uint32_t g_main_loop_heartbeat = 0U;

/* TIM2: 84MHz / 84 = 1MHz, 10000 counts = 10ms.
 * Encoder sampling, speed calculation, position judgement and PWM output run here.
 */
#define TIM2_ISR_PERIOD_US      10000U

void TIM2_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 84 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = TIM2_ISR_PERIOD_US - 1U;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    HAL_TIM_Base_Init(&htim2);

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig);

    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    HAL_TIM_Base_Start_IT(&htim2);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *tim_baseHandle)
{
    if (tim_baseHandle->Instance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
}

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        static int32_t last_enc3 = 0;
        static int32_t last_enc4 = 0;
        int32_t de3;
        int32_t de4;

        /* Fixed 10 ms control pipeline: read both encoders, judge every
         * linear/shift/spin target, then output the new PWM immediately.
         */
        encoder_update_count();

        de3 = g_encoder3_count - last_enc3;
        de4 = g_encoder4_count - last_enc4;
        last_enc3 = g_encoder3_count;
        last_enc4 = g_encoder4_count;

        /* Encoder direction is irrelevant to distance. Each 10 ms period uses
         * abs(current_count - previous_count) for both wheels. control.c then
         * averages the front/rear distances and adds the result to the target accumulator.
         */
        g_encoder_speed_front = (de3 < 0) ? -de3 : de3;  /* TIM3: front wheel */
        g_encoder_speed_rear = (de4 < 0) ? -de4 : de4;   /* TIM4: rear wheel */

        /* Encoder accumulation and target judgement must not depend on main-loop time. */
        car_encoder_control_10ms();

        /* Apply the command generated above in the same interrupt period. */
        motor_pwm_output_10ms();

        g_motion_update_req = 1U;
    }
}


void demo_run(void)
{
    while (1)
    {
        g_main_loop_heartbeat++;

        usart_protocol_process();
        hcsr04_service();
        jiguang_query_process();

        car_laser_distance_judge_process();
        car_protocol_report_process();

        if ((g_motion_update_req != 0U) || (g_ultrasonic_stop_flag != 0U))
        {
            g_motion_update_req = 0U;
            car_motion_process();
        }

        delay_ms(2);
    }
}
