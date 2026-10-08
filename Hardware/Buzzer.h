#ifndef __BUZZER_H
#define __BUZZER_H



void Buzzer_Init(void);
void Buzzer_On(void);    // 打开PWM输出，蜂鸣器响
void Buzzer_Off(void);   // 关闭PWM输出，蜂鸣器停

#endif