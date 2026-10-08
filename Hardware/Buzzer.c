#include "stm32f10x.h"


/**
  * @brief  蜂鸣器初始化
  * @note   PB0 先配成普通推挽输出并拉高，上电绝对不响
  *         只有 Buzzer_On() 时才切换成复用推挽并启动 TIM3
  */
void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // 1. 开时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    // 2. PB0 先配成普通推挽输出，输出高电平（蜂鸣器不响）
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_0);   // 高电平，不响

    // 3. 配置 TIM3
    TIM_TimeBaseStructure.TIM_Period = 369;         // ARR
    TIM_TimeBaseStructure.TIM_Prescaler = 71;       // PSC
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable;  // 先禁用输出
    TIM_OCInitStructure.TIM_Pulse = 185;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init(TIM3, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);

    // 4. 定时器不启动
    TIM_Cmd(TIM3, DISABLE);
}

/**
  * @brief  蜂鸣器响
  * @note   把 PB0 切成复用推挽，使能 PWM 输出，启动 TIM3
  */
void Buzzer_On(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // PB0 切换成复用推挽
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    TIM_CtrlPWMOutputs(TIM3, ENABLE);   // 高级定时器需要，通用定时器可省
    TIM_CCxCmd(TIM3, TIM_Channel_3, TIM_CCx_Enable);
    TIM_Cmd(TIM3, ENABLE);              // 启动定时器
}

/**
  * @brief  蜂鸣器停
  * @note   关定时器，把 PB0 切回普通推挽并拉高
  */
void Buzzer_Off(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    TIM_Cmd(TIM3, DISABLE);             // 关定时器
    TIM_CCxCmd(TIM3, TIM_Channel_3, TIM_CCx_Disable);

    // PB0 切回普通推挽，输出高电平
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_0);
}