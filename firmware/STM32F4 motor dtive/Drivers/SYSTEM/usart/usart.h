/**
 ****************************************************************************************************
 * @file        usart.h
 * @author      ԭŶ(ALIENTEK)
 * @version     V1.0
 * @date        2021-10-14
 * @brief       ڳʼ(һǴ1)֧printf
 * @license     Copyright (c) 2020-2032, ӿƼ޹˾
 ****************************************************************************************************
 * @attention
 *
 * ʵƽ̨ԭ F407
 * Ƶwww.yuanzige.com
 * ̳http://www.openedv.com/forum.php
 * ˾ַwww.alientek.com
 * ַzhengdianyuanzi.tmall.com
 *
 * ޸˵
 * V1.0 20211014
 * һη
 *
 ****************************************************************************************************
 */

#ifndef _USART_H
#define _USART_H

#include "stdio.h"
#include "./SYSTEM/sys/sys.h"

/*******************************************************************************************************/
/*     
 * ĬUSART1.
 * ע: ͨ޸12궨,֧USART1~UART7һ.
 */

#define USART_TX_GPIO_PORT              GPIOC
#define USART_TX_GPIO_PIN               GPIO_PIN_12
#define USART_TX_GPIO_AF                GPIO_AF8_UART5
#define USART_TX_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* ʱʹ */

#define USART_RX_GPIO_PORT              GPIOD
#define USART_RX_GPIO_PIN               GPIO_PIN_2
#define USART_RX_GPIO_AF                GPIO_AF8_UART5
#define USART_RX_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOD_CLK_ENABLE(); }while(0)   /* ʱʹ */

#define USART_UX                        UART5
#define USART_UX_IRQn                   UART5_IRQn
#define USART_UX_IRQHandler             UART5_IRQHandler
#define USART_UX_CLK_ENABLE()           do{ __HAL_RCC_UART5_CLK_ENABLE(); }while(0)  /* UART5 ʱʹ */

/*******************************************************************************************************/

#define USART_REC_LEN   200                     /* ֽ 200 */
#define USART_EN_RX     1                       /* ʹܣ1/ֹ01 */
#define RXBUFFERSIZE    1                       /* С */
#define RX_FLAG_COMPLETE 0x8000
#define RX_FLAG_0D 0x4000

#define HC15_CMD_INTERVAL  1000  // 1
#define HC15_ACK_MSG       "ok\r\n"
#define USART_PREFIX_IGNORE_TIMEOUT_MS   100U
#define USART_CMD_SUFFIX_TIMEOUT_MS      60U

extern UART_HandleTypeDef g_uart5_handle;       /* UART */

extern uint8_t  g_usart_rx_buf[USART_REC_LEN];  /* ջ,USART_REC_LENֽ.ĩֽΪз */
extern uint16_t g_usart_rx_sta;                 /* ״̬ */
extern uint8_t g_rx_buffer[RXBUFFERSIZE];       /* HALUSARTBuffer */
extern uint32_t last_send_time;  // ʱ
extern uint8_t g_send_ok_enabled;
extern volatile uint8_t g_usart_2prefix_ignore;
extern volatile uint8_t g_usart_parse_prefix_wait;
extern volatile uint32_t g_usart_last_rx_tick;

void usart_init(uint32_t baudrate);             /* ڳʼ */
void usart_protocol_process(void);


#endif







