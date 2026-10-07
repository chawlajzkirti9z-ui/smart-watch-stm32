#include "stm32f10x.h"

#include "MPU6050.h"

/* 引脚定义：与OLED共用 I2C 总线 (PB8=SCL, PB9=SDA) */
#define MPU_SCL_H()    GPIO_SetBits(GPIOB, GPIO_Pin_8)
#define MPU_SCL_L()    GPIO_ResetBits(GPIOB, GPIO_Pin_8)
#define MPU_SDA_H()    GPIO_SetBits(GPIOB, GPIO_Pin_9)
#define MPU_SDA_L()    GPIO_ResetBits(GPIOB, GPIO_Pin_9)
#define MPU_SDA_READ() GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9)

/* MPU6050 寄存器地址 */
#define MPU_ADDR         0xD0    // 0x68 << 1 | 0（AD0接地）
#define MPU_PWR_MGMT_1   0x6B    // 电源管理寄存器
#define MPU_CONFIG       0x1A    // 配置寄存器（数字低通滤波器）
#define MPU_ACCEL_CONFIG 0x1C    // 加速度计配置
#define MPU_ACCEL_XOUT_H 0x3B    // 加速度计数据起始地址

/* 软件 I2C 底层时序 */
static void I2C_Start(void)
{
    MPU_SDA_H(); MPU_SCL_H();
    MPU_SDA_L(); MPU_SCL_L();
}

static void I2C_Stop(void)
{
    MPU_SDA_L(); MPU_SCL_H();
    MPU_SDA_H();
}

static uint8_t I2C_SendByte(uint8_t Byte)
{
    uint8_t i, ack;
    for (i = 0; i < 8; i++)
    {
        if (Byte & 0x80) MPU_SDA_H(); else MPU_SDA_L();
        Byte <<= 1;
        MPU_SCL_H(); MPU_SCL_L();
    }
    MPU_SDA_H(); MPU_SCL_H();
    ack = MPU_SDA_READ(); // 读取应答
    MPU_SCL_L();
    return ack;
}

static uint8_t I2C_ReceiveByte(uint8_t ack)
{
    uint8_t i, Byte = 0;
    MPU_SDA_H(); // 释放SDA
    for (i = 0; i < 8; i++)
    {
        Byte <<= 1;
        MPU_SCL_H();
        if (MPU_SDA_READ()) Byte |= 0x01;
        MPU_SCL_L();
    }
    if (ack) MPU_SDA_L(); else MPU_SDA_H();
    MPU_SCL_H(); MPU_SCL_L();
    MPU_SDA_H();
    return Byte;
}

/* 寄存器写操作 */
static void MPU_WriteReg(uint8_t reg, uint8_t data)
{
    I2C_Start();
    I2C_SendByte(MPU_ADDR);
    I2C_SendByte(reg);
    I2C_SendByte(data);
    I2C_Stop();
}

/* ==================== 对外接口 ==================== */
void MPU6050_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    MPU_WriteReg(MPU_PWR_MGMT_1, 0x80); // 复位MPU6050
    for (int i = 0; i < 100000; i++);   // 等待复位完成
    MPU_WriteReg(MPU_PWR_MGMT_1, 0x01); // 唤醒，时钟源选PLL
    
    // 【核心修复】开启内部数字低通滤波器(DLPF)，带宽约5Hz，滤除高频噪声防乱跳
    MPU_WriteReg(MPU_CONFIG, 0x06); 
    
    MPU_WriteReg(MPU_ACCEL_CONFIG, 0x00); // 加速度量程±2g
}

void MPU6050_ReadAccel(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t buf[6];
    // 读取6个字节(0x3B~0x40)，高字节在前
    I2C_Start();
    I2C_SendByte(MPU_ADDR);
    I2C_SendByte(MPU_ACCEL_XOUT_H);
    I2C_Start(); // 重复起始
    I2C_SendByte(MPU_ADDR | 0x01); // 读
    buf[0] = I2C_ReceiveByte(1);
    buf[1] = I2C_ReceiveByte(1);
    buf[2] = I2C_ReceiveByte(1);
    buf[3] = I2C_ReceiveByte(1);
    buf[4] = I2C_ReceiveByte(1);
    buf[5] = I2C_ReceiveByte(0); // NACK
    I2C_Stop();

    // 拼接成有符号16位
    *ax = (int16_t)((buf[0] << 8) | buf[1]);
    *ay = (int16_t)((buf[2] << 8) | buf[3]);
    *az = (int16_t)((buf[4] << 8) | buf[5]);
}