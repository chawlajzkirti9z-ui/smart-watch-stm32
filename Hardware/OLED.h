#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

/* 显存缓冲区：8页 × 128列，对外声明方便调试 */
extern uint8_t OLED_GRAM[8][128];

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Update(void);   /* 把显存刷到OLED屏幕，必须调用 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

/* ====== 新增：图形绘制 ====== */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t mode);
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t mode);
void OLED_DrawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t mode);

void OLED_DisplayOn(void);
void OLED_DisplayOff(void);

void OLED_SetBrightness(uint8_t brightness);

#endif