#include "./BSP/MOTOR/motor.h"
#include "./BSP/MOTOR/motor_uart.h"

static volatile int16_t g_motor_target_front = 0;
static volatile int16_t g_motor_target_rear = 0;

#if (PROJECT_HW_MODE == PROJECT_HW_MODE_MOTOR)
static int16_t motor_apply_pwm_offset(int16_t pwm, int16_t offset)
{
    int32_t pwm_with_offset = pwm;

    if (pwm_with_offset > 0)
    {
        pwm_with_offset += offset;
    }
    else if (pwm_with_offset < 0)
    {
        pwm_with_offset -= offset;
    }
    else
    {
        return 0;
    }

    if (pwm_with_offset > MOTOR_PWM_MAX_DUTY)
    {
        pwm_with_offset = MOTOR_PWM_MAX_DUTY;
    }
    else if (pwm_with_offset < -MOTOR_PWM_MAX_DUTY)
    {
        pwm_with_offset = -MOTOR_PWM_MAX_DUTY;
    }

    return (int16_t)pwm_with_offset;
}

static int16_t motor_apply_polarity(int16_t pwm)
{
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
    return (int16_t)(-pwm);
#else
    return pwm;
#endif
}

static int motor_limit_duty(int16_t pwm)
{
    int duty = (int)pwm;

    if (duty < 0)
    {
        duty = -duty;
    }

    if (duty > MOTOR_PWM_MAX_DUTY)
    {
        duty = MOTOR_PWM_MAX_DUTY;
    }

    return duty;
}

static void motor_pwm_write_hw(int16_t front, int16_t rear)
{
    int duty;
    int16_t front_cmd;
    int16_t rear_cmd;
    if (MOTOR_PWM_TIM_HANDLE.Instance == NULL)
    {
        return;
    }

    front_cmd = motor_apply_polarity(
        motor_apply_pwm_offset(front, MOTOR_PWM_ACTIVE_MODE_OFFSET));
    rear_cmd = motor_apply_polarity(
        motor_apply_pwm_offset(rear, MOTOR_PWM_ACTIVE_MODE_OFFSET));

    if (front_cmd >= 0)
    {
        HAL_GPIO_WritePin(MOTOR_FRONT_DIR_GPIO_PORT, MOTOR_FRONT_DIR_GPIO_PIN, MOTOR_FRONT_FORWARD_LEVEL);
    }
    else
    {
        HAL_GPIO_WritePin(MOTOR_FRONT_DIR_GPIO_PORT, MOTOR_FRONT_DIR_GPIO_PIN, MOTOR_FRONT_REVERSE_LEVEL);
    }

    duty = motor_limit_duty(front_cmd);
    __HAL_TIM_SET_COMPARE(&MOTOR_PWM_TIM_HANDLE, MOTOR_FRONT_PWM_CHANNEL, duty);

    if (rear_cmd >= 0)
    {
        HAL_GPIO_WritePin(MOTOR_REAR_DIR_GPIO_PORT, MOTOR_REAR_DIR_GPIO_PIN, MOTOR_REAR_FORWARD_LEVEL);
    }
    else
    {
        HAL_GPIO_WritePin(MOTOR_REAR_DIR_GPIO_PORT, MOTOR_REAR_DIR_GPIO_PIN, MOTOR_REAR_REVERSE_LEVEL);
    }

    duty = motor_limit_duty(rear_cmd);
    __HAL_TIM_SET_COMPARE(&MOTOR_PWM_TIM_HANDLE, MOTOR_REAR_PWM_CHANNEL, duty);
}
#endif

static void motor_target_set_atomic(int16_t front, int16_t rear)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    g_motor_target_front = front;
    g_motor_target_rear = rear;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void motor_pwm_set(int16_t front, int16_t rear)
{
    motor_target_set_atomic(front, rear);
}

void motor_shift_pwm_set(int16_t front, int16_t rear)
{
    motor_target_set_atomic(front, rear);
}

void motor_pwm_output_10ms(void)
{
#if (PROJECT_HW_MODE == PROJECT_HW_MODE_MOTOR)
    motor_pwm_write_hw(g_motor_target_front, g_motor_target_rear);
#endif
}
