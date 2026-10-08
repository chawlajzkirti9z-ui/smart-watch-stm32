#include "stm32f10x.h"
#include "OLED_Font.h"
#include "OLED.h"

/*引脚配置*/
#define OLED_W_SCL(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_8, (BitAction)(x))
#define OLED_W_SDA(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_9, (BitAction)(x))

/* ====== 新增：显存缓冲区，8页×128列 ====== */
uint8_t OLED_GRAM[8][128];

/*引脚初始化*/
void OLED_I2C_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_W_SDA(0);
	OLED_W_SCL(0);
}

void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(!!(Byte & (0x80 >> i)));
		OLED_W_SCL(1);
		OLED_W_SCL(0);
	}
	OLED_W_SCL(1);
	OLED_W_SCL(0);
}

void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);
	OLED_I2C_SendByte(0x00);
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}

void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);
	OLED_I2C_SendByte(0x40);
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}

void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
	OLED_WriteCommand(0x00 | (X & 0x0F));
}

/* ==================== 新增：把显存刷到屏幕 ==================== */
void OLED_Update(void)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        OLED_SetCursor(page, 0);
        for (uint8_t col = 0; col < 128; col++)
        {
            OLED_WriteData(OLED_GRAM[page][col]);
        }
    }
}

/* ==================== 清屏：只清显存，不清屏幕 ==================== */
void OLED_Clear(void)
{  
    for (uint8_t page = 0; page < 8; page++)
        for (uint8_t col = 0; col < 128; col++)
            OLED_GRAM[page][col] = 0x00;
}

/* ==================== 显示字符：写入显存 ==================== */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
    uint8_t page = (Line - 1) * 2;
    uint8_t col = (Column - 1) * 8;
    for (uint8_t i = 0; i < 8; i++)
    {
        OLED_GRAM[page][col + i] = OLED_F8x16[Char - ' '][i];
        OLED_GRAM[page + 1][col + i] = OLED_F8x16[Char - ' '][i + 8];
    }
}

void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(Line, Column + i, String[i]);
	}
}

uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i;
	uint32_t Number1;
	if (Number >= 0)
	{
		OLED_ShowChar(Line, Column, '+');
		Number1 = Number;
	}
	else
	{
		OLED_ShowChar(Line, Column, '-');
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)							
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
		}
	}
}

void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
	}
}

/* ==================== 新增：画点 ==================== */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t mode)
{
    if (x > 127 || y > 63) return;
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    if (mode)
        OLED_GRAM[page][x] |= (1 << bit);
    else
        OLED_GRAM[page][x] &= ~(1 << bit);
}

/* ==================== 新增：画线（Bresenham算法） ==================== */
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t mode)
{
    int16_t dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int16_t dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t x = x1, y = y1;

    while (1)
    {
        OLED_DrawPoint(x, y, mode);
        if (x == x2 && y == y2) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
    }
}

/* ==================== 新增：画矩形框 ==================== */
void OLED_DrawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t mode)
{
    OLED_DrawLine(x1, y1, x2, y1, mode);
    OLED_DrawLine(x2, y1, x2, y2, mode);
    OLED_DrawLine(x2, y2, x1, y2, mode);
    OLED_DrawLine(x1, y2, x1, y1, mode);
}

/* ==================== 初始化 ==================== */
void OLED_Init(void)
{
	uint32_t i, j;
	
	for (i = 0; i < 1000; i++)
	{
		for (j = 0; j < 1000; j++);
	}
	
	OLED_I2C_Init();
	
	OLED_WriteCommand(0xAE);
	OLED_WriteCommand(0xD5);
	OLED_WriteCommand(0x80);
	OLED_WriteCommand(0xA8);
	OLED_WriteCommand(0x3F);
	OLED_WriteCommand(0xD3);
	OLED_WriteCommand(0x00);
	OLED_WriteCommand(0x40);
	OLED_WriteCommand(0xA1);
	OLED_WriteCommand(0xC8);
	OLED_WriteCommand(0xDA);
	OLED_WriteCommand(0x12);
	OLED_WriteCommand(0x81);
	OLED_WriteCommand(0xCF);
	OLED_WriteCommand(0xD9);
	OLED_WriteCommand(0xF1);
	OLED_WriteCommand(0xDB);
	OLED_WriteCommand(0x30);
	OLED_WriteCommand(0xA4);
	OLED_WriteCommand(0xA6);
	OLED_WriteCommand(0x8D);
	OLED_WriteCommand(0x14);
	OLED_WriteCommand(0xAF);
		
	OLED_Clear();
	OLED_Update();  /* 清完显存后刷一次屏 */
}

/**
  * @brief  打开OLED显示
  */
void OLED_DisplayOn(void)
{
    OLED_WriteCommand(0xAF);
}

/**
  * @brief  关闭OLED显示（息屏）
  * @note   只是不显示，GRAM内容不变，下次唤醒时还是原画面
  */
void OLED_DisplayOff(void)
{
    OLED_WriteCommand(0xAE);
}

/**
  * @brief  设置OLED亮度（对比度）
  * @param  brightness: 0~255，越大越亮
  */
void OLED_SetBrightness(uint8_t brightness)
{
    OLED_WriteCommand(0x81);      // 对比度控制命令
    OLED_WriteCommand(brightness); // 对比度值
}