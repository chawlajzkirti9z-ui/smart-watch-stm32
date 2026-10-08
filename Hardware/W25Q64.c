#include "stm32f10x.h"

#include "W25Q64.h"

/* ==================== 引脚定义 ==================== */
#define W25Q_CS_L()   GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define W25Q_CS_H()   GPIO_SetBits(GPIOA, GPIO_Pin_4)

/* ==================== W25Q64 指令 ==================== */
#define W25Q_CMD_WRITE_ENABLE   0x06
#define W25Q_CMD_READ_STATUS    0x05
#define W25Q_CMD_READ_DATA      0x03
#define W25Q_CMD_PAGE_PROGRAM   0x02
#define W25Q_CMD_SECTOR_ERASE   0x20
#define W25Q_CMD_JEDEC_ID       0x9F

/* ==================== 存储地址分区 ==================== */
#define TIME_SAVE_ADDR     0x000000   // 扇区0：时间
#define STEPS_SAVE_ADDR    0x001000   // 扇区1：步数+日期

/* ==================== SPI 底层 ==================== */
static void SPI_Init_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    /* PA4: CS 推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA5: SCK, PA7: MOSI 复用推挽 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA6: MISO 浮空输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* SPI1 配置：主机，模式0，8位，分频8（9MHz） */
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    SPI_Cmd(SPI1, ENABLE);
    W25Q_CS_H();
}

/* SPI 收发一个字节 */
static uint8_t SPI_SwapByte(uint8_t byte)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, byte);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI1);
}

/* 等待 W25Q64 空闲 */
static void W25Q_WaitBusy(void)
{
    W25Q_CS_L();
    SPI_SwapByte(W25Q_CMD_READ_STATUS);
    while (SPI_SwapByte(0xFF) & 0x01);
    W25Q_CS_H();
}

/* 写使能 */
static void W25Q_WriteEnable(void)
{
    W25Q_CS_L();
    SPI_SwapByte(W25Q_CMD_WRITE_ENABLE);
    W25Q_CS_H();
}

/* ==================== 对外接口 ==================== */

void W25Q64_Init(void)
{
    SPI_Init_Config();
    W25Q_WaitBusy();
}

uint32_t W25Q64_ReadID(void)
{
    uint32_t id = 0;
    W25Q_CS_L();
    SPI_SwapByte(W25Q_CMD_JEDEC_ID);
    id |= (uint32_t)SPI_SwapByte(0xFF) << 16;
    id |= (uint32_t)SPI_SwapByte(0xFF) << 8;
    id |= (uint32_t)SPI_SwapByte(0xFF);
    W25Q_CS_H();
    return id;   // 期望 0xEF4017
}

void W25Q64_ReadData(uint32_t addr, uint8_t *buf, uint16_t len)
{
    W25Q_CS_L();
    SPI_SwapByte(W25Q_CMD_READ_DATA);
    SPI_SwapByte((addr >> 16) & 0xFF);
    SPI_SwapByte((addr >> 8) & 0xFF);
    SPI_SwapByte(addr & 0xFF);
    for (uint16_t i = 0; i < len; i++)
        buf[i] = SPI_SwapByte(0xFF);
    W25Q_CS_H();
}

void W25Q64_SectorErase(uint32_t addr)
{
    W25Q_WriteEnable();
    W25Q_CS_L();
    SPI_SwapByte(W25Q_CMD_SECTOR_ERASE);
    SPI_SwapByte((addr >> 16) & 0xFF);
    SPI_SwapByte((addr >> 8) & 0xFF);
    SPI_SwapByte(addr & 0xFF);
    W25Q_CS_H();
    W25Q_WaitBusy();
}

void W25Q64_PageProgram(uint32_t addr, uint8_t *buf, uint16_t len)
{
    W25Q_WriteEnable();
    W25Q_CS_L();
    SPI_SwapByte(W25Q_CMD_PAGE_PROGRAM);
    SPI_SwapByte((addr >> 16) & 0xFF);
    SPI_SwapByte((addr >> 8) & 0xFF);
    SPI_SwapByte(addr & 0xFF);
    for (uint16_t i = 0; i < len; i++)
        SPI_SwapByte(buf[i]);
    W25Q_CS_H();
    W25Q_WaitBusy();
}

/* ==================== 时间存储（扇区0） ==================== */

void W25Q64_SaveTime(uint32_t counter)
{
    uint8_t buf[4];
    buf[0] = (counter >> 24) & 0xFF;
    buf[1] = (counter >> 16) & 0xFF;
    buf[2] = (counter >> 8) & 0xFF;
    buf[3] = counter & 0xFF;

    W25Q64_SectorErase(TIME_SAVE_ADDR);
    W25Q64_PageProgram(TIME_SAVE_ADDR, buf, 4);
}

uint32_t W25Q64_ReadTime(void)
{
    uint8_t buf[4];
    W25Q64_ReadData(TIME_SAVE_ADDR, buf, 4);
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           buf[3];
}

/* ==================== 步数存储（扇区1） ==================== */

void W25Q64_SaveSteps(uint32_t steps, uint32_t date)
{
    uint8_t buf[8];
    buf[0] = (steps >> 24) & 0xFF;
    buf[1] = (steps >> 16) & 0xFF;
    buf[2] = (steps >> 8) & 0xFF;
    buf[3] = steps & 0xFF;
    buf[4] = (date >> 24) & 0xFF;
    buf[5] = (date >> 16) & 0xFF;
    buf[6] = (date >> 8) & 0xFF;
    buf[7] = date & 0xFF;

    W25Q64_SectorErase(STEPS_SAVE_ADDR);
    W25Q64_PageProgram(STEPS_SAVE_ADDR, buf, 8);
}

void W25Q64_ReadSteps(uint32_t *steps, uint32_t *date)
{
    uint8_t buf[8];
    W25Q64_ReadData(STEPS_SAVE_ADDR, buf, 8);
    *steps = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
             ((uint32_t)buf[2] << 8) | buf[3];
    *date = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
            ((uint32_t)buf[6] << 8) | buf[7];
}