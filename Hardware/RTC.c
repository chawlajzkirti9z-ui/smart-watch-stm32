#include "stm32f10x.h"                  // Device header


/**
  * @brief  RTC初始化配置
  * @note   使用内部LSI作为时钟源，预分频到1Hz，从0开始计时
  */
void RTC_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    BKP_DeInit();

    RCC_LSICmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);

    RCC_RTCCLKCmd(ENABLE);
    RTC_WaitForSynchro();
    RTC_WaitForLastTask();

    RTC_SetPrescaler(40000 - 1); // LSI约40000Hz，分频到1Hz
    RTC_WaitForLastTask();

    RTC_SetCounter(0); // 从0开始计时
    RTC_WaitForLastTask();
}

/**
  * @brief  读取当前时间
  */
void RTC_Get_Time(uint8_t *hour, uint8_t *min, uint8_t *sec)
{
    uint32_t counter = RTC_GetCounter();
    *sec  = counter % 60;
    *min  = (counter / 60) % 60;
    *hour = (counter / 3600) % 24;
}

/**
  * @brief  读取当前日期
  * @note   基准日期：2000-01-01
  */
void RTC_Get_Date(uint16_t *year, uint8_t *month, uint8_t *day)
{
    uint16_t base_year = 2000;   // 基准年改为2000
    uint8_t days_in_month[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

    uint32_t total_sec = RTC_GetCounter();
    uint32_t total_days = total_sec / 86400;

    uint16_t y = base_year;
    while (1)
    {
        uint16_t days_in_year = 365;
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))
            days_in_year = 366;

        if (total_days < days_in_year)
            break;

        total_days -= days_in_year;
        y++;
    }

    *year = y;

    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))
        days_in_month[1] = 29;

    uint8_t m = 0;
    while (total_days >= days_in_month[m])
    {
        total_days -= days_in_month[m];
        m++;
    }

    *month = m + 1;
    *day   = total_days + 1;
}

/**
  * @brief  设置日期和时间
  * @note   基准日期：2000-01-01
  */
void RTC_Set_DateTime(uint16_t year, uint8_t month, uint8_t day,
                      uint8_t h, uint8_t m, uint8_t s)
{
    uint8_t days_in_month[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t total_days = 0;

    for (uint16_t y = 2000; y < year; y++)   // 起始年改为2000
    {
        total_days += 365;
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))
            total_days++;
    }

    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
        days_in_month[1] = 29;

    for (uint8_t i = 0; i < month - 1; i++)
        total_days += days_in_month[i];

    total_days += day - 1;

    uint32_t cnt = total_days * 86400 + h * 3600 + m * 60 + s;

    PWR_BackupAccessCmd(ENABLE);
    RTC_SetCounter(cnt);
    RTC_WaitForLastTask();
}