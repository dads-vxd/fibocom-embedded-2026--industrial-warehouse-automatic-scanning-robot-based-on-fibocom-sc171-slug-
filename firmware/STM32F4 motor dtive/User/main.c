#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LED/led.h"
#include "./BSP/KEY/key.h"
#include "./BSP/ENCODER/encoder.h"
#include "./BSP/MOTOR/motor_uart.h"
#include "./BSP/MOTOR/motor.h"
#include "./BSP/CONTROL/control.h"
#include "./BSP/HCSR04/hcsr04.h"
#include "./BSP/ATK_MS53L0/jiguang.h"
#include "demo.h"

int main(void)
{
    HAL_Init();
    sys_stm32_clock_init(336, 8, 2, 7);
    delay_init(168);

    usart_init(9600);
    led_init();

    key_init();
    motor_init();
    encoder_init();
    hcsr04_init();
    jiguang_init();

    TIM2_Init();
    demo_run();
}
