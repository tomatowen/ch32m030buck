#include "encoder.h"

/*int16_t last_count = 0;
int16_t current_count = 0;
int16_t delta = 0;

void Loop() {
    current_count = (int16_t)TIM_GetCounter(TIM2);
    
    // 计算增量（转了多少格）
    // 注意：转一格物理“咔哒”声，硬件可能计了 4 个数（4倍频）
    delta = (current_count - last_count) / 4; 
    
    if (delta != 0) {
        last_count = current_count;
        // 在这里处理你的逻辑，比如数值加减
        printf("Encoder moved: %d \n", delta);
    }
}*/


/*********************************************************************
 * @fn TIM2_UP_IRQHandler
 * 
 * @brief TIM2_UP_IRQHandlerfunction handles the interrupt for TIM1 and updates the "circle" variable based
 * on the current count and auto-reload values of TIM2.
 */
/* void TIM2_UP_IRQHandler()
{
    volatile uint16_t tempcnt = TIM2->CNT, temparr = TIM2->ATRLR;
    if (TIM_GetITStatus(TIM2, TIM_IT_Update))
    {
        if (tempcnt < temparr / 2)
        {
            circle += 1;
        }
        else
        {
            circle -= 1;
        }
    }
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

} */

/*********************************************************************
 * @fn TIM2_Encoder_Init
 * 
 * @brief function initializes the TIM3 timer as an encoder with specific settings.
 * 
 * @return none
 */
void TIM2_Encoder_Init()
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA, ENABLE);
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM2, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;       // 自动重装载值
    TIM_TimeBaseStructure.TIM_Prescaler = 0x0;
    //TIM_TimeBaseStructure.TIM_Period = 80;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_EncoderInterfaceConfig(TIM2, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);

    TIM_ICStructInit(&TIM_ICInitStructure);
    //TIM_ICInitStructure.TIM_ICFilter = 10;
    TIM_ICInitStructure.TIM_ICFilter = 0x04; // 值越大滤波效果越强
    TIM_ICInit(TIM2, &TIM_ICInitStructure);

    // NVIC_SetPriority(TIM2_IRQn, 1);
    // NVIC_EnableIRQ(TIM2_IRQn);

    // TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    // TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_SetCounter(TIM2, 0);
    TIM_Cmd(TIM2, ENABLE);
}



void Encoder_Init(void)
{

    printf("Encoder INIT\r\n");
    TIM2_Encoder_Init();

}

