#include "stm32f10x.h"                  // Device header



/**
  * @brief  确认按键初始化
  * @note   初始化 PA2，配置为上拉输入
  */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
  * @brief  读取确认按键（带消抖）
  * @return 1=按下, 0=无动作
  */
uint8_t Key_GetNum(void)
{
    static uint8_t key_up = 1;  // 按键松开标志，防止重复触发
    uint8_t key = 0;
    
    // 如果之前已松开，且现在检测到按下
    if (key_up && GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == 0)
    {
        for (int i = 0; i < 20000; i++); // 软件消抖延时
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == 0) // 再次确认按下
        {
            key = 1;
            key_up = 0; // 标记已按下，等待松开
        }
    }
    else if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == 1) // 检测到松开
    {
        key_up = 1; // 恢复检测
    }
    return key;
}