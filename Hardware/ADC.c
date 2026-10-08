#include "stm32f10x.h"                  // Device header

#include "ADC.h"

/**
  * @brief  ADC初始化配置
  * @note   使用 PB1 (ADC通道9) 接电位器模拟电池电压
  *         注意：原来用的是PA3，改成PA3当按键后，ADC挪到PB1
  */
void ADC_Init_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef  ADC_InitStructure;

    // 1. 开时钟：PB1属于GPIOB，ADC1挂APB2
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);   // 改成GPIOB
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    // 2. 配置 PB1 为模拟输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;               // PB1
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;           // 模拟输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);                  // GPIOB

    // 3. 配置 ADC1 基本参数
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;           // 独立模式
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;                // 单通道
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;          // 单次转换
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;  // 软件触发
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;       // 数据右对齐
    ADC_InitStructure.ADC_NbrOfChannel = 1;                      // 1个通道
    ADC_Init(ADC1, &ADC_InitStructure);

    // 4. 配置通道9（对应PB1），采样时间55.5周期
    ADC_RegularChannelConfig(ADC1, ADC_Channel_9, 1, ADC_SampleTime_55Cycles5);

    // 5. 使能 ADC1
    ADC_Cmd(ADC1, ENABLE);

    // 6. ADC校准（不校准精度很差）
    ADC_ResetCalibration(ADC1);                      // 复位校准
    while (ADC_GetResetCalibrationStatus(ADC1));     // 等待复位完成
    ADC_StartCalibration(ADC1);                      // 开始校准
    while (ADC_GetCalibrationStatus(ADC1));          // 等待校准完成
}

/**
  * @brief  获取电池电量百分比
  * @return 0~100
  * @note   直接读ADC原始值，用3800做分母，保证电位器拧到底能到100%
  */
uint8_t ADC_GetBatteryPercent(void)
{
    uint16_t val;
    uint32_t percent;

    // 触发转换并等待完成
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    val = ADC_GetConversionValue(ADC1);

    // 原始值(0~4095) 映射到 0~100%
    percent = (uint32_t)val * 100 / 3800;
    if (percent > 100) percent = 100;
    return (uint8_t)percent;
}
