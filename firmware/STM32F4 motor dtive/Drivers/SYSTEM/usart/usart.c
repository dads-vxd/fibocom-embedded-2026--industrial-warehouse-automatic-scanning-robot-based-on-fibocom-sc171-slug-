#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/CONTROL/control.h"
#include "./BSP/HCSR04/hcsr04.h"
#include "./BSP/ATK_MS53L0/jiguang.h"
#include "./BSP/MOTOR/motor_uart.h"
#include "./BSP/LED/led.h"
#include "./BSP/ENCODER/encoder.h"
#include <stdio.h>

#if SYS_SUPPORT_OS
#include "includes.h"
#endif

#if 1
#if (__ARMCC_VERSION >= 6010050)
__asm(".global __use_no_semihosting\n\t");
__asm(".global __ARM_use_no_argv \n\t");
#else
#pragma import(__use_no_semihosting)
struct __FILE
{
    int handle;
};
#endif

int _ttywrch(int ch)
{
    return ch;
}

void _sys_exit(int x)
{
    (void)x;
}

char *_sys_command_string(char *cmd, int len)
{
    (void)cmd;
    (void)len;
    return NULL;
}

FILE __stdout;

int fputc(int ch, FILE *f)
{
    (void)f;
    while ((USART_UX->SR & 0X40) == 0)
    {
    }
    USART_UX->DR = (uint8_t)ch;
    return ch;
}
#endif

#if USART_EN_RX
uint8_t g_usart_rx_buf[USART_REC_LEN];
uint8_t g_send_ok_enabled = 1U;
uint16_t g_usart_rx_sta = 0U;
uint8_t g_rx_buffer[RXBUFFERSIZE];
UART_HandleTypeDef g_uart5_handle;
uint32_t last_send_time;
volatile uint8_t g_usart_2prefix_ignore = 0U;
volatile uint8_t g_usart_parse_prefix_wait = 0U;
volatile uint32_t g_usart_last_rx_tick = 0U;

void usart_init(uint32_t baudrate)
{
    g_uart5_handle.Instance = USART_UX;
    g_uart5_handle.Init.BaudRate = baudrate;
    g_uart5_handle.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart5_handle.Init.StopBits = UART_STOPBITS_1;
    g_uart5_handle.Init.Parity = UART_PARITY_NONE;
    g_uart5_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_uart5_handle.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&g_uart5_handle);

    HAL_UART_Receive_IT(&g_uart5_handle, (uint8_t *)g_rx_buffer, RXBUFFERSIZE);
    last_send_time = HAL_GetTick();
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    if (huart->Instance == USART_UX)
    {
        USART_UX_CLK_ENABLE();
        USART_TX_GPIO_CLK_ENABLE();
        USART_RX_GPIO_CLK_ENABLE();

        gpio_init_struct.Pin = USART_TX_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_struct.Alternate = USART_TX_GPIO_AF;
        HAL_GPIO_Init(USART_TX_GPIO_PORT, &gpio_init_struct);

        gpio_init_struct.Pin = USART_RX_GPIO_PIN;
        gpio_init_struct.Alternate = USART_RX_GPIO_AF;
        HAL_GPIO_Init(USART_RX_GPIO_PORT, &gpio_init_struct);

#if USART_EN_RX
        HAL_NVIC_SetPriority(USART_UX_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(USART_UX_IRQn);
#endif
    }
}

static void usart_reset_rx_protocol_state(void);

static volatile uint8_t s_command10_reset_req = 0U;
static volatile uint8_t s_ignore_line = 0U;
static volatile uint8_t s_wait_second_byte = 0U;
static volatile uint8_t s_wait_17_selector = 0U;
static volatile uint8_t s_suffix_wait = 0U;
static volatile uint8_t s_suffix_invalid = 0U;
static volatile uint32_t s_protocol_last_tick = 0U;
static uint8_t s_deferred_motion_cmd = 0U;
static uint8_t s_goal_suffix_present = 0U;
static char s_goal_buf[12];
static uint8_t s_goal_len = 0U;

static uint8_t usart_is_digit(uint8_t ch)
{
    return ((ch >= '0') && (ch <= '9')) ? 1U : 0U;
}

static int32_t usart_parse_number_buf(const char *buf, uint8_t len)
{
    uint8_t i;
    int32_t value = 0;

    for (i = 0U; i < len; i++)
    {
        value = value * 10 + (int32_t)(buf[i] - '0');
        if (value > 1000000L)
        {
            value = 1000000L;
            break;
        }
    }

    return value;
}

static void usart_execute_motion_command(uint8_t cmd,
                                         int32_t goal_abs,
                                         uint8_t suffix_present)
{
    if (g_send_ok_enabled == 0U)
    {
        return;
    }

    if (car_protocol_command_allowed(cmd, suffix_present) == 0U)
    {
        return;
    }

#if (MOTOR_POLARITY == MOTOR_POLARITY_LEFT)
    if ((cmd == '1') && (suffix_present != 0U))
    {
        goal_abs -= MOTOR_FORWARD_LEFT_GOAL_REDUCE;
        if (goal_abs < 1)
        {
            goal_abs = 1;
        }
    }
#endif

    switch (cmd)
    {
        case '1':      /* 11 / 11+xx: forward, without IMU zero calibration */
            LED1(0);
            if (suffix_present == 0U)
            {
                /* Bare 11: use MOTION_PWM_DEFAULT from the first control period. */
                car_forward_with_goal_bare_11(goal_abs);
            }
            else
            {
                /* 11+xx: independent full-time PWM. */
                car_forward_with_goal_suffix_pwm(goal_abs);
            }
            break;

        case '5':      /* 15 / 15+xx: shift left */
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
            if (suffix_present != 0U)
            {
                car_protocol_report_lateral_by_step();
            }
#endif
            if ((suffix_present != 0U) && (goal_abs == 0))
            {
                LED1(1);
                car_cancel_motion();
                break;
            }
            LED1(0);
            car_shift_left_with_goal(goal_abs);
            break;

        case '6':      /* 16 / 16+xx: shift right */
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
            if (suffix_present != 0U)
            {
                car_protocol_report_lateral_by_step();
            }
#endif
            LED1(0);
            car_shift_right_with_goal(goal_abs);
            break;

        case '8':      /* 18 / 18+xx: backward, without IMU zero calibration */
            LED1(0);
            if (suffix_present == 0U)
            {
                /* Bare 18: use MOTION_PWM_DEFAULT from the first control period. */
                car_backward_with_goal_bare_18(goal_abs);
            }
            else
            {
                /* 18+xx: independent full-time PWM. */
                car_backward_with_goal_suffix_pwm(goal_abs);
            }
            break;

        default:
            break;
    }
}

static void usart_reset_deferred_motion(void)
{
    s_suffix_wait = 0U;
    s_suffix_invalid = 0U;
    s_deferred_motion_cmd = 0U;
    s_goal_suffix_present = 0U;
    s_goal_len = 0U;
    s_goal_buf[0] = '\0';
}

static void usart_execute_deferred_motion(void)
{
    uint8_t cmd = s_deferred_motion_cmd;
    uint8_t suffix_present = s_goal_suffix_present;
    uint8_t invalid = s_suffix_invalid;
    uint8_t goal_len = s_goal_len;
    int32_t goal_abs = 0;

    if ((suffix_present != 0U) && (s_goal_len != 0U))
    {
        goal_abs = usart_parse_number_buf(s_goal_buf, s_goal_len);
    }
    else if ((suffix_present != 0U) && (s_goal_len == 0U))
    {
        invalid = 1U;
    }

    usart_reset_deferred_motion();

    if ((invalid == 0U) && (cmd == '4'))
    {
        if ((suffix_present != 0U) && (goal_len == 1U) &&
            ((goal_abs == 0) || (goal_abs == 1)))
        {
            car_protocol_command14((uint8_t)goal_abs);
            g_usart_rx_sta = RX_FLAG_COMPLETE;
        }
        else
        {
            g_usart_rx_sta = 0U;
        }
    }
    else if (invalid == 0U)
    {
        usart_execute_motion_command(cmd, goal_abs, suffix_present);
        g_usart_rx_sta = RX_FLAG_COMPLETE;
    }
    else
    {
        g_usart_rx_sta = 0U;
    }
}

static void usart_start_deferred_motion(uint8_t cmd)
{
    usart_reset_deferred_motion();
    s_deferred_motion_cmd = cmd;
    s_protocol_last_tick = HAL_GetTick();
    s_suffix_wait = 1U;
}

static void usart_handle_deferred_suffix(uint8_t ch)
{
    s_protocol_last_tick = HAL_GetTick();

    if ((ch == '\r') || (ch == '\n'))
    {
        usart_execute_deferred_motion();
        return;
    }

    if (s_suffix_invalid != 0U)
    {
        return;
    }

    if ((ch == '+') && (s_goal_suffix_present == 0U) && (s_goal_len == 0U))
    {
        s_goal_suffix_present = 1U;
        return;
    }

    if ((s_goal_suffix_present != 0U) && (usart_is_digit(ch) != 0U))
    {
        if (s_goal_len < (sizeof(s_goal_buf) - 1U))
        {
            s_goal_buf[s_goal_len++] = (char)ch;
            s_goal_buf[s_goal_len] = '\0';
        }
        return;
    }

    /* '-' and all other suffix forms are no longer valid. Ignore this line. */
    s_suffix_invalid = 1U;
}

static void usart_execute_single_command(uint8_t ch)
{
    if (ch == '0')
    {
        s_command10_reset_req = 1U;
        return;
    }

    if (g_send_ok_enabled == 0U)
    {
        return;
    }

    switch (ch)
    {
        case '1':
            usart_execute_motion_command('1', 0, 0U);
            break;

        case '2':      /* 12: clockwise spin by encoder target */
            if (car_protocol_command_allowed('2', 0U) == 0U)
            {
                break;
            }
            LED1(0);
            car_spin_clockwise();
            break;

        case '3':      /* 13: counterclockwise spin by encoder target */
            if (car_protocol_command_allowed('3', 0U) == 0U)
            {
                break;
            }
            LED1(0);
            car_spin_counterclockwise();
            break;

        case '4':
            (void)car_protocol_command_allowed('4', 0U);
            LED1(1);
            car_cancel_motion();
            break;

        case '5':
            usart_execute_motion_command('5', 0, 0U);
            break;

        case '6':
            usart_execute_motion_command('6', 0, 0U);
            break;

        case '7':
            /* Bare 17 is no longer a complete command. 170/171/172 are
             * handled after the third byte is received.
             */
            break;

        case '8':
            usart_execute_motion_command('8', 0, 0U);
            break;

        case '9':      /* 19: final step in state 1; blocked while state 3 is parked. */
            if (car_protocol_command_allowed('9', 0U) == 0U)
            {
                break;
            }
            LED1(1);
            car_protocol_command19();
            break;

        default:
            break;
    }
}

static void usart_report_encoder_counts(void)
{
    int32_t encoder3;
    int32_t encoder4;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    encoder3 = g_encoder3_count;
    encoder4 = g_encoder4_count;
    if (primask == 0U)
    {
        __enable_irq();
    }

    printf("%ld;%ld\r\n", (long)encoder3, (long)encoder4);
}

static void usart_execute_command17_selector(uint8_t selector)
{
    if (g_send_ok_enabled == 0U)
    {
        return;
    }

    if (car_protocol_command_allowed('7', 0U) == 0U)
    {
        return;
    }

    switch (selector)
    {
        case '0':      /* 170: retain the existing right-mode numeric reply. */
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
            printf("0\r\n");
#endif
            break;

        case '1':      /* 171: left mode reports TIM3;TIM4 encoder counts. */
#if (MOTOR_POLARITY == MOTOR_POLARITY_LEFT)
            usart_report_encoder_counts();
#endif
            break;

        case '2':      /* 172: right mode reports TIM3;TIM4 encoder counts. */
#if (MOTOR_POLARITY == MOTOR_POLARITY_RIGHT)
            usart_report_encoder_counts();
#endif
            break;

        default:
            break;
    }
}

void usart_protocol_process(void)
{
    uint32_t now = HAL_GetTick();

    if (s_command10_reset_req != 0U)
    {
        s_command10_reset_req = 0U;
        car_ultrasonic_parking_reset();
        hcsr04_parking_reset();
        car_protocol_reset();
        LED1(1);
        printf("20\r\n");
    }

    if ((s_suffix_wait != 0U) &&
        ((now - s_protocol_last_tick) > USART_CMD_SUFFIX_TIMEOUT_MS))
    {
        usart_execute_deferred_motion();
    }

    if (((s_ignore_line != 0U) || (s_wait_second_byte != 0U) ||
          (s_wait_17_selector != 0U)) &&
        ((now - s_protocol_last_tick) > USART_PREFIX_IGNORE_TIMEOUT_MS))
    {
        usart_reset_rx_protocol_state();
    }
}

static void usart_reset_rx_protocol_state(void)
{
    s_ignore_line = 0U;
    s_wait_second_byte = 0U;
    s_wait_17_selector = 0U;
    g_usart_2prefix_ignore = 0U;
    g_usart_parse_prefix_wait = 0U;
    g_usart_rx_sta = 0U;
    usart_reset_deferred_motion();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint8_t ch = g_rx_buffer[0];
    uint32_t now = HAL_GetTick();

    if (huart != &g_uart5_handle)
    {
        HAL_UART_Receive_IT(&g_uart5_handle, g_rx_buffer, 1);
        return;
    }

    if (s_suffix_wait != 0U)
    {
        usart_handle_deferred_suffix(ch);
        HAL_UART_Receive_IT(&g_uart5_handle, g_rx_buffer, 1);
        return;
    }

    if (((s_ignore_line != 0U) || (s_wait_second_byte != 0U) ||
          (s_wait_17_selector != 0U)) &&
        ((now - s_protocol_last_tick) > USART_PREFIX_IGNORE_TIMEOUT_MS))
    {
        usart_reset_rx_protocol_state();
    }

    s_protocol_last_tick = now;
    g_usart_last_rx_tick = now;

    /* Protocol:
     * 10                  initialize/reset; send 20 immediately
     * 11 / 11+xx          forward; left mode uses xx-MOTOR_FORWARD_LEFT_GOAL_REDUCE
     * 12 / 13             turn until encoder target is reached
     * 14+1 / 14+0         enter state-3 parking / resume saved movement
     * 15+xx / 16+xx       right mode reports 31; left mode is silent
     * 170                 right mode retains the numeric compatibility reply
     * 171                 left mode returns TIM3;TIM4 encoder counts
     * 172                 right mode returns TIM3;TIM4 encoder counts
     * 18 / 18+xx          backward without IMU zero calibration
     * 19                  no motion; right state-1 final stage sends 22
     * right WAIT_FINAL     5 s after sending 31: auto-send 19, then send 22
     * 2... / 3...         ignore the whole line without parsing
     * minus/multi-part suffixes are invalid and ignored
     * Both modes track state 1/state 2. Right sends 21+xx/31/22; left sends
     * none of those reports. State 3 accepts only 14+0/14+1 until resumed.
     * Command 10 returns either mode to state 1.
     */
    if (s_ignore_line != 0U)
    {
        if ((ch == '\r') || (ch == '\n'))
        {
            usart_reset_rx_protocol_state();
        }
        HAL_UART_Receive_IT(&g_uart5_handle, g_rx_buffer, 1);
        return;
    }

    if (s_wait_17_selector != 0U)
    {
        s_wait_17_selector = 0U;
        g_usart_parse_prefix_wait = 0U;

        if ((ch == '0') || (ch == '1') || (ch == '2'))
        {
            usart_execute_command17_selector(ch);
            g_usart_rx_sta = RX_FLAG_COMPLETE;
        }
        else
        {
            g_usart_rx_sta = 0U;
        }

        HAL_UART_Receive_IT(&g_uart5_handle, g_rx_buffer, 1);
        return;
    }

    if (s_wait_second_byte != 0U)
    {
        s_wait_second_byte = 0U;
        g_usart_parse_prefix_wait = 0U;

        if ((ch == '1') || (ch == '4') || (ch == '5') || (ch == '6') || (ch == '8'))
        {
            usart_start_deferred_motion(ch);
        }
        else if (ch == '7')
        {
            s_wait_17_selector = 1U;
            g_usart_parse_prefix_wait = 1U;
            g_usart_rx_sta = 0U;
        }
        else if ((ch >= '0') && (ch <= '9'))
        {
            usart_execute_single_command(ch);
            g_usart_rx_sta = RX_FLAG_COMPLETE;
        }
        else if (ch == '\r')
        {
            g_usart_rx_sta = RX_FLAG_0D;
        }
        else
        {
            g_usart_rx_sta = 0U;
        }

        HAL_UART_Receive_IT(&g_uart5_handle, g_rx_buffer, 1);
        return;
    }

    if (ch == '1')
    {
        s_wait_second_byte = 1U;
        g_usart_parse_prefix_wait = 1U;
        g_usart_rx_sta = 0U;
    }
    else if ((ch == '2') || (ch == '3'))
    {
        s_ignore_line = 1U;
        g_usart_2prefix_ignore = 1U;
        g_usart_rx_sta = 0U;
    }
    else if (ch == '\r')
    {
        g_usart_rx_sta = RX_FLAG_0D;
    }
    else
    {
        g_usart_rx_sta = 0U;
    }

    HAL_UART_Receive_IT(&g_uart5_handle, g_rx_buffer, 1);
}

void USART_UX_IRQHandler(void)
{
#if SYS_SUPPORT_OS
    OSIntEnter();
#endif

    HAL_UART_IRQHandler(&g_uart5_handle);

#if SYS_SUPPORT_OS
    OSIntExit();
#endif
}
#endif
