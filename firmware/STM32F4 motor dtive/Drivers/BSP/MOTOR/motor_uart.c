#include "./BSP/MOTOR/motor_uart.h"

TIM_HandleTypeDef htim1;

#if (PROJECT_HW_MODE == PROJECT_HW_MODE_MOTOR)
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
#endif

void motor_init(void)
{
#if (PROJECT_HW_MODE == PROJECT_HW_MODE_MOTOR)
    MX_GPIO_Init();
    MX_TIM1_Init();
#endif
}

#if (PROJECT_HW_MODE == PROJECT_HW_MODE_MOTOR)
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    MOTOR_FRONT_DIR_GPIO_CLK_ENABLE();
    MOTOR_REAR_DIR_GPIO_CLK_ENABLE();

    gpio_init_struct.Mode = MOTOR_DIR_GPIO_MODE;
    gpio_init_struct.Pull = MOTOR_DIR_GPIO_PULL;
    gpio_init_struct.Speed = MOTOR_DIR_GPIO_SPEED;

    gpio_init_struct.Pin = MOTOR_FRONT_DIR_GPIO_PIN;
    HAL_GPIO_Init(MOTOR_FRONT_DIR_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = MOTOR_REAR_DIR_GPIO_PIN;
    HAL_GPIO_Init(MOTOR_REAR_DIR_GPIO_PORT, &gpio_init_struct);

    HAL_GPIO_WritePin(MOTOR_FRONT_DIR_GPIO_PORT, MOTOR_FRONT_DIR_GPIO_PIN, MOTOR_FRONT_DIR_DEFAULT_LEVEL);
    HAL_GPIO_WritePin(MOTOR_REAR_DIR_GPIO_PORT, MOTOR_REAR_DIR_GPIO_PIN, MOTOR_REAR_DIR_DEFAULT_LEVEL);
}

static void MX_TIM1_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim1.Instance = MOTOR_PWM_TIM;
    htim1.Init.Prescaler = MOTOR_PWM_PRESCALER;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = MOTOR_PWM_PERIOD;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim1);

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = MOTOR_PWM_INIT_DUTY;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, MOTOR_FRONT_PWM_CHANNEL);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, MOTOR_REAR_PWM_CHANNEL);

    HAL_TIM_MspPostInit(&htim1);

    HAL_TIM_PWM_Start(&htim1, MOTOR_FRONT_PWM_CHANNEL);
    HAL_TIM_PWM_Start(&htim1, MOTOR_REAR_PWM_CHANNEL);
}
#endif

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *tim_pwmHandle)
{
#if (PROJECT_HW_MODE == PROJECT_HW_MODE_MOTOR)
    if (tim_pwmHandle->Instance == MOTOR_PWM_TIM)
    {
        MOTOR_PWM_TIM_CLK_ENABLE();
    }
#else
    (void)tim_pwmHandle;
#endif
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *timHandle)
{
#if (PROJECT_HW_MODE == PROJECT_HW_MODE_MOTOR)
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (timHandle->Instance == MOTOR_PWM_TIM)
    {
        MOTOR_FRONT_PWM_GPIO_CLK_ENABLE();
        MOTOR_REAR_PWM_GPIO_CLK_ENABLE();

        GPIO_InitStruct.Mode = MOTOR_PWM_GPIO_MODE;
        GPIO_InitStruct.Pull = MOTOR_PWM_GPIO_PULL;
        GPIO_InitStruct.Speed = MOTOR_PWM_GPIO_SPEED;

        GPIO_InitStruct.Pin = MOTOR_FRONT_PWM_GPIO_PIN;
        GPIO_InitStruct.Alternate = MOTOR_FRONT_PWM_GPIO_AF;
        HAL_GPIO_Init(MOTOR_FRONT_PWM_GPIO_PORT, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = MOTOR_REAR_PWM_GPIO_PIN;
        GPIO_InitStruct.Alternate = MOTOR_REAR_PWM_GPIO_AF;
        HAL_GPIO_Init(MOTOR_REAR_PWM_GPIO_PORT, &GPIO_InitStruct);
    }
#else
    (void)timHandle;
#endif
}

void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef *tim_pwmHandle)
{
#if (PROJECT_HW_MODE == PROJECT_HW_MODE_MOTOR)
    if (tim_pwmHandle->Instance == MOTOR_PWM_TIM)
    {
        MOTOR_PWM_TIM_CLK_DISABLE();
    }
#else
    (void)tim_pwmHandle;
#endif
}
