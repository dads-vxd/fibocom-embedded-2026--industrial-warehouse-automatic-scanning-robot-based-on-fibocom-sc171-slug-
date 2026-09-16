#include "./BSP/ENCODER/encoder.h"

TIM_HandleTypeDef htim3;  // 全局定时器句柄
TIM_HandleTypeDef htim4;

// 全局编码器计数值定义
volatile int32_t g_encoder3_count = 0;
volatile int32_t g_encoder4_count = 0;

/* Private function prototypes */
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);

/**
  * @brief 编码器接口初始化(GPIO+定时器配置)
  */
void encoder_init(void)
{
    MX_GPIO_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);  // 启动编码器模式
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    // 初始化计数器值
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    __HAL_TIM_SET_COUNTER(&htim4, 0);
}

/**
  * @brief 更新编码器计数值（需周期性调用，如主循环/定时器中断）
  * @note 处理16位计数器溢出/下溢问题，转换为32位累计值
  */
void encoder_update_count(void)
{
    static uint16_t last_encoder3_val = 0;
    static uint16_t last_encoder4_val = 0;
    
    uint16_t curr_encoder3_val = __HAL_TIM_GET_COUNTER(&htim3);
    uint16_t curr_encoder4_val = __HAL_TIM_GET_COUNTER(&htim4);
    
    // 处理TIM3编码器计数（溢出/下溢补偿）
    int16_t diff3 = (int16_t)(curr_encoder3_val - last_encoder3_val);
    g_encoder3_count += diff3;
    last_encoder3_val = curr_encoder3_val;
    
    // 处理TIM4编码器计数（溢出/下溢补偿）
    int16_t diff4 = (int16_t)(curr_encoder4_val - last_encoder4_val);
    g_encoder4_count += diff4;
    last_encoder4_val = curr_encoder4_val;
}

/**
  * @brief GPIO初始化
  */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();  // 使能GPIOB时钟
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 配置TIM4编码器引脚
    GPIO_InitStruct.Pin = ENCODER_TIM4_PIN_CH1 | ENCODER_TIM4_PIN_CH2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;         // 复用推挽输出
    GPIO_InitStruct.Pull = ENCODER_GPIO_PULL;       // 上下拉配置
    GPIO_InitStruct.Speed = ENCODER_GPIO_SPEED;     // 速度配置
    GPIO_InitStruct.Alternate = ENCODER_TIM4_GPIO_AF;// 复用功能
    HAL_GPIO_Init(ENCODER_TIM4_GPIO_PORT, &GPIO_InitStruct);
    
    // 配置TIM3编码器引脚
    GPIO_InitStruct.Pin = ENCODER_TIM3_PIN_CH1 | ENCODER_TIM3_PIN_CH2;
    GPIO_InitStruct.Alternate = ENCODER_TIM3_GPIO_AF;// 复用功能
    HAL_GPIO_Init(ENCODER_TIM3_GPIO_PORT, &GPIO_InitStruct);
}

/**
  * @brief TIM4编码器模式初始化
  */
static void MX_TIM4_Init(void)
{
  __HAL_RCC_TIM4_CLK_ENABLE();  // 使能TIM4时钟

  TIM_Encoder_InitTypeDef Encoder_Config = {0};
  TIM_MasterConfigTypeDef MasterConfig = {0};

  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;          // 分频系数1:1
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = ENCODER_COUNTER_PERIOD;  // 16位计数器最大值
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim4);

  // 编码器接口配置
  Encoder_Config.EncoderMode = TIM_ENCODERMODE_TI12;  // 双通道模式（A/B相同时计数）
  Encoder_Config.IC1Polarity = TIM_INPUTCHANNELPOLARITY_RISING;  // A相上升沿触发
  Encoder_Config.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  Encoder_Config.IC1Prescaler = TIM_ICPSC_DIV1;
  Encoder_Config.IC1Filter = ENCODER_IC_FILTER;      // 输入滤波等级
  Encoder_Config.IC2Polarity = TIM_INPUTCHANNELPOLARITY_RISING;  // B相上升沿触发
  Encoder_Config.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  Encoder_Config.IC2Prescaler = TIM_ICPSC_DIV1;
  Encoder_Config.IC2Filter = ENCODER_IC_FILTER;
  HAL_TIM_Encoder_Init(&htim4, &Encoder_Config);

  // 主模式配置（用于触发ADC等，此处禁用）
  MasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  MasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim4, &MasterConfig);
}

static void MX_TIM3_Init(void)
{
  __HAL_RCC_TIM3_CLK_ENABLE();  // 使能TIM3时钟

  TIM_Encoder_InitTypeDef Encoder_Config = {0};
  TIM_MasterConfigTypeDef MasterConfig = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;          // 分频系数1:1
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = ENCODER_COUNTER_PERIOD;  // 16位计数器最大值
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim3);

  // 编码器接口配置
  Encoder_Config.EncoderMode = TIM_ENCODERMODE_TI12;  // 双通道模式（A/B相同时计数）
  Encoder_Config.IC1Polarity = TIM_INPUTCHANNELPOLARITY_RISING;  // A相上升沿触发
  Encoder_Config.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  Encoder_Config.IC1Prescaler = TIM_ICPSC_DIV1;
  Encoder_Config.IC1Filter = ENCODER_IC_FILTER;      // 输入滤波等级
  Encoder_Config.IC2Polarity = TIM_INPUTCHANNELPOLARITY_RISING;  // B相上升沿触发
  Encoder_Config.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  Encoder_Config.IC2Prescaler = TIM_ICPSC_DIV1;
  Encoder_Config.IC2Filter = ENCODER_IC_FILTER;
  HAL_TIM_Encoder_Init(&htim3, &Encoder_Config);

  // 主模式配置（用于触发ADC等，此处禁用）
  MasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  MasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim3, &MasterConfig);
}
