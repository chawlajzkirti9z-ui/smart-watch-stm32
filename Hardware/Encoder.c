#include "stm32f10x.h"                  // Device header




// 记录上一次A、B相组合状态（00,01,10,11），用于状态机解码
static uint8_t last_state = 0;

/**
  * @brief  编码器初始化
  * @note   只初始化 PA0(A相), PA1(B相)，配置为上拉输入
  */
void Encoder_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // 上拉输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
  * @brief  读取旋转方向（4状态正交解码，彻底过滤机械抖动）
  * @return 1=顺时针, -1=逆时针, 0=无动作
  */
int8_t Encoder_GetRotate(void)
{
    // 读取A、B相电平，拼接成 2bit 状态
    uint8_t a = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
    uint8_t b = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1);
    uint8_t cur_state = (a << 1) | b;
    int8_t ret = 0;

    // 只有状态发生改变才处理
    if (cur_state != last_state)
    {
        // 顺时针状态转移：00->01->11->10->00
        if ((last_state == 0 && cur_state == 1) ||
            (last_state == 1 && cur_state == 3) ||
            (last_state == 3 && cur_state == 2) ||
            (last_state == 2 && cur_state == 0))
        {
            ret = 1;
        }
        // 逆时针状态转移：00->10->11->01->00
        else if ((last_state == 0 && cur_state == 2) ||
                 (last_state == 2 && cur_state == 3) ||
                 (last_state == 3 && cur_state == 1) ||
                 (last_state == 1 && cur_state == 0))
        {
            ret = -1;
        }
        last_state = cur_state;
    }
    return ret;
}