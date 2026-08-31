#include "fan.h"


/*********************************************************************
 * @fn      FanPWM_Init
 *
 * @brief   初始化风扇控制PWM (PA4/T2C4)
 *
 * @return  none
 */
void FanPWM_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA, ENABLE);
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM2, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseStructure.TIM_Period = 7200 - 1; // 72MHz / 7200 = 10KHz PWM
    TIM_TimeBaseStructure.TIM_Prescaler = 0; // 不分频
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0; // 初始占空比0
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init(TIM2, &TIM_OCInitStructure);
    TIM_OC4PreloadConfig(TIM2, TIM_OCPreload_Enable);

    TIM_Cmd(TIM2, ENABLE);
}

void FanGPIO_Init(void){
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}


void Fan_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    // 1. 开启时钟：GPIOA, TIM3, 和 AFIO (重映射需要)
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA | RCC_PB2Periph_AFIO, ENABLE);
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM3, ENABLE);

    // 2. 配置 PA4 为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 引脚重映射：将 TIM3_CH1N 映射到 PA4
    // 注意：CH32M030 的重映射宏请根据官方库确认为下述或类似
    GPIO_PinRemapConfig(GPIO_PartialRemap3_TIM3, ENABLE);

    // 4. 定时器基础配置 (10KHz @ 72MHz)
    TIM_TimeBaseStructure.TIM_Period = FAN_PWM_PERIOD;
    TIM_TimeBaseStructure.TIM_Prescaler = 0; // 不分频
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    //TIM_TimeBaseStructure.TIM_RepetitionCounter = 1;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_ClearITPendingBit(TIM3, TIM_IT_Update );

    NVIC_SetPriority(TIM3_IRQn, TIM3_UP_IT_PRI);  //优先级10，对应00，01，10，11的第三优先级
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    // 5. PWM 模式配置 (通道 1)
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable; // 主通道不输出
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable; // 开启互补通道输出 (CH1N)
    TIM_OCInitStructure.TIM_Pulse = 0;                              // 初始占空比 0
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;     // 高电平有效
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;

    TIM_OC1Init(TIM3, &TIM_OCInitStructure);
    
    // 6. 必须启用死区控制寄存器中的 MOE 位 (主输出使能)，互补通道才会有输出
    TIM_CtrlPWMOutputs(TIM3, ENABLE);

    TIM_OC1PreloadConfig( TIM3, TIM_OCPreload_Enable );
	TIM_ARRPreloadConfig( TIM3, ENABLE );

    // 7. 使能定时器
    TIM_Cmd(TIM3, ENABLE);
}

/**
 * 设置风扇占空比
 * @param duty: 0 到 7200 (对应 0% 到 100%)
 */
void Fan_SetDuty(uint16_t duty) {
    if (duty > FAN_PWM_PERIOD + 1) duty = FAN_PWM_PERIOD + 1;
    // 对应 CH1N 通道的比较寄存器
    TIM_SetCompare1(TIM3, duty);
}