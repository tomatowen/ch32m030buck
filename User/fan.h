#ifndef __FAN_H
#define __FAN_H

#include "ch32m030.h" // 请确保引用了正确的CH32M030标准库头文件
#include "ch32m030_conf.h"

// 定义 PWM 周期，对应 100% 占空比
#define FAN_PWM_PERIOD  7199
#define TIM3_UP_IT_PRI  1<<7

extern volatile uint32_t systemtick;

void Fan_Init(void);
void FanGPIO_Init(void);
void Fan_SetDuty(uint16_t duty); // duty 取值 0 - 7200


#endif