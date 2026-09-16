#ifndef __MOTOR_UART_H
#define __MOTOR_UART_H

#include "./SYSTEM/sys/sys.h"

#define PROJECT_HW_MODE_LCD                  0
#define PROJECT_HW_MODE_MOTOR                1
#define PROJECT_HW_MODE                      PROJECT_HW_MODE_MOTOR

#if ((PROJECT_HW_MODE != PROJECT_HW_MODE_LCD) && (PROJECT_HW_MODE != PROJECT_HW_MODE_MOTOR))
#error "PROJECT_HW_MODE must be PROJECT_HW_MODE_LCD(0) or PROJECT_HW_MODE_MOTOR(1)"
#endif

#define MOTOR_FRONT_DIR_GPIO_PORT            GPIOE
#define MOTOR_FRONT_DIR_GPIO_PIN             GPIO_PIN_6
#define MOTOR_FRONT_DIR_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOE_CLK_ENABLE(); }while(0)

#define MOTOR_REAR_DIR_GPIO_PORT             GPIOE
#define MOTOR_REAR_DIR_GPIO_PIN              GPIO_PIN_12
#define MOTOR_REAR_DIR_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOE_CLK_ENABLE(); }while(0)

#define MOTOR_POLARITY_LEFT                  1
#define MOTOR_POLARITY_RIGHT                 -1
#define MOTOR_POLARITY                       MOTOR_POLARITY_RIGHT

#if ((MOTOR_POLARITY != MOTOR_POLARITY_LEFT) && (MOTOR_POLARITY != MOTOR_POLARITY_RIGHT))
#error "MOTOR_POLARITY must be MOTOR_POLARITY_LEFT(1) or MOTOR_POLARITY_RIGHT(-1)"
#endif

/* PWM output compensation is disabled for both hardware modes. */
#define MOTOR_PWM_LEFT_MODE_OFFSET           0
#define MOTOR_PWM_RIGHT_MODE_OFFSET          0

#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
#define MOTOR_PWM_ACTIVE_MODE_OFFSET         MOTOR_PWM_RIGHT_MODE_OFFSET
#else
#define MOTOR_PWM_ACTIVE_MODE_OFFSET         MOTOR_PWM_LEFT_MODE_OFFSET
#endif

#define MOTOR_GOAL_LEFT_MODE_ABS             9900
#define MOTOR_GOAL_RIGHT_MODE_ABS            10000

/* Encoder target for command 12/13 rotation. Calibrate the left and right
 * vehicle controllers independently; both TIM3/TIM4 encoder increments are
 * converted to absolute values and averaged every 10 ms.
 */
#define MOTOR_SPIN_GOAL_LEFT_MODE_ABS        10400
#define MOTOR_SPIN_GOAL_RIGHT_MODE_ABS       10400

/* Left mode: command 11+xx uses xx minus this adjustable encoder amount. */
#define MOTOR_FORWARD_LEFT_GOAL_REDUCE       0

#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
#define MOTOR_GOAL_ABS                       MOTOR_GOAL_RIGHT_MODE_ABS
#define MOTOR_SPIN_GOAL_ABS                  MOTOR_SPIN_GOAL_RIGHT_MODE_ABS
#else
#define MOTOR_GOAL_ABS                       MOTOR_GOAL_LEFT_MODE_ABS
#define MOTOR_SPIN_GOAL_ABS                  MOTOR_SPIN_GOAL_LEFT_MODE_ABS
#endif

#define MOTOR_DEFAULT_GOAL                   MOTOR_GOAL_ABS

/* All four replacement motors rotate opposite to the previous motors.
 * Swap both hardware direction mappings so protocol motion directions remain unchanged.
 */
#define MOTOR_FRONT_FORWARD_LEVEL            GPIO_PIN_SET
#define MOTOR_FRONT_REVERSE_LEVEL            GPIO_PIN_RESET
#define MOTOR_REAR_FORWARD_LEVEL             GPIO_PIN_RESET
#define MOTOR_REAR_REVERSE_LEVEL             GPIO_PIN_SET

#define MOTOR_FRONT_DIR_DEFAULT_LEVEL        GPIO_PIN_SET
#define MOTOR_REAR_DIR_DEFAULT_LEVEL         GPIO_PIN_SET

#define MOTOR_PWM_TIM                        TIM1
#define MOTOR_PWM_TIM_HANDLE                 htim1
#define MOTOR_PWM_TIM_CLK_ENABLE()           do{ __HAL_RCC_TIM1_CLK_ENABLE(); }while(0)
#define MOTOR_PWM_TIM_CLK_DISABLE()          do{ __HAL_RCC_TIM1_CLK_DISABLE(); }while(0)

#define MOTOR_PWM_PRESCALER                  0
#define MOTOR_PWM_PERIOD                     9881
#define MOTOR_PWM_MAX_DUTY                   9880
#define MOTOR_PWM_INIT_DUTY                  0

#define MOTOR_FRONT_PWM_CHANNEL              TIM_CHANNEL_1
#define MOTOR_FRONT_PWM_GPIO_PORT            GPIOE
#define MOTOR_FRONT_PWM_GPIO_PIN             GPIO_PIN_9
#define MOTOR_FRONT_PWM_GPIO_AF              GPIO_AF1_TIM1
#define MOTOR_FRONT_PWM_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOE_CLK_ENABLE(); }while(0)

#define MOTOR_REAR_PWM_CHANNEL               TIM_CHANNEL_4
#define MOTOR_REAR_PWM_GPIO_PORT             GPIOE
#define MOTOR_REAR_PWM_GPIO_PIN              GPIO_PIN_14
#define MOTOR_REAR_PWM_GPIO_AF               GPIO_AF1_TIM1
#define MOTOR_REAR_PWM_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOE_CLK_ENABLE(); }while(0)

#define MOTOR_DIR_GPIO_MODE                  GPIO_MODE_OUTPUT_PP
#define MOTOR_DIR_GPIO_PULL                  GPIO_NOPULL
#define MOTOR_DIR_GPIO_SPEED                 GPIO_SPEED_FREQ_HIGH

#define MOTOR_PWM_GPIO_MODE                  GPIO_MODE_AF_PP
#define MOTOR_PWM_GPIO_PULL                  GPIO_NOPULL
#define MOTOR_PWM_GPIO_SPEED                 GPIO_SPEED_FREQ_LOW

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
void motor_init(void);

#endif
