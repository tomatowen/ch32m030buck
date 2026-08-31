#include "OPA.h"





/*********************************************************************
 * @fn      Timer_Configure
 *
 * @brief   timer and PWM ports initialization
 *
 * @return  none
 */
void Timer_Configure(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure={0};
    TIM_OCInitTypeDef TIM_OCInitStructure={0};
    TIM_BDTRInitTypeDef TIM_BDTRInitStructure={0};
    GPIO_InitTypeDef GPIO_InitStructure={0};

    RCC_PB2PeriphClockCmd( RCC_PB2Periph_GPIOB | RCC_PB2Periph_TIM1, ENABLE);


    GPIO_StructInit(&GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin =   GPIO_Pin_9 ;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    //FLASH->CTLR2 = 0x60;

    TIM_DeInit(TIM1);

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);

    TIM_TimeBaseStructure.TIM_Prescaler = TIMER1_PRESCALER_PWM;


    //centeraligned2，对应向上计数时产生UP中断
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_CenterAligned2;

    TIM_TimeBaseStructure.TIM_Period = TIMER1_PERIOD;

    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;

    TIM_TimeBaseStructure.TIM_RepetitionCounter = TIM1_REPETITION;   

    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    TIM_ClearITPendingBit(TIM1, TIM_IT_Update );

    TIM_OCStructInit(&TIM_OCInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCInitStructure.TIM_Pulse = 0;   // 初始占空比为 0
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);

    TIM_OCStructInit(&TIM_OCInitStructure);

    TIM_SetCompare5(TIM1, TIM_COMPARE_CC5);
    TIM_OC5PolarityConfig(TIM1, TIM_OCPolarity_Low);

    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
    
    
    TIM_BDTRInitStructure.TIM_OSSRState = TIM_OSSRState_Enable;
    TIM_BDTRInitStructure.TIM_OSSIState = TIM_OSSIState_Enable;
    TIM_BDTRInitStructure.TIM_LOCKLevel = TIM_LOCKLevel_OFF;
    TIM_BDTRInitStructure.TIM_DeadTime = 0;  // 如果不使用互补输出，死区设为0
    TIM_BDTRInitStructure.TIM_Break = TIM_Break_Enable;
    TIM_BDTRInitStructure.TIM_BreakPolarity = TIM_BreakPolarity_Low;
    TIM_BDTRInitStructure.TIM_AutomaticOutput = TIM_AutomaticOutput_Disable;

    TIM_BDTRConfig(TIM1, &TIM_BDTRInitStructure);

    NVIC_SetPriority(TIM1_UP_IRQn, TIM1_UP_IT_PRI);  //优先级00，对应00，01，10，11的最高优先级
    NVIC_EnableIRQ(TIM1_UP_IRQn);
    TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE); 

    NVIC_SetPriority(TIM1_BRK_IRQn, TIM1_BREAK_IT_PRI);  //优先级00，对应00，01，10，11的最高优先级
    NVIC_EnableIRQ(TIM1_BRK_IRQn);
    TIM_ITConfig(TIM1, TIM_IT_Break, ENABLE); 

    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_OC5PreloadConfig(TIM1, TIM_OCPreload_Disable);

    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
   

    //TIM_SetCounter(TIM1,0);
    //TIM_ClearFlag(TIM1, TIM_FLAG_Update);
    //TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);
    //TIM_ITConfig(TIM1, TIM_IT_CC4,DISABLE);
}
/*********************************************************************
 * @fn      ADC_OPA_Configure
 *
 * @brief   ADC and OPA configuration
 *
 * @para    ADCStruc: ADC sampling calculation structure pointer
 *
 * @return  none
 */
void ADC_OPA_Configure(void)
{
    OPA_ISP_InitTypeDef OPA_ISP1_InitStruct= {0};
    CMP3_InitTypeDef CMP3_InitStruct= {0};
    CMP2_InitTypeDef CMP2_InitStruct= {0};

    GPIO_InitTypeDef GPIO_InitStructure={0};

    RCC_PB2PeriphClockCmd( RCC_PB2Periph_GPIOB |RCC_PB2Periph_GPIOA, ENABLE);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_OPCM, ENABLE);
    
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_AFIO, ENABLE); // 必须开启

 

    /*配置cmp3输出通道PB4为GPIO模式*/
    //PWR->CTLR&=~(0X3<<12);
    //PWR->CTLR|=(0X1<<12);

    GPIO_StructInit(&GPIO_InitStructure);

    //GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4;
    //GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    //GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    //GPIO_Init(GPIOB, &GPIO_InitStructure);

    //手动拉高
    //GPIO_SetBits(GPIOB, GPIO_Pin_4);

    GPIO_InitStructure.GPIO_Pin =  ISN1_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;  // 模拟输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    OPA_ISP_StructInit(&OPA_ISP1_InitStruct);
#ifdef REVISED_HARDWARE
    OPA_ISP1_InitStruct.OPA_ISP_GAIN=OPA_ISP_GAIN_8;
#else
    OPA_ISP1_InitStruct.OPA_ISP_GAIN=OPA_ISP_GAIN_55;
#endif
    OPA_ISP1_InitStruct.OPA_ISP_VFBIAS=OPA_ISP_VFBIAS_1_6V;
    OPA_ISP1_InitStruct.OPA_ISP_NESL=OPA_ISP_NESL_ISN;
    OPA_ISP1_InitStruct.OPA_ISP_SEL_IO=OPA_ISP_SEL_TO_ADC_ON;
    OPA_ISP1_InitStruct.OPA_ISP_QDET=OPA_ISP_QDET_EN_OFF;
    OPA_ISP_Init(OPA3_ISP1,&OPA_ISP1_InitStruct);

    OPA_ISP_Cmd(OPA3_ISP1,ENABLE);

    CMP2_StructInit(&CMP2_InitStruct);
    CMP2_InitStruct.QII2_HYPSEL = CMP2_QII2_HYPSEL_60mv;
    CMP2_InitStruct.CMP2_BK_EN=CMP2_BK_EN_TIM1BKIN_ON;
    CMP2_InitStruct.CMP2_FILT_CFG=16;  //clock cycles signal keep
    CMP2_InitStruct.CMP2_FILT_EN=CMP2_FILT_EN_ON;
    CMP2_InitStruct.CMP2_QII2_DAC=Protect_I_DACDAT; 
    CMP2_InitStruct.CMP2_QII2_DACEN=CMP2_QII2_DACEN_ON;
    CMP2_InitStruct.CMP2_PT_IO = CMP2_PT_IO_PA13_OFF;
    CMP2_InitStruct.QII2_CHNSEL=CMP2_QII2_CHNSEL_DAC1;
    CMP2_InitStruct.QII2_CHPSEL=CMP2_QII2_CHPSEL_ISP1_Output;

    CMP2_Init(&CMP2_InitStruct);
    QII2_AE_Cmd(ENABLE);

    /*
    CMP3_InitStruct.CMP3_INT_EN = CMP3_INT_EN_OFF;
    CMP3_InitStruct.CMP3_CAP = CMP3_CAP_EN_T2_Channels_OFF;
    CMP3_InitStruct.CMP3_PSEL = CMP3_PSEL_ISP1;
    CMP3_InitStruct.CMP3_NSEL = CMP3_NSEL_DAC;
    CMP3_InitStruct.CMP3_HYS = CMP3_HYS_0mv;
    CMP3_InitStruct.CMP3_RMID = CMP3_RMID_EN_OFF;
    CMP3_InitStruct.CMP3_AT_IO = CMP3_AT_IO_ON;
    CMP3_InitStruct.CMP3_PT_IO = CMP3_PT_IO_ON;
    CMP3_InitStruct.CMP3_CH_SW_NUM = CMP3_CH_SW_NUM_0;
    CMP3_InitStruct.CMP3_PT_IO_SEL = CMP3_PT_IO_PB4;
    CMP3_InitStruct.CMP3_TRG_GATE = CMP3_TRG_GATE_OFF;
    CMP3_InitStruct.CMP3_BK_EN = CMP3_BK_EN_TIM1BKIN_ON;
    CMP3_InitStruct.CMP3_COM_EN = CMP3_COM_MODE_ON;

    CMP3_InitStruct.CMP3_DACDAT = 15; 
    CMP3_InitStruct.CMP3_OUT_CH1_POL = CMP3_OUT_CH1_POL_LOW;
    */
    
    /*CMP3_StructInit(&CMP3_InitStruct);
    CMP3_InitStruct.CMP3_TRG_GATE = CMP3_TRG_GATE_OFF;
    CMP3_InitStruct.CMP3_COM_EN = CMP3_COM_MODE_ON;
    CMP3_InitStruct.CMP3_FILT_EN = CMP3_FILT_EN_ON;
    CMP3_InitStruct.CMP3_FILT_CFG = 1;
    CMP3_InitStruct.CMP3_BK_EN = CMP3_BK_EN_TIM1BKIN_ON;
    CMP3_InitStruct.CMP3_INT_EN = CMP3_INT_EN_OFF;
    CMP3_InitStruct.CMP3_CAP = CMP3_CAP_EN_T2_Channels_OFF;   //输出到TIM2
    CMP3_InitStruct.CMP3_PSEL = CMP3_PSEL_ISP1;
    CMP3_InitStruct.CMP3_NSEL = CMP3_NSEL_DAC;
    CMP3_InitStruct.CMP3_HYS = CMP3_HYS_10mv;   //到达阈值附近的迟滞
    CMP3_InitStruct.CMP3_RMID = CMP3_RMID_EN_OFF;
    CMP3_InitStruct.CMP3_DACDAT = Protect_I_DACDAT;   //Preotect_I_DACDAT
    CMP3_InitStruct.CMP3_AT_IO = CMP3_AT_IO_ON;
    CMP3_InitStruct.CMP3_PT_IO = CMP3_PT_IO_ON;
    CMP3_InitStruct.CMP3_PT_IO_SEL = CMP3_PT_IO_PB4;
    CMP3_InitStruct.CMP3_CH_SW_NUM = CMP3_CH_SW_NUM_0;
    CMP3_InitStruct.CMP3_OUT_CH1_POL = CMP3_OUT_CH1_POL_LOW;
    CMP3_InitStruct.CMP3_OUT_CH2_POL = CMP3_OUT_CH2_POL_LOW;
    CMP3_InitStruct.CMP3_OUT_CH3_POL = CMP3_OUT_CH3_POL_LOW;
    CMP3_InitStruct.CMP3_OUT_CH4_POL = CMP3_OUT_CH4_POL_LOW;*/

    //CMP3_Init(&CMP3_InitStruct);
    
    
    //CMP3_Cmd(ENABLE); 
}

/*********************************************************************
 * @fn      OPA4_Init
 *
 * @brief   Initializes OPA4.
 *
 * @return  none
 */
void OPA4_Init(void)
{

#ifndef REVISED_HARDWARE
    OPA_ISP_InitTypeDef OPA_ISP_InitStruct = {0};
    GPIO_InitTypeDef     GPIO_InitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA, ENABLE);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_OPCM, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);


    OPA_ISP_StructInit(&OPA_ISP_InitStruct);
    OPA_ISP_InitStruct.OPA_ISP_GAIN = OPA_ISP_GAIN_4;
    OPA_ISP_InitStruct.OPA_ISP_VFBIAS = OPA_ISP_VFBIAS_0_55V;
    OPA_ISP_InitStruct.OPA_ISP_NESL = OPA_ISP_NESL_VSS;
    OPA_ISP_InitStruct.OPA_ISP_SEL_IO = OPA_ISP_SEL_TO_ADC_ON;
    OPA_ISP_InitStruct.OPA_ISP_QDET = OPA_ISP_QDET_EN_OFF;
    OPA_ISP_InitStruct.OPA_ISP_QDET_PD30K = OPA_ISP_QDET_PD30K_OFF;
    OPA_ISP_InitStruct.OPA_ISP_QDET_VBSEL = OPA_ISP_QDET_VBSEL_OFF;
    OPA_ISP_Init(OPA4_ISP2,&OPA_ISP_InitStruct);

    OPA_ISP_Cmd(OPA4_ISP2,ENABLE);
#endif

}