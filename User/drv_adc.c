/**
 * @file    drv_adc.c
 * @brief   ADC 多通道采样驱动源文件
 * @version 1.0
 * @date    2024-01-01
 */

#include "drv_adc.h"


/* ==================== 全局变量定义 ==================== */

/* DMA 缓冲区 (与 TIM1 中断共享) */
volatile uint16_t g_adc_dma_buffer[ADC_DMA_BUFFER_SIZE] = {0};

/* 数据处理缓冲区 */
ADC_Data_Buffer_t g_adc_data = {0};

/* 通道配置表 */
static const ADC_Channel_Config_t s_adc_channels[ADC_CHANNEL_COUNT] = {
    //{0,  ADC_CURRENT_PORT, ADC_CURRENT_PIN, ADC_Channel_0},   // 电流 (需确认实际通道)
    //{9,  ADC_VOUT_PORT,    ADC_VOUT_PIN,    ADC_Channel_9},   // 输出电压 PA14
    {19, ADC_VIN_PORT,     ADC_VIN_PIN,     ADC_Channel_19},  // 输入电压 PA12
    {18, ADC_V12_PORT,     ADC_V12_PIN,     ADC_Channel_18},  // 12V 供电 PA13
    {8,  ADC_NTC_PORT,     ADC_NTC_PIN,     ADC_Channel_8},   // NTC 温度 PA11
};

/* 分压电阻配置 (上拉 KΩ, 下拉 KΩ) */
static const struct {
    uint32_t top;     // 上拉电阻
    uint32_t bottom;  // 下拉电阻
} s_voltage_divider[ADC_CHANNEL_COUNT] = {
    //{0, 0},       // 电流通道 (特殊处理)
    //{30, 22},     // 输出电压 (30K + 2.2K)
    {100, 10},    // 输入电压 (需根据实际调整)
    {18, 47},     // 12V 供电 (18K + 4.7K)
    {10, 10},     // NTC (10K + NTC)
};


// 预先将浮点系数转换为整型
// 假设 Kp=1.2, Ki=0.5, Kd=0.1
// 预设 Kp=0.005, Ki=0.0001, Kd=0 作为起始值更加安全
int32_t V_Kp = (int32_t)(0.001 * PID_SCALE );
int32_t V_Ki = (int32_t)(1 * PID_SCALE  );
int32_t V_Kd = (int32_t)(0 * PID_SCALE );

//电流环参数kp略大，以便增大阻尼，ki略小，降低过冲
int32_t I_Kp = (int32_t)(0.001 * PID_SCALE );
int32_t I_Ki = (int32_t)(0.02 * PID_SCALE);
int32_t I_Kd = (int32_t)(0.0 * PID_SCALE );

// 目标值
int32_t Target_V = 0; // 目标电压 ADC 值
int32_t Target_I = OFFSET_1V_6_ADC; // 限制电流 ADC 值

// 电压环状态
volatile int32_t V_Ek = 0, V_Ek_1 = 0, V_Ek_2 = 0;
volatile int32_t V_delta,I_delta;
// 电流环状态
volatile int32_t I_Ek = 0, I_Ek_1 = 0, I_Ek_2 = 0;
volatile uint8_t current_flag = 0;
volatile int32_t voltage_buffer = 0,current_buffer = 0;
static volatile int32_t filtered_adc_v = 0,filtered_adc_i = 0;
volatile uint8_t output_enabled = 0;

volatile uint8_t led_color=1;
volatile uint8_t led_updated = 0;

// 定义全局变量，由电压环更新，电流环消费
static volatile int32_t Target_I_Ref = 0; 

static volatile uint8_t pid_initialized = 0;
static volatile uint8_t Dead_Zone_Flag = 0,Dead_Zone_Current_Flag = 0; 

static WindowSumChecker pid_checker;
static IntWindowChecker stable_checker;

static int32_t smoothed_ff_pwm = 0; // 放大 1024 倍的平滑前馈值

/* ==================== 静态函数声明 ==================== */
static void ADC_GPIO_Config(void);
static void ADC_DMA_Config(void);
static void ADC_Peripheral_Config(void);
static void ADC_NVIC_Config(void);
static void ADC_ProcessData(void);
static int32_t NTC_CalculateTemperature(uint16_t adc_val);
static int32_t Convert_To_ADC_I(int32_t);

/* ==================== 初始化函数 ==================== */

/**
 * @brief 初始化检查器
 */
void WindowChecker_Init(WindowSumChecker *chk) {
    for (int i = 0; i < WINDOW_SIZE; i++) {
        chk->buffer[i] = 0;
    }
    chk->head = 0;
    chk->count = 0;
    chk->current_sum = 0;
}

/**
 * @brief 在 PID 中断中调用的更新函数
 * @param chk 结构体指针
 * @param val 当前的变量值
 * @return true 表示满足条件，false 表示不满足
 */
bool WindowChecker_Update(WindowSumChecker *chk, float val) {
    // 1. 范围判断：判断变量是否在 1 和 -1 之间（包含边界）
    if (val < -1 || val > 1) {
        // 超出范围，根据需求处理（这里直接返回不满足或重置）
        return false; 
    }

    // 2. 如果窗口已经满了 10 个，需要减去即将被覆盖掉的“最老的那次数据”
    if (chk->count == WINDOW_SIZE) {
        chk->current_sum -= chk->buffer[chk->head];
    } else {
        // 还没满 10 次时，计数器自增
        chk->count++;
    }

    // 3. 将新数据存入当前头部，并累加到总和中
    chk->buffer[chk->head] = val;
    chk->current_sum += val;

    // 4. 移动循环指针（到达 10 后归零）
    chk->head++;
    if (chk->head >= WINDOW_SIZE) {
        chk->head = 0;
    }

    // 5. 判断条件：必须满 10 次，且总和的绝对值小于 2
    if (chk->count == WINDOW_SIZE) {
        // 使用浮点数绝对值函数 fabsf (如果在单片机里，也可以用宏定义或位操作优化)
        if (chk->current_sum >= -2 && chk->current_sum <= 2) {
            return true; // 符合“过去十次总和绝对值小于2”
        }
    }

    return false;
}

/**
 * @brief 初始化/重置检查器
 */
void StrictAbsChecker_Init(StrictAbsChecker *chk) {
    for (int i = 0; i < WINDOW_SIZE; i++) {
        chk->is_abs_one[i] = 0;
    }
    chk->head = 0;
    chk->count = 0;
    chk->sum_abs_ones = 0;
    chk->initialized = true;
}

/**
 * @brief 在 PID 中断中调用的更新函数
 * @param chk 结构体指针
 * @param val 当前的变量值
 * @return true 表示满足条件（满10次且绝对值为1的次数 >= 8），false 表示不满足
 */
bool StrictAbsChecker_Update(StrictAbsChecker *chk, int32_t val) {
    // 1. 越界检查：每次出现大于 1 或小于 -1 时，重新初始化结构体
    if (val < -1 || val > 1) {
        if(!chk->initialized) StrictAbsChecker_Init(chk);
        return false; 
    }

    // 2. 判断当前值的绝对值是否等于 1（由于是浮点数，加一个微小容差 0.01，或者如果是整型/精确值可以直接比）
    uint8_t current_is_one = (val == 1) || (val == -1)? 1 : 0;

    // 3. 如果窗口已满 10 个，滑出最老的一个数据
    if (chk->count == WINDOW_SIZE) {
        chk->sum_abs_ones -= chk->is_abs_one[chk->head];
    } else {
        chk->count++;
    }

    // 4. 写入新数据并更新总次数
    chk->is_abs_one[chk->head] = current_is_one;
    chk->sum_abs_ones += current_is_one;

    // 5. 移动循环指针
    chk->head++;
    if (chk->head >= WINDOW_SIZE) {
        chk->head = 0;
    }

    // 6. 判断条件：必须满 10 次，且绝对值为 1 的次数 >= 8
    if (chk->count == WINDOW_SIZE && chk->sum_abs_ones >= 8) {
        return true; 
    }

    return false;
}

/**
 * @brief 初始化检查器
 */
void IntWindowChecker_Init(IntWindowChecker *chk) {
    for (int i = 0; i < WINDOW_SIZE; i++) {
        chk->buffer[i] = 0;
    }
    chk->head = 0;
    chk->count = 0;
    chk->initialized = true;
}

/**
 * @brief 在 PID 中断中调用的更新函数
 * @param chk 结构体指针
 * @param val 当前的整型变量值
 * @return true 表示过去 10 次全部都是 0, 1, -1 且满 10 次，false 表示有越界或未满 10 次
 */
bool IntWindowChecker_Update(IntWindowChecker *chk, int val) {
    // 1. 如果当前值不是 0、1 或 -1，说明不符合要求，立刻重置并返回 false
    if (abs(val) > 1) {
        if(!chk->initialized) IntWindowChecker_Init(chk);
        return false;
    }

    // 2. 将当前合法值存入循环队列
    chk->buffer[chk->head] = (int8_t)val;

    // 3. 维护计数器（最多到 10）
    if (chk->count < WINDOW_SIZE) {
        chk->count++;
    }

    // 4. 移动循环指针
    chk->head++;
    if (chk->head >= WINDOW_SIZE) {
        chk->head = 0;
    }

    // 5. 判断条件：必须已经攒够了 10 次
    // 因为前面已经把非 0,1,-1 的值过滤并重置了，所以能走到这里说明这 10 次全部都是 0, 1 或 -1
    if (chk->count == WINDOW_SIZE) {
        return true;
    }

    return false;
}



void pid_init(void){
    Target_I_Ref = 0;
    V_Ek = 0;
    V_Ek_1 = 0;
    V_Ek_2 = 0;
    I_Ek = 0;
    I_Ek_1 = 0;
    I_Ek_2 = 0;
    current_flag = 0;
    pid_initialized = 1;

}

/**
 * @brief 初始化 ADC 及 DMA
 */
void myADC_Init(void) {
    ADC_GPIO_Config();
    ADC_DMA_Config();
    ADC_Peripheral_Config();
        
    /* ADC 校准 */
    ADC_Calibration();
    
    /* 清除缓冲区 */
    for(uint8_t i = 0; i < ADC_DMA_BUFFER_SIZE; i++) {
        g_adc_dma_buffer[i] = 0;
        g_adc_data.raw[i] = 0;
    }
    g_adc_data.ready = 0;
}

/**
 * @brief 配置 ADC GPIO
 */
static void ADC_GPIO_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    
    /* 开启 GPIOA 时钟 */
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA, ENABLE);
    
    /* 开启 ADC 时钟 */
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_ADC1, ENABLE);
    
    /* 配置所有 ADC 引脚为模拟输入 */
    GPIO_InitStructure.GPIO_Pin = ADC_VOUT_PIN | 
                                 ADC_VIN_PIN | 
                                  ADC_V12_PIN | 
                                  ADC_NTC_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;  // 模拟输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
 * @brief 配置 DMA
 */
static void ADC_DMA_Config(void) {
    DMA_InitTypeDef DMA_InitStructure = {0};
    
    /* 开启 DMA1 时钟 */
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    
    /* 复位 DMA 通道 */
    DMA_DeInit(ADC_DMA_CHANNEL);
    
    /* DMA 配置 */
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->RDATAR;  // ADC 数据寄存器
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)g_adc_dma_buffer;   // 内存缓冲区
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;                   // 外设到内存
    DMA_InitStructure.DMA_BufferSize = ADC_DMA_BUFFER_SIZE;              // 缓冲区大小
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;     // 外设地址不变
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;              // 内存地址递增
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;  // 16 位
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;          // 16 位
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;                      // 循环模式
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;                  // 高优先级
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;                         // 非内存到内存
    DMA_Init(ADC_DMA_CHANNEL, &DMA_InitStructure);

    NVIC_SetPriority(DMA1_Channel1_IRQn, ADC_DMA_NVIC_PREEMPT);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    
    /* 使能 DMA 传输完成中断 */
    DMA_ITConfig(ADC_DMA_CHANNEL, DMA_IT_TC, ENABLE);
    
    /* 使能 DMA 通道 */
    DMA_Cmd(ADC_DMA_CHANNEL, ENABLE);
}

/**
 * @brief 配置 ADC 外设
 */
static void ADC_Peripheral_Config(void) {
    ADC_InitTypeDef ADC_InitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_HB_Div4);
    
    ADC_DeInit(ADC1);
    /* ADC 配置 */
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;              // 扫描模式
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;       // 非连续转换 (由 TIM1 触发)
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; // TIM3中断里软件触发
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;    // 右对齐
    ADC_InitStructure.ADC_NbrOfChannel = ADC_REGULAR_CHANNEL_COUNT;   // 通道数量
    ADC_Init(ADC1, &ADC_InitStructure);
    
    /* 配置各通道采样时间 */
    /* 通道顺序需与 DMA 缓冲区顺序一致 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_19, 1, ADC_SAMPLE_TIME);  // 输入电压
    ADC_RegularChannelConfig(ADC1, ADC_Channel_18, 2, ADC_SAMPLE_TIME);  // 12V 供电
    ADC_RegularChannelConfig(ADC1, ADC_Channel_8,  3, ADC_SAMPLE_TIME);  // NTC 温度
    
    /*关闭OPA3与ADC channel 9之间的开关*/
    //OPA->ISP_CTLR &= ~(uint32_t)(1<<OPA_ISP1_SEL_IO_BIT);

    
    //设置注入通道的触发源为 TIM1 的 TRGO 信号（或 CC4 等具体通道）
    //ADC_ExternalTrigInjectedConvConfig(ADC1, ADC_INJECTED_TRIGGER_SRC);

    ADC_InjectedSequencerLengthConfig(ADC1, 2);
#ifdef  REVISED_HARDWARE
    ADC_InjectedChannelConfig(ADC1,ADC_Channel_2,1,ADC_SAMPLE_TIME_FAST); // 输出电压  
#else
    ADC_InjectedChannelConfig(ADC1,ADC_Channel_10,1,ADC_SAMPLE_TIME_FAST); // 输出电压  
#endif
    ADC_InjectedChannelConfig(ADC1,ADC_Channel_9,2,ADC_CURRENT_SAMPLE_TIME); // 输出电流
 
    //ADC_AutoInjectedConvCmd(ADC1, ENABLE); 

    // 开启注入通道的外部触发（这一步至关重要）
    //ADC_ExternalTrigInjectedConvCmd(ADC1, ENABLE);
    
    
    /* 使能 ADC DMA，用于规则组*/
    ADC_DMACmd(ADC1, ENABLE);

    /*打开PA14与ADC channel 9之间的开关*/
    /*通道9连接了PA14,检测输出电压*/
    //OPA->ISP_CTLR |= (uint32_t)(1<<OPA_ISP1_QDET1_BIT);
    //current_flag = 0;
    
    /*使能ADC中断，用于注入组*/
    //ADC_NVIC_Config();

    /* 使能 ADC */
    ADC_Cmd(ADC1, ENABLE);

    ADC_BufferCmd(ADC1, DISABLE); //disable buffer
    

}

/**
 * @brief 配置 ADC 中断优先级
 */
static void ADC_NVIC_Config(void) {

    ADC_ClearITPendingBit(ADC1,ADC_IT_JEOC);

    NVIC_SetPriority(ADC_IRQn, ADC_NVIC_PREEMPT);
    NVIC_EnableIRQ(ADC_IRQn);

    ADC_ITConfig(ADC1, ADC_IT_JEOC, ENABLE);
}

/*********************************************************************
 * @fn      TIM1_UP_IRQHandler
 *
 * @brief   This function handles TIM3 exception.
 *
 *
 * @return  none
 */
void TIM1_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM1_UP_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM1, TIM_IT_Update)==SET)
    {
        // 在文件开头定义
        static int32_t duty_remainder = 0; // 存储不足 1/1024 的微小增量
        static int32_t target_i_reminder = 0;
        static uint8_t voltage_div = 10; // 电压环降频计数器
        typedef enum {
            MODE_VOLTAGE,
            MODE_CURRENT
        } ControlMode_t;

        static ControlMode_t current_mode = MODE_VOLTAGE;
        int32_t raw_delta = 0;
        //int32_t I_Ek;
        static int32_t Target_I_local = 0;
        int32_t Target_I_ref_local = 0;
        int32_t V_out_target_raw=0;
        static int32_t last_ff_val = 0;
        static uint8_t ff_first_run = 1;




        ADC_SoftwareStartInjectedConvCmd(ADC1,ENABLE);
        while(!ADC_GetFlagStatus( ADC1, ADC_FLAG_JEOC ));
        int32_t voltage_buffer_local=ADC_GetInjectedConversionValue(ADC1,ADC_InjectedChannel_1);
        current_buffer=ADC_GetInjectedConversionValue(ADC1,ADC_InjectedChannel_2);

        //手动清除标志位，为下一次转换做准备
        ADC_ClearFlag(ADC1, ADC_FLAG_JEOC);

        // 电压一阶滤波 (Alpha = 1/8),因为飞线和分压限制读数抖动厉害，所以用滤波平滑
        // filtered_adc = last_adc * 7/8 + current_adc * 1/8
        static int32_t last_adc_v = 0;
        filtered_adc_v = (last_adc_v * 7 + voltage_buffer_local) >> 3;
        last_adc_v = filtered_adc_v;

        //电流一阶滤波，采样时间比较短所以滤波，增加稳定性
        //filtered_adc = last_adc*7/8 + current_adc*1/8
        static int32_t last_adc_i = 0;
        filtered_adc_i = (last_adc_i*7 + current_buffer) >> 3;
        last_adc_i = filtered_adc_i;
        
        


        if(output_enabled){

            //output_enabled = 0期间初始化pid
            pid_initialized = 0;
            Dead_Zone_Current_Flag = 0;
            


           


            // 1. 电压 PID 计算 (增量式)
            // 2. 电压环：每 10 次中断运行一次 (1kHz)
            if (++voltage_div >= 10) {
                voltage_div = 0;
                Dead_Zone_Flag = 0;
                

                
                // 计算电压误差
#ifdef REVISED_HARDWARE
                voltage_buffer = voltage_buffer_local;
#else
                voltage_buffer = filtered_adc_v;
#endif     
                V_Ek =  Target_V - voltage_buffer; 
                int32_t V_Ek_abs = abs(V_Ek);

 #ifdef DEAD_ZONE
                if(V_Ek_abs<DEAD_ZONE_V_LIMIT*RATE_V_ADC){
                    Dead_Zone_Flag = 1;
                    V_Ek = 0;
                }else if(V_Ek_abs >(DEAD_ZONE_V_LIMIT+DEAD_ZONE_V_HYS)*RATE_V_ADC){
                    Dead_Zone_Flag = 0;
                }
#endif    
                
                // 电压环 PID 计算出的是“目标电流”的增量
                int32_t V_delta = (V_Kp * (V_Ek - V_Ek_1) + V_Ki * V_Ek + V_Kd * (V_Ek - 2*V_Ek_1 + V_Ek_2));
                
                // 更新目标电流基准
                target_i_reminder += V_delta;
                int32_t target_i_delta = target_i_reminder /PID_SCALE;
                Target_I_Ref += target_i_delta; // 使用类似余数池逻辑平滑
                target_i_reminder = target_i_reminder % PID_SCALE;
                
                // 电流基准限幅 (防过流)
                // 5A对应adc读数1241， 100mA对应25
                //if(Target_I_Ref > MAX_I_LIMIT) Target_I_Ref = MAX_I_LIMIT;
                //if(Target_I_Ref < MIN_I_LIMIT) Target_I_Ref = MIN_I_LIMIT;

                //转换成ADC读数以便与Target_I比较
                Target_I_ref_local = Convert_To_ADC_I(Target_I_Ref);

                //ADC读数越大电流越小
                //所以选ADC读数目标大的环路
                if(Target_I > Target_I_ref_local) { 

                     //电流环工作
                    Target_I_local = Target_I;

                    //电压环的目标电流不再更新
                    Target_I_Ref -= target_i_delta;
                    target_i_reminder = 0;

                    //首次切换复位duty余数
                    //前馈也重新开始
                    if(current_mode == MODE_VOLTAGE){
                        duty_remainder = 0;
                        ff_first_run = 1;
                    }
                    current_mode = MODE_CURRENT;
                    // 电压环抗饱和：这是解决“不恢复”的关键
                    // 强制同步误差，让电压环在下一帧计算增量时，认为误差没有变化
                    V_Ek_2 = V_Ek; V_Ek_1 = V_Ek;
                }
                else {
                    Target_I_local = Target_I_ref_local;
                    if(current_mode == MODE_CURRENT){
                        duty_remainder = 0;
                        ff_first_run = 1;
                    }
                    current_mode = MODE_VOLTAGE;
                    // 更新电压环状态
#ifdef DEAD_ZONE 
                    if(Dead_Zone_Flag != 1){
                        V_Ek_2 = V_Ek_1; V_Ek_1 = V_Ek;
                    }
                    else{
                        V_Ek_2 = V_Ek; V_Ek_1 = V_Ek;
                    }
#else
                    V_Ek_2 = V_Ek_1; V_Ek_1 = V_Ek;
#endif
                }
                
             
            }
            

            // 3. 电流环：每次中断都运行 (10kHz)
            // 现在的目标是追赶 Target_I_Ref
            I_Ek = filtered_adc_i - Target_I_local; // 调整误差符号以匹配增量逻辑

            int32_t I_delta = (I_Kp * (I_Ek - I_Ek_1) + I_Ki * I_Ek + I_Kd * (I_Ek - 2*I_Ek_1 + I_Ek_2));
            
            if(current_mode == MODE_CURRENT){
                I_Ek_2 = I_Ek_1; I_Ek_1 = I_Ek;
                V_out_target_raw = filtered_adc_v;
            } else {
                I_Ek_2 = I_Ek; I_Ek_1 = I_Ek;
                V_out_target_raw = Target_V;
            }

            //用于解决空载时电压环无法降低电流环问题
            //因为目标电流是0时最小，没有负电流
            int32_t voltage_compensation = 0;
            int32_t V_Error = filtered_adc_v - Target_V;
            if ((V_Error > RATE_V_ADC*OVERLOAD_V_LIMIT) && (filtered_adc_i > OFFSET_1V_6_ADC - LIGHTLOAD_CURRENT*RATE_I_ADC)){
                voltage_compensation = -V_Error * K_COMP * PID_SCALE;
            }


#ifdef FEED_FORWARD

            //这里需要确定输入采样是否有效？或者滞后？或者会不会同时访问？
            int32_t V_in_raw = g_adc_data.raw[ADC_CH_VIN];
            
            //ADC读数83对应1V，为了防止除0问题设置一个最小值
            if (V_in_raw < 100) V_in_raw = 100; 

            // 2. 计算当前原始前馈对应的 PWM 比较值 (放大 1024 倍以防整型丢失精度)
            // 核心公式: (V_out * PWM_PERIOD * 1024) / V_in
            int32_t raw_ff_pwm = (int32_t)((V_out_target_raw * TIMER1_PERIOD * 1024) / V_in_raw);

            // 3. 对前馈进行整型一阶低通滤波 (替代浮点 alpha 滤波)
            // smoothed = (alpha * raw) + ((1 - alpha) * last_smoothed)
            // 这里 alpha 取 1/32，即 (raw - last) >> 5
            // 2. 首次运行特殊处理：直接初始化滤波器的基准，避免第一拍阶跃
            if (ff_first_run) {
                smoothed_ff_pwm = raw_ff_pwm;     // 滤波器直接填满当前值
                last_ff_val = raw_ff_pwm >> 10;   // 历史前馈值设为当前值
                ff_first_run = 0;                 // 清除标志
            } else {
            // 正常一阶低通滤波
                smoothed_ff_pwm += (raw_ff_pwm - smoothed_ff_pwm) >> 5;
            }


            smoothed_ff_pwm += (raw_ff_pwm - smoothed_ff_pwm) >> 5;

            // 4. 计算当前平滑前馈的实际整数值（还原 1024 倍）
            int32_t current_ff_val = smoothed_ff_pwm >> 10;

            // 5. 计算上一次的前馈值（需要保存历史记录）
            int32_t delta_ff = current_ff_val - last_ff_val;
            last_ff_val = current_ff_val;

            duty_remainder +=delta_ff;


#endif            


            // 4. PWM 执行 (电流环直接控制)
            duty_remainder += I_delta;
            duty_remainder += voltage_compensation;
            int32_t delta_duty = duty_remainder / 1024;
            duty_remainder %= 1024; 

           
            //4. 对每次改变量限制幅度
            //duty 240时为5，-5，360时改为3，-3
            if(delta_duty >= 3) {
                delta_duty=3;
                duty_remainder = duty_remainder+ delta_duty-3;
            }else if (delta_duty <= -3) {
                delta_duty = -3;
                duty_remainder = duty_remainder + delta_duty + 3;
            
            }
#ifdef STABLE_CHECK
            bool Stable_state = IntWindowChecker_Update(&stable_checker, delta_duty);
#endif

#ifdef DEAD_ZONE
#ifdef STABLE_CHECK
            if((current_mode == MODE_VOLTAGE && Dead_Zone_Flag == 1) || (current_mode == MODE_CURRENT && Stable_state)){
#else
            if(current_mode == MODE_VOLTAGE && Dead_Zone_Flag == 1){
#endif
                I_Ek_2 = I_Ek;
                duty_remainder = 0;

            }else{
                // 5. 获取当前 PWM 值
                int32_t current_pwm = (int32_t)TIM_GetCapture1(TIM1);
                 
              
                // 6. 计算需要调整的PWM值
                int32_t next_pwm = current_pwm + delta_duty;
            

                // 7. 限幅保护 (150kHz 对应 ARR=720，center_aligned减半360)
                //100Khz对应360
                if(next_pwm > TIMER1_PERIOD*9/10) {
                    next_pwm = TIMER1_PERIOD*9/10;
                }else if(next_pwm < TIMER1_PERIOD/10) {
                    next_pwm = TIMER1_PERIOD/10;
                }

                // 8. 更新硬件
                TIM_SetCompare1(TIM1, (uint16_t)next_pwm);
            }
#else
                // 5. 获取当前 PWM 值
                int32_t current_pwm = (int32_t)TIM_GetCapture1(TIM1);
                 
              
                // 6. 计算需要调整的PWM值
                int32_t next_pwm = current_pwm + delta_duty;
            

                // 7. 限幅保护 (150kHz 对应 ARR=720，center_aligned减半360)
                //100Khz对应360
                if(next_pwm > TIMER1_PERIOD*9/10) {
                     next_pwm = TIMER1_PERIOD*9/10;
                }else if(next_pwm < TIMER1_PERIOD/10) {
                    next_pwm = TIMER1_PERIOD/10;
                }

                // 8. 更新硬件
                TIM_SetCompare1(TIM1, (uint16_t)next_pwm);
            
    
#endif
            // 9. 状态滚动与抗饱和处理
            // 如果当前是电流环起作用，电压环的误差不应继续滚动累加（防止积分饱和）
            //V_Ek_2 = V_Ek_1; V_Ek_1 = V_Ek;
            //I_Ek_2 = I_Ek_1; I_Ek_1 = I_Ek;
        }else{
            if(pid_initialized == 0){
                //状态保留变量清零
                duty_remainder = 0;
                target_i_reminder = 0;

                //前馈初始化
                smoothed_ff_pwm = 0;
                last_ff_val = 0;
                ff_first_run = 1;
                
                //输出关闭期间设置占空比0
                //测试期间暂不执行此语句
                //此操作似乎没有必要出现在中断里，可以在主循环做
                //TIM_SetCompare1(TIM1, 0);
                pid_init();
                IntWindowChecker_Init(&stable_checker);
                //WindowChecker_Init(&pid_checker);
            }             
    
        }

        
     
        
        /*if(led_updated){
            led_color++;
            if(led_color> 3){
                led_color = 1;
            }
            led_updated = 0;
        }*/

        TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
    }
    
}


/**
 * @brief DMA1 通道 1 中断服务程序
 * @note  在 TIM1 更新中断后触发，处理采样数据
 */
void DMA1_Channel1_IRQHandler(void)  __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel1_IRQHandler(void) {
    if(DMA_GetITStatus(DMA1_IT_TC1) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TC1);
        
        /* 复制 DMA 缓冲区到数据处理区 */
        for(uint8_t i = 0; i < ADC_DMA_BUFFER_SIZE; i++) {
            g_adc_data.raw[i] = g_adc_dma_buffer[i];
        }
        
        /* 数据处理 (电压/电流转换) */
        ADC_ProcessData();
        
        /* 设置就绪标志 */
        g_adc_data.ready++;
    }
}

/* ==================== ADC函数 ==================== */


/**
 * @brief ADC 校准
 */
void ADC_Calibration(void) {

    /* 复位 ADC 校准寄存器 */
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));
}

/**
 * @brief 启动 ADC 连续采样
 */
void ADC_StartConversion(void) {
    /* 使能外部触发 */
    //ADC_ExternalTrigConvCmd(ADC1, ENABLE);
    
    /* 软件启动一次转换 (后续由 TIM1 触发) */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/**
 * @brief 停止 ADC 采样
 */
void ADC_StopConversion(void) {
    //ADC_ExternalTrigConvCmd(ADC1, DISABLE);
    ADC_Cmd(ADC1, DISABLE);
}


/* ==================== 数据处理函数 ==================== */

/**
 * @brief 定点数修正电压
 * @param raw_v_mv 原始计算出的电压值，单位 mV (例如 12400)
 * @return 修正后的电压值，单位 mV
 */
int32_t get_calibrated_voltage_int(int32_t raw_v_mv) {
    // 放大 1024 倍进行计算
    // 公式: (raw_v_mv * 1069 - 962 * 1024) / 1024
    // 简化: (raw_v_mv * 1069) >> 10 - 962
    
    int32_t corrected_v = (raw_v_mv * 1069) >> 10;
    //原计算所得结果偏小80mv，尝试从962改成882
    corrected_v -= 882;

    // 边界处理：防止 0V 附近出现负数
    if (corrected_v < 0) {
        corrected_v = 0;
    }

    return corrected_v;
}

/**
 * @brief 将设定电压转换为预期的 ADC 读数电压
 * @param set_v_mv 用户设定的目标电压，单位 mV (例如想输出 5V，传入 5000)
 * @return 预期的 ADC 读数电压，单位 mV (对应你算法中会读到的 0.9V~12.4V 区间值)
 */
int32_t convert_set_to_adc_read(int32_t set_v_mv) {
    // 公式: V_read = (set_v_mv * 981) >> 10 + 900
    
    // 1. 计算比例部分
    int32_t expected_read = (set_v_mv * 981) >> 10;
    
    // 2. 加上 0V 时的基础偏移 (0.9V)
    expected_read += 845;
    // 边界处理：防止 超出30V?
    /*if (expected_read > 30000) {
        expected_read = 30000;
    }*/
    
    return expected_read;
}

/**
 * @brief 准备 ADC 采样数据 ，原dma中断处理程序内容
 */
void ADC_PrepareData(void) {
       
    /* 复制 DMA 缓冲区到数据处理区 */
    for(uint8_t i = 0; i < ADC_DMA_BUFFER_SIZE; i++) {
        g_adc_data.raw[i] = g_adc_dma_buffer[i];
    }
        
    /* 数据处理 (电压/电流转换) */
    ADC_ProcessData();
        
    /* 设置就绪标志 */
    g_adc_data.ready++;
}


/**
 * @brief 处理 ADC 采样数据 (整型运算)
 */
static void ADC_ProcessData(void) {
    uint32_t temp_val;
    
    /* 通道 0: 电流 (特殊处理) */
    /* 25mΩ * 16 倍放大 = 0.4V/A = 400mV/A */
    /* I(mA) = ADC * 3300 * 1000 / 4096 / 400 */
    //temp_val = (g_adc_data.raw[ADC_CH_CURRENT] * 3300 * 1000);
    //g_adc_data.current_ma[ADC_CH_CURRENT] = temp_val / (4096 * 400);
    
    /* 通道 1: 输出电压 (30K + 2.2K 分压) */
    /* Vout = ADC * 3300 * (30+2.2) / 4096 / 2.2 */
    //temp_val = (g_adc_data.raw[ADC_CH_VOUT] * 3300 * 322);
    //g_adc_data.voltage_mv[ADC_CH_VOUT] = temp_val / (4096 * 22);
    
    /* 通道 0: 输入电压 (需根据实际分压调整) */
    /* 假设 33K + 2.2K 分压 */
    temp_val = (g_adc_data.raw[ADC_CH_VIN] * 3300 * 352);
    g_adc_data.voltage_mv[ADC_CH_VIN] = temp_val / (4096 * 22);
    
    /* 通道 1: 12V 供电 (18K + 4.7K 分压) */
    temp_val = (g_adc_data.raw[ADC_CH_V12] * 3300 * 227);
    g_adc_data.voltage_mv[ADC_CH_V12] = temp_val / (4096 * 47);
    
    /* 通道 2: NTC 温度 */
    g_adc_data.voltage_mv[ADC_CH_NTC] = (g_adc_data.raw[ADC_CH_NTC] * 3300) / 4096;
}

/**
 * @brief NTC 温度计算 (3950 10K NTC)
 * @param adc_val ADC 原始值
 * @return 温度值 (℃ * 10)
 */
static int32_t NTC_CalculateTemperature(uint16_t adc_val) {
    /* NTC 分压电路：3.3V - 10K - NTC - GND */
    /* Vout = 3.3V * Rntc / (10K + Rntc) */
    /* Rntc = 10K * Vout / (3.3V - Vout) */
    /* Rntc = 10K * ADC / (4096 - ADC) */
    
    if(adc_val >= 4090 || adc_val <= 6) {
        return -9999;  // 超出范围
    }
    
    /* 计算 NTC 电阻值 (整型，单位：欧姆) */
    uint32_t rntc = (10000 * adc_val) / (4096 - adc_val);
    
    /* Steinhart-Hart 方程简化版 (整型近似) */
    /* 1/T = 1/T0 + 1/B * ln(R/R0) */
    /* T0 = 298.15K (25℃), R0 = 10K, B = 3950 */
    
    /* 使用查找表或近似公式 */
    /* 简化：T(℃) ≈ 1/(1/298.15 + 1/3950 * ln(R/10000)) - 273.15 */
    
    /* 整型近似计算 (放大 1000 倍) */
    int32_t temp_k_x1000;
    
    /* ln(R/R0) 近似计算 */
    /* 使用泰勒展开或查找表 */
    if(rntc >= 10000) {
        /* R > R0, 温度低于 25℃ */
        int32_t ratio = (rntc - 10000) * 1000 / 10000;
        //temp_k_x1000 = 298150 - (ratio * 298150 * 298150) / (3950 * 1000000);
        temp_k_x1000 = 298150 - (ratio * 45) / 2;
    } else {
        /* R < R0, 温度高于 25℃ */
        int32_t ratio = (10000 - rntc) * 1000 / 10000;
        //temp_k_x1000 = 298150 + (ratio * 298150 * 298150) / (3950 * 1000000);
        temp_k_x1000 = 298150 + (ratio * 45) / 2;
    }
    
    /* 转换为摄氏度 (×10) */
    int32_t temp_c_x10 = (temp_k_x1000 - 273150) / 100;
    
    return temp_c_x10;
}

#ifdef REVISED_HARDWARE

static int32_t Convert_From_ADC_V(int32_t v_adc){

    // 1. 先计算出 ADC 引脚的真实电压（mv）
    // 使用 int64_t 避免中间乘法 3047 * v 溢出
    int64_t v_pin_mv = ((int64_t)v_adc * 3300) / 4096;

    // 2. 根据分压电阻计算原始电压
    // 你的原公式是：(v_pin) * (32200 /2200) 
    // 合并简化： v_pin * 322 / 22
    int32_t result = (int32_t)(v_pin_mv * 322 / 22);

    return result;
} 

static int32_t Convert_To_ADC_V(int32_t v_mv){
    // 目标：计算 ADC 硬件引脚的期望电压 v_pin (mV)
    // 公式推导：v_pin = (v_mv * 22 / 322) 
       
    // 1. 计算引脚电压 (mV)
    int32_t v_pin_mv = (v_mv * 22 / 322);
    
    // 2. 将电压转换为 ADC 原始读数 (0-4095)
    // 原始公式：v_pin_mv / 3300 * 4096
    // 优化：使用 int64_t 避免 (v_pin_mv * 4096) 溢出（如果是 3.3V 时 v_pin_mv 最大 3300，不会溢出，但安全起见）
    int32_t adc_val = ((int64_t)v_pin_mv * 4096) / 3300;

    // 3. 限幅保护
    if (adc_val > 4095) adc_val = 4095;
    if (adc_val < 0) adc_val = 0;

    return adc_val;
}

static int32_t Convert_From_ADC_I(int32_t i_adc) {
    // 1. 计算差值（考虑实际 offset 可能抖动，允许小范围负数处理）
    // 假设 OFFSET_1V_6_ADC 是通过上电校准得到的
    int32_t i_adc_diff = (int32_t)OFFSET_1V_6_ADC - i_adc;
    
    // 如果差值为负，说明电流方向反了或只是轻微噪声，归零
    if(i_adc_diff < 0) return 0; 

    // 2. 将 ADC 差值直接转换为 mA
    // 公式简化：mA = diff * (3300 / 4096) / (8 * 0.025)
    // mA = diff * 0.8056 / 0.2 = diff * 4.028
    // 为了不使用浮点数，使用 * 2929 / 1000
    int32_t i_ma = (int32_t)((int64_t)i_adc_diff * 4028 / 1000);

    return i_ma;
}

static int32_t Convert_To_ADC_I(int32_t i_ma) {
    // 1. 限流保护 5A (5000mA)
    if(i_ma > 5000) i_ma = 5000;
    if(i_ma < 0) i_ma = 0;

    // 2. 直接计算电流对应的 ADC 变化量 (delta_adc)
    // delta_adc = i_ma * (增益8 * 电阻0.025) * (4096 / 3300)
    // delta_adc = i_ma * 0.2 * 1.2412 = i_ma * 0.2482
    // 使用 * 248 / 1000 保持精度
    int32_t delta_adc = i_ma * 248 / 1000;

    // 3. 从偏置中减去改变量
    int32_t adc_val = (int32_t)OFFSET_1V_6_ADC - delta_adc;

    // 4. 限幅
    //if (adc_val > 4095) adc_val = 4095;
    //if (adc_val < 0) adc_val = 0;

    return adc_val;
}

#else

static int32_t Convert_From_ADC_V(int32_t v_adc){

    // 1. 先计算出 ADC 引脚的真实电压（mv）
    // 使用 int64_t 避免中间乘法 3047 * v 溢出
    int64_t v_pin_mv = ((int64_t)v_adc * 3300) / 4096;

    // 2. 减去 0.55V 偏置
    v_pin_mv -= 550;
    
    // 保护：如果电压低于偏置，直接归零，防止负数干扰后续逻辑
    if (v_pin_mv < 0) v_pin_mv = 0;

    // 3. 根据分压电阻计算原始电压
    // 你的原公式是：(v_pin / 4) * (30470 / 470) 
    // 合并简化：v_pin * 3047 / (47 * 4) = v_pin * 3047 / 188
    int32_t result = (int32_t)(v_pin_mv * 3047 / 188);

    return result;
} 

static int32_t Convert_To_ADC_V(int32_t v_mv){
    // 目标：计算 ADC 硬件引脚的期望电压 v_pin (mV)
    // 公式推导：v_pin = (v_mv * 47 / 3047) * 4 + 550
    // 简化为：v_pin = (v_mv * 188 / 3047) + 550
    
    // 1. 计算引脚电压 (mV)
    int32_t v_pin_mv = (v_mv * 188 / 3047) + 550;
    
    // 2. 将电压转换为 ADC 原始读数 (0-4095)
    // 原始公式：v_pin_mv / 3300 * 4096
    // 优化：使用 int64_t 避免 (v_pin_mv * 4096) 溢出（如果是 3.3V 时 v_pin_mv 最大 3300，不会溢出，但安全起见）
    int32_t adc_val = (v_pin_mv * 4096) / 3300;

    // 3. 限幅保护
    if (adc_val > 4095) adc_val = 4095;
    if (adc_val < 0) adc_val = 0;

    return adc_val;
}

static int32_t Convert_From_ADC_I(int32_t i_adc) {
    // 1. 计算差值（考虑实际 offset 可能抖动，允许小范围负数处理）
    // 假设 OFFSET_1V_6_ADC 是通过上电校准得到的
    int32_t i_adc_diff = (int32_t)OFFSET_1V_6_ADC - i_adc;
    
    // 如果差值为负，说明电流方向反了或只是轻微噪声，归零
    if(i_adc_diff < 0) return 0; 

    // 2. 将 ADC 差值直接转换为 mA
    // 公式简化：mA = diff * (3300 / 4096) / (55 * 0.005)
    // mA = diff * 0.8056 / 0.275 = diff * 2.929
    // 为了不使用浮点数，使用 * 2929 / 1000
    int32_t i_ma = (int32_t)((int64_t)i_adc_diff * 2929 / 1000);

    return i_ma;
}

static int32_t Convert_To_ADC_I(int32_t i_ma) {
    // 1. 限流保护 4A (4000mA)
    if(i_ma > 4000) i_ma = 4000;
    if(i_ma < 0) i_ma = 0;

    // 2. 直接计算电流对应的 ADC 变化量 (delta_adc)
    // delta_adc = i_ma * (增益55 * 电阻0.005) * (4096 / 3300)
    // delta_adc = i_ma * 0.275 * 1.2412 = i_ma * 0.3413
    // 使用 * 341 / 1000 保持精度
    int32_t delta_adc = (int32_t)((int64_t)i_ma * 341 / 1000);

    // 3. 从偏置中减去改变量
    int32_t adc_val = (int32_t)OFFSET_1V_6_ADC - delta_adc;

    // 4. 限幅
    if (adc_val > 4095) adc_val = 4095;
    if (adc_val < 0) adc_val = 0;

    return adc_val;
}

#endif

/* ==================== 接口函数 ==================== */

/**
 * @brief 获取指定通道的原始 ADC 值
 */
uint16_t ADC_GetRawValue(uint8_t channel) {
    if(channel >= ADC_CHANNEL_COUNT) return 0;
    return g_adc_data.raw[channel];
}

/**
 * @brief 获取指定通道的电压值 (毫伏)
 */
uint32_t ADC_GetVoltage_mV(uint8_t channel) {
    if(channel >= ADC_CHANNEL_COUNT) return 0;
    return g_adc_data.voltage_mv[channel];
}

/**
 * @brief 获取电流值 (毫安)
 */
uint32_t ADC_GetCurrent_mA(void) {
    return Convert_From_ADC_I(filtered_adc_i);
}

/**
 * @brief 获取目标电流值 (毫安)
 */
uint32_t ADC_GetTargetCurrent_mA(void) {
    return Convert_From_ADC_I(Target_I);
}

/**
 * @brief 设置电流值 (毫安)
 */
void ADC_SetCurrent_mA(int32_t i) {
    Target_I = Convert_To_ADC_I(i);
}


/**
 * @brief 获取输出电压值 (毫伏)
 */
uint32_t ADC_GetOutputVoltage_mV(void) {
    //根据ADC读数换算成电压，单位mv
    int32_t v_mv = Convert_From_ADC_V(filtered_adc_v);
    //返回修正后的电压，根据测量结果修正
#ifdef REVISED_HARDWARE
    return v_mv;
#else
    return get_calibrated_voltage_int(v_mv);
#endif
}

/**
 * @brief 设置输出电压值 (毫伏)
 */

void ADC_SetOutputVoltage_mv(int32_t v){

#ifdef REVISED_HARDWARE
    int32_t set_v_mv = v;
#else   
    //将目标值先反向修正得到ADC读数对应的电压
    int32_t set_v_mv = convert_set_to_adc_read(v);
#endif
    //将ADC电压转换成ADC的读数以便PID算法使用
    Target_V = Convert_To_ADC_V(set_v_mv);
}

/**
 * @brief 获取目标输出电压值 (毫伏)
 */

uint32_t ADC_GetTargetOutputVoltage_mv(void){

    // 1. 先从 ADC 原始数值转回修正后的电压
    int32_t adc_v_mv = Convert_From_ADC_V(Target_V);
    // 2. 再通过逆函数转回用户原始设定的电压
#ifdef REVISED_HARDWARE
    return adc_v_mv;
#else   
    return get_calibrated_voltage_int(adc_v_mv);
#endif
}



/**
 * @brief 获取输入电压值 (毫伏)
 */
uint32_t ADC_GetInputVoltage_mV(void) {
    return g_adc_data.voltage_mv[ADC_CH_VIN];
}

/**
 * @brief 获取 12V 供电电压值 (毫伏)
 */
uint32_t ADC_Get12VVoltage_mV(void) {
    return g_adc_data.voltage_mv[ADC_CH_V12];
}

/**
 * @brief 获取 NTC 温度值 (摄氏度 x10)
 */
int32_t ADC_GetTemperature_x10(void) {
    return NTC_CalculateTemperature(g_adc_data.raw[ADC_CH_NTC]);
}

/**
 * @brief 获取数据就绪标志
 */
uint16_t ADC_IsDataReady(void) {
    return g_adc_data.ready;
}

/**
 * @brief 清除数据就绪标志
 */
void ADC_ClearDataReady(void) {
    g_adc_data.ready = 0;
}

int32_t ADC_GetVEk(void){
    return Target_I_Ref;
}