#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f10x.h"

void MPU6050_Init(void);
uint8_t MPU6050_ReadAccel(int16_t *ax, int16_t *ay, int16_t *az);  // 0=成功, 1=失败

#endif