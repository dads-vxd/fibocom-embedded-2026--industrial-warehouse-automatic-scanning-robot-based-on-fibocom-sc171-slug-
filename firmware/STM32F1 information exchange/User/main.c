#include "stm32f1xx_hal.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/HUB75/hub75.h"
#include "./APP/QEYES/qeyes.h"

int main(void)
{
    HAL_Init();

    /* 正点原子 STM32F1 HAL 模板常用写法。若你的模板函数参数不同，见 README 说明。 */
    sys_stm32_clock_init(RCC_PLL_MUL9);      /* 8MHz HSE -> 72MHz */
    delay_init(72);
    usart_init(9600);

    hub75_init();
    hub75_set_brightness(90);                /* 先用低亮度，确认供电稳定后可调到 120~180 */
    qeyes_init();

    while (1) {
        hub75_display_scan();                /* 必须高频调用，不能加 HAL_Delay */
        qeyes_task();                        /* 非阻塞表情任务；哭哭眼与眨眼均按160ms节拍更新 */
    }
}
