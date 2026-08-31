/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2024/09/01
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/*
 *@Note
 *External interrupt line routine:
 *EXTI_Line4(PC4)
 *PA7 sets the pull-up input, and the rising & falling edge triggers the interrupt.
 *
 */

#include "debug.h"
#include "button.h"

/* Global define */

/* Global Variable */

volatile uint8_t event_flag=0;
volatile uint32_t press_ticks=0,release_ticks=0;


/*********************************************************************
 * @fn      EXTI4_INT_INIT
 *
 * @brief   Initializes EXTI4 collection.
 *
 * @return  none
 */
void EXTI4_INT_INIT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    EXTI_InitTypeDef EXTI_InitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_AFIO | RCC_PB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* GPIOC ----> EXTI_Line4 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource4);
    EXTI_InitStructure.EXTI_Line = EXTI_Line4;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_SetPriority(EXTI7_0_IRQn, BUTTON_IT_PRI);
    NVIC_EnableIRQ(EXTI7_0_IRQn);
}


void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      EXTI0_IRQHandler
 *
 * @brief   This function handles EXTI0 Handler.
 *
 * @return  none
 */
void EXTI7_0_IRQHandler(void)
{
    if(EXTI_GetITStatus(EXTI_Line4)!=RESET)
    {
        //printf("Run at EXTI\r\n");
        // 假设：低电平按下，高电平抬起
        if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4) == Bit_RESET) {
            // --- 按下动作 ---
            press_ticks = systemtick;
        } else {
            // --- 抬起动作 ---
            release_ticks = systemtick;
            event_flag = 1; // 标记一次完整的“按压-释放”周期完成
        }
        EXTI_ClearITPendingBit(EXTI_Line4);     /* Clear Flag */
    }
}


