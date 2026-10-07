#ifndef __RTC_H
#define __RTC_H

void RTC_Init(void);
void RTC_Get_Time(uint8_t *hour, uint8_t *min, uint8_t *sec);
void RTC_Set_Time(uint8_t h, uint8_t m, uint8_t s);
void RTC_Get_Date(uint16_t *year, uint8_t *month, uint8_t *day);
void RTC_Set_Date(uint16_t year, uint8_t month, uint8_t day);
void RTC_Set_DateTime(uint16_t year, uint8_t month, uint8_t day,uint8_t h, uint8_t m, uint8_t s);

#endif
