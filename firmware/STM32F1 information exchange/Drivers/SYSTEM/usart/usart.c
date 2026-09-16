#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"

#if SYS_SUPPORT_OS
#include "os.h"
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
    ch = ch;
    return ch;
}

void _sys_exit(int x)
{
    x = x;
}

char *_sys_command_string(char *cmd, int len)
{
    cmd = cmd;
    len = len;
    return NULL;
}

FILE __stdout;

int fputc(int ch, FILE *f)
{
    f = f;
    while ((USART_UX->SR & 0X40) == 0) {
    }

    USART_UX->DR = (uint8_t)ch;
    return ch;
}
#endif

#if USART_EN_RX

uint8_t g_usart_rx_buf[USART_REC_LEN];
uint16_t g_usart_rx_sta = 0;
uint8_t g_rx_buffer[RXBUFFERSIZE];
UART_HandleTypeDef g_uart2_handle;

static volatile uint8_t g_eye_cmd = 0u;
static volatile uint8_t g_eye_first_digit = 0u;
static volatile uint8_t g_eye_digit_count = 0u;
static volatile uint8_t g_eye_frame_done = 0u;
static volatile uint32_t g_eye_last_rx_tick = 0u;

/* At 9600 baud, characters in one send are about 1 ms apart.
 * A gap longer than this starts a new command frame when no CR/LF is sent. */
#define EYE_CMD_FRAME_GAP_MS    30u

static void usart_eye_parser_reset(void)
{
    g_eye_first_digit = 0u;
    g_eye_digit_count = 0u;
    g_eye_frame_done = 0u;
}

uint8_t usart_get_eye_cmd(void)
{
    uint8_t cmd;

    __disable_irq();
    cmd = g_eye_cmd;
    g_eye_cmd = 0;
    __enable_irq();

    return cmd;
}

void usart_init(uint32_t baudrate)
{
    g_uart2_handle.Instance = USART_UX;
    g_uart2_handle.Init.BaudRate = baudrate;
    g_uart2_handle.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart2_handle.Init.StopBits = UART_STOPBITS_1;
    g_uart2_handle.Init.Parity = UART_PARITY_NONE;
    g_uart2_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_uart2_handle.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&g_uart2_handle);

    HAL_UART_Receive_IT(&g_uart2_handle, (uint8_t *)g_rx_buffer, RXBUFFERSIZE);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio_init_struct;

    if (huart->Instance == USART_UX) {
        USART_TX_GPIO_CLK_ENABLE();
        USART_RX_GPIO_CLK_ENABLE();
        USART_UX_CLK_ENABLE();

        gpio_init_struct.Pin = USART_TX_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(USART_TX_GPIO_PORT, &gpio_init_struct);

        gpio_init_struct.Pin = USART_RX_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_INPUT;
        gpio_init_struct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(USART_RX_GPIO_PORT, &gpio_init_struct);

        HAL_NVIC_SetPriority(USART_UX_IRQn, 3, 3);
        HAL_NVIC_EnableIRQ(USART_UX_IRQn);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint8_t data;
    uint16_t rx_len;
    uint32_t now;

    if (huart->Instance == USART_UX) {
        data = g_rx_buffer[0];
        now = HAL_GetTick();

        /* A CR/LF or an inter-frame gap releases the parser for the next send. */
        if ((data == '\r') || (data == '\n')) {
            usart_eye_parser_reset();
            g_eye_last_rx_tick = 0u;
        } else {
            if ((g_eye_last_rx_tick != 0u) &&
                ((uint32_t)(now - g_eye_last_rx_tick) > EYE_CMD_FRAME_GAP_MS)) {
                usart_eye_parser_reset();
            }
            g_eye_last_rx_tick = now;

            /* Only the first two decimal digits in one frame are used.
             * After two digits are collected, all remaining bytes are ignored
             * until CR/LF or a new frame gap. Examples:
             * 1118 -> 11, 11+140 -> 11, 11+180-140 -> 11, 1218 -> 12. */
            if ((g_eye_frame_done == 0u) && (data >= '0') && (data <= '9')) {
                if (g_eye_digit_count == 0u) {
                    g_eye_first_digit = (uint8_t)(data - '0');
                    g_eye_digit_count = 1u;
                } else {
                    g_eye_cmd = (uint8_t)(g_eye_first_digit * 10u + (uint8_t)(data - '0'));
                    g_eye_digit_count = 0u;
                    g_eye_frame_done = 1u;
                }
            }
        }

        rx_len = g_usart_rx_sta & 0x3FFF;
        if (rx_len < USART_REC_LEN) {
            g_usart_rx_buf[rx_len] = data;
            rx_len++;
            g_usart_rx_sta = rx_len;
        } else {
            g_usart_rx_sta = 0;
        }

        HAL_UART_Receive_IT(&g_uart2_handle, (uint8_t *)g_rx_buffer, RXBUFFERSIZE);
    }
}

void USART_UX_IRQHandler(void)
{
#if SYS_SUPPORT_OS
    OSIntEnter();
#endif

    HAL_UART_IRQHandler(&g_uart2_handle);

#if SYS_SUPPORT_OS
    OSIntExit();
#endif
}

#endif
