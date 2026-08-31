#ifndef __OPA_H
#define __OPA_H

#include "ch32m030.h" // 请确保引用了正确的CH32M030标准库头文件
#include "ch32m030_conf.h"

//#define REVISED_HARDWARE        1

#define TIMER1_PRESCALER_PWM    0
#define TIM1_BREAK_IT_PRI       0   //TIMER1 Brake中断优先级
#define TIM1_UP_IT_PRI          1<<6
#define TIM1_REPETITION         9   //1:每两个周期产生一次UP中断，0：每周期一次中断，2：每3个周期一次

/*TIM_CounterMode_CenterAligned1 (72000000/150000)/2 */
//TIM_CounterMode_Up 72000000/150000
#define TIMER1_PERIOD           360
#define TIM_COMPARE_CC5         TIMER1_PERIOD - 5 

//oxf->3.1V, 0x1->0.1V, 0.2V/bit，6-->1.1V，7-->1.3V
//target I: 1.6 - 5.45A*0.005ohm*55  = 0.1V，根据ADC读数再下修0.3A，估计5.15A刹车
//REVISED_HARDWARE 25毫欧*7.5A*8 = 1.5V, 1.6V-1.5V=0.1V，7.5A刹车
#define Protect_I_DACDAT              0

#define ISN1_PIN    GPIO_Pin_8 

void ADC_OPA_Configure(void);
void Timer_Configure(void);
void OPA4_Init(void);


#endif