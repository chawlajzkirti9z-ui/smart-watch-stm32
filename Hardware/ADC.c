#include "stm32f10x.h"                  // Device header


#include "ADC.h"

/**
  * @brief  ADC初始化配置
  * @note   使用 PA3 (ADC通道3) 接电位器
  */
void ADC_Init_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef  ADC_InitStructure;

    // 1. 开时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    // 2. 配置 PA3 为模拟输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 配置 ADC1
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // 4. 配置通道3，采样时间55.5周期
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 1, ADC_SampleTime_55Cycles5);

    // 5. 使能并校准
    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

/**
  * @brief  获取电池电量百分比
  * @return 0~100
  * @note   直接用原始值映射，分母改小到3800，保证电位器拧到底能到100%
  */
uint8_t ADC_GetBatteryPercent(void)
{
    uint16_t val;
    uint32_t percent;

    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    val = ADC_GetConversionValue(ADC1);

    // 用3800做分母，超过100%截断为100
    percent = (uint32_t)val * 100 / 3800;
    if (percent > 100) percent = 100;
    return (uint8_t)percent;
}