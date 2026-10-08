#include "stm32f10x.h"
#include "MPU6050.h"

/* 引脚定义：与OLED共用 I2C 总线 (PB8=SCL, PB9=SDA) */
#define MPU_SCL_H()    GPIO_SetBits(GPIOB, GPIO_Pin_8)
#define MPU_SCL_L()    GPIO_ResetBits(GPIOB, GPIO_Pin_8)
#define MPU_SCL_READ() GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8)
#define MPU_SDA_H()    GPIO_SetBits(GPIOB, GPIO_Pin_9)
#define MPU_SDA_L()    GPIO_ResetBits(GPIOB, GPIO_Pin_9)
#define MPU_SDA_READ() GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9)

/* I2C 超时阈值：等待 ACK 最多循环 1000 次 */
#define I2C_TIMEOUT    1000

/* MPU6050 寄存器地址 */
#define MPU_ADDR         0xD0
#define MPU_PWR_MGMT_1   0x6B
#define MPU_CONFIG       0x1A
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_ACCEL_XOUT_H 0x3B

/* ==================== I2C 底层（带超时） ==================== */

/* 返回 0=成功, 1=超时失败 */
static uint8_t I2C_Start(void)
{
    MPU_SDA_H(); MPU_SCL_H();
    MPU_SDA_L(); MPU_SCL_L();
    return 0;  // Start 时序固定，无需等待
}

static void I2C_Stop(void)
{
    MPU_SDA_L(); MPU_SCL_H();
    MPU_SDA_H();
}

/* 发送一个字节，返回 0=收到ACK, 1=NACK或超时 */
static uint8_t I2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    uint16_t timeout;

    for (i = 0; i < 8; i++)
    {
        if (Byte & 0x80) MPU_SDA_H();
        else             MPU_SDA_L();
        Byte <<= 1;

        MPU_SCL_H();
        timeout = 0;
        while (MPU_SCL_READ() == 0)   // 若SCL被从机拉低，等超时
        {
            if (++timeout > I2C_TIMEOUT) return 1;
        }
        MPU_SCL_L();
    }

    /* 第9个时钟：读ACK */
    MPU_SDA_H();      // 释放 SDA
    MPU_SCL_H();
    timeout = 0;
    while (MPU_SDA_READ() == 1)   // 等待从机拉低 SDA（ACK）
    {
        if (++timeout > I2C_TIMEOUT) return 1;  // 超时：从机没响应
    }
    MPU_SCL_L();
    return 0;  // 成功收到 ACK
}

/* 接收一个字节，ack=1发送ACK，ack=0发送NACK */
static uint8_t I2C_ReceiveByte(uint8_t ack)
{
    uint8_t i, Byte = 0;
    MPU_SDA_H();  // 释放 SDA，让从机控制

    for (i = 0; i < 8; i++)
    {
        Byte <<= 1;
        MPU_SCL_H();
        if (MPU_SDA_READ()) Byte |= 0x01;
        MPU_SCL_L();
    }

    if (ack) MPU_SDA_L();   // 发送 ACK
    else     MPU_SDA_H();   // 发送 NACK
    MPU_SCL_H(); MPU_SCL_L();
    MPU_SDA_H();

    return Byte;
}

/* 写寄存器，返回 0=成功, 1=失败 */
static uint8_t MPU_WriteReg(uint8_t reg, uint8_t data)
{
    if (I2C_Start())                    return 1;
    if (I2C_SendByte(MPU_ADDR))         return 1;
    if (I2C_SendByte(reg))              return 1;
    if (I2C_SendByte(data))             return 1;
    I2C_Stop();
    return 0;
}

/* ==================== 对外接口 ==================== */

void MPU6050_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    MPU_WriteReg(MPU_PWR_MGMT_1, 0x80);   // 复位
    for (int i = 0; i < 100000; i++);
    MPU_WriteReg(MPU_PWR_MGMT_1, 0x01);   // 唤醒
    MPU_WriteReg(MPU_CONFIG, 0x06);       // 低通滤波 5Hz
    MPU_WriteReg(MPU_ACCEL_CONFIG, 0x00); // ±2g
}

/**
  * @brief  读取加速度
  * @return 0=成功, 1=失败
  */
uint8_t MPU6050_ReadAccel(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t buf[6];

    if (I2C_Start())                       return 1;
    if (I2C_SendByte(MPU_ADDR))            return 1;
    if (I2C_SendByte(MPU_ACCEL_XOUT_H))    return 1;
    if (I2C_Start())                       return 1;   // 重复起始
    if (I2C_SendByte(MPU_ADDR | 0x01))     return 1;

    buf[0] = I2C_ReceiveByte(1);
    buf[1] = I2C_ReceiveByte(1);
    buf[2] = I2C_ReceiveByte(1);
    buf[3] = I2C_ReceiveByte(1);
    buf[4] = I2C_ReceiveByte(1);
    buf[5] = I2C_ReceiveByte(0);  // 最后一个字节 NACK
    I2C_Stop();

    *ax = (int16_t)((buf[0] << 8) | buf[1]);
    *ay = (int16_t)((buf[2] << 8) | buf[3]);
    *az = (int16_t)((buf[4] << 8) | buf[5]);
    return 0;
}