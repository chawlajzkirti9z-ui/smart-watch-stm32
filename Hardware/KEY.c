#include "stm32f10x.h"                  // Device header


/**
  * @brief  按键初始化
  * @note   PA2=确认键，PA3=清零返回键，都配置为上拉输入
  */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // PA2：确认键
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;   // 上拉输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA3：清零并返回主界面键
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);          // 沿用上拉输入配置
}

/**
  * @brief  读取确认键（PA2）
  * @return 1=按下, 0=无动作
  * @note   带消抖，按下一次只触发一次
  */
uint8_t Key_GetNum(void)
{
    static uint8_t key_up = 1;   // 按键松开标志，防止重复触发
    uint8_t key = 0;

    if (key_up && GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == 0)
    {
        for (volatile int i = 0; i < 20000; i++);   // 消抖
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == 0)
        {
            key = 1;
            key_up = 0;          // 标记已按下，等待松开
        }
    }
    else if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == 1)
    {
        key_up = 1;              // 松开后恢复检测
    }
    return key;
}

/**
  * @brief  读取清零返回键（PA3）
  * @return 1=按下, 0=无动作
  * @note   秒表页面按此键：秒表清零并回主界面
  */
uint8_t Key2_GetNum(void)
{
    static uint8_t key_up = 1;
    uint8_t key = 0;

    if (key_up && GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) == 0)
    {
        for (volatile int i = 0; i < 20000; i++);   // 消抖
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) == 0)
        {
            key = 1;
            key_up = 0;
        }
    }
    else if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) == 1)
    {
        key_up = 1;
    }
    return key;
}