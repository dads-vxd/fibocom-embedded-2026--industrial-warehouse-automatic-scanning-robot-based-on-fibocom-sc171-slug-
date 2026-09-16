#ifndef __ENCODER_H
#define __ENCODER_H

#include "./SYSTEM/sys/sys.h"

/********************* 编码器引脚定义 *********************/
// TIM3 编码器引脚
#define ENCODER_TIM3_GPIO_PORT    GPIOB
#define ENCODER_TIM3_PIN_CH1      GPIO_PIN_4
#define ENCODER_TIM3_PIN_CH2      GPIO_PIN_5
#define ENCODER_TIM3_GPIO_AF      GPIO_AF2_TIM3

// TIM4 编码器引脚
#define ENCODER_TIM4_GPIO_PORT    GPIOB
#define ENCODER_TIM4_PIN_CH1      GPIO_PIN_6
#define ENCODER_TIM4_PIN_CH2      GPIO_PIN_7
#define ENCODER_TIM4_GPIO_AF      GPIO_AF2_TIM4

/********************* 编码器配置参数 *********************/
#define ENCODER_GPIO_PULL         GPIO_NOPULL        // 无上下拉（可根据实际硬件修改为GPIO_PULLUP/GPIO_PULLDOWN）
#define ENCODER_GPIO_SPEED        GPIO_SPEED_FREQ_HIGH // 高速
#define ENCODER_COUNTER_PERIOD    65535U             // 16位计数器最大值
#define ENCODER_IC_FILTER         0x0U               // 输入滤波器（0=无滤波，可根据需求调整）

/********************* 全局编码器数值 *********************/
// 全局编码器计数值（volatile确保每次读取最新值）
extern volatile int32_t g_encoder3_count;  // TIM3编码器计数值
extern volatile int32_t g_encoder4_count;  // TIM4编码器计数值

void encoder_init(void);
void encoder_update_count(void);           // 编码器数值更新函数

extern TIM_HandleTypeDef htim4; 
extern TIM_HandleTypeDef htim3; 

#endif
