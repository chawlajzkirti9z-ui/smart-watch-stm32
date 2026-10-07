#include "stm32f10x.h"                  // Device header

void LED_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	GPIO_InitTypeDef GPIO_initStructure;
	GPIO_initStructure.GPIO_Mode=GPIO_Mode_Out_PP;
	GPIO_initStructure.GPIO_Pin=GPIO_Pin_1|GPIO_Pin_2;
	GPIO_initStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_initStructure);
	
	GPIO_SetBits(GPIOA,GPIO_Pin_1|GPIO_Pin_2);
}

void LED_SET(uint8_t led1_state,uint8_t led2_state)
{
	led1_state ?
	GPIO_ResetBits(GPIOA,GPIO_Pin_1) :
	GPIO_SetBits(GPIOA,GPIO_Pin_1);
	
	
	led2_state ?
	GPIO_ResetBits(GPIOA,GPIO_Pin_2) :
	GPIO_SetBits(GPIOA,GPIO_Pin_2);
}
