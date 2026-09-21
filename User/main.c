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
 *GPIO routine:
 *PB0 push-pull output.
 *
 */

#include "debug.h"
#include "ws2812.h"
#include "fan.h"
#include "oled.h"
#include "encoder.h"
#include "drv_adc.h"
#include "OPA.h"
#include "button.h"
#include "usb_cdc.h"
#include "drv_config.h"
#include  "PD_Process.h"
#include "drv_menu.h"

/* Global define */
#define SOFT_START_PWM  3*TIMER1_PERIOD/10
//输出电压测量值似乎偏小一点，加50mv再显示到OLED上
//只是修正显示，设置值不参考偏移
#define SHOW_VOLTAGE_OFFSET 50


/* Global Variable */

volatile uint32_t systemtick = 0;
volatile uint8_t tim1_break = 0;

volatile uint32_t  Tim_Ms_Cnt = 0;

void TIM1_BRK_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
/*********************************************************************
 * @fn      TIM1_BRK_IRQHandler
 *
 * @brief   Brake interrupt
 *
 * @return  none
 */
void TIM1_BRK_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM1, TIM_IT_Break)==SET)
    {
        output_enabled = 0;
        GPIO_WriteBit(GPIOB, GPIO_Pin_4, Bit_RESET);
        GPIO_ResetBits(GPIOC, GPIO_Pin_2); 
        tim1_break = 1;
        TIM_ClearITPendingBit(TIM1, TIM_IT_Break);
    }
}



/*********************************************************************
 * @fn      TIM3_UP_IRQHandler
 *
 * @brief   This function handles TIM3 exception.
 *
 *
 * @return  none
 */
void TIM3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM3_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM3, TIM_IT_Update)==SET)
    {
        static uint16_t count = 100,ms_count=10;
        
        //0.1ms system ticks
        systemtick++;

        User_USB_CDC_CTRL.USB_Up_TimeOut++;     
        
        //USB PD timer, 1ms ticks
        ms_count--;

        if(ms_count == 0) { 
            Tim_Ms_Cnt++;
            ms_count = 10;
        }

        //ADC regular group conversion count, 10ms
        count--;

        if(count == 0) { 
            count = 100;
            //ADC1->CTLR2 |= ADC_SWSTART;  //直接操作寄存器，比调用库函数更快
            ADC_SoftwareStartConvCmd(ADC1, ENABLE); // 开始规则组转换
        }

    }
    TIM_ClearITPendingBit( TIM3, TIM_IT_Update );
}


void Recovery_From_Brake(void){
    // 2. 重新使能主输出 (MOE)
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
}



/*********************************************************************
 * @fn      GPIO_Toggle_INIT
 *
 * @brief   Initializes PB12 GREEN LED
 *
 * @return  none
 */
void GPIO_Toggle_INIT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOB, GPIO_Pin_12); // 默认禁用
}

/*********************************************************************
 * @fn      Button_Init
 *
 * @brief   初始化按钮 (PC4)
 *
 * @return  none
 */
void Button_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 上拉输入
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

/*********************************************************************
 * @fn      InputEnable_Init
 *
 * @brief   初始化前级输入使能 (PC2)
 *
 * @return  none
 */
void InputEnable_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOC, GPIO_Pin_2); // 默认禁用
}

/*********************************************************************
 * @fn      OutputEnable_Init
 *
 * @brief   初始化输出使能 (PB4)
 *
 * @return  none
 */
void PB4_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOC, GPIO_Pin_2); // 默认禁用
}

//show voltage related line on OLED
//diff is encoder count to change target voltage
//diff=0, olny update OLED, will not set Target_V
void OLED_ShowVoltage(int16_t diff){
    char buf[32];
#ifdef REVISED_HARDWARE
    //测量值似乎偏小一些，加50mv修正显示结果
    int32_t current_voltage=ADC_GetOutputVoltage_mV() + SHOW_VOLTAGE_OFFSET;
#else
     int32_t current_voltage=ADC_GetOutputVoltage_mV();   
#endif
    int32_t next_voltage;
    int32_t target_voltage=ADC_GetTargetOutputVoltage_mv();
    uint8_t rate = 10;
    if(diff >=2 || diff <= -2) rate = 50;
    next_voltage = target_voltage + diff*rate;
    if(diff != 0){    
        //printf("target voltage: %d\r\n",target_voltage);
        //printf("current voltage: %d\r\n",current_voltage);
        //printf("raw voltage %d\r\n",voltage_buffer);
        if(next_voltage > 30000) next_voltage=30000;
        if(next_voltage < 500) next_voltage = 500;
        ADC_SetOutputVoltage_mv(next_voltage);
    }
    sprintf(buf,"V:%d.%03d %d.%03d",current_voltage/1000,current_voltage%1000,next_voltage/1000,next_voltage%1000);
    OLED_ShowString_fast(0,0,buf,16);  
}

//show current related line on OLED
//diff is encoder count to change target current
//diff=0, olny update OLED, will not set Target_I
void OLED_ShowCurrent(int16_t diff){
    char buf[32];  
    int32_t current_i=ADC_GetCurrent_mA();
               
    // 简单的数字滤波 (IIR)
    static int32_t filtered_i = 0;
    filtered_i = (filtered_i * 3 + current_i) >> 2; // 1/4 权重滤波

    int32_t target_i=ADC_GetTargetCurrent_mA();
    //printf("target current: %d\r\n",target_i);
    //printf("current current: %d\r\n",current_i);
    //printf("raw current %d\r\n",current_buffer);
    int32_t next_i = target_i + diff*10;
    if(diff != 0){
        if(next_i > 5000) next_i=5000;
        if(next_i < 100) next_i = 100;
        ADC_SetCurrent_mA(next_i);
    }
    sprintf(buf,"I:%d.%03d %d.%03d",current_i/1000,current_i%1000,next_i/1000,next_i%1000);
    OLED_ShowString_fast(0,16,buf,16);
}

// manual PWM soft start
void pwm_soft_start(uint16_t target_duty) {

    //最小从360/12=30,360/10=36可以但似乎偏大
    uint16_t current_duty = TIMER1_PERIOD/12;

    if(target_duty <= 0){
        TIM_SetCompare1(TIM1, (uint16_t)0);
    }else if(target_duty <= current_duty){
        TIM_SetCompare1(TIM1, (uint16_t)current_duty);
    }else{

        for (current_duty = TIMER1_PERIOD/10; current_duty <= target_duty; current_duty++) {
            // 更新 PWM 寄存器的值
            // CH32 使用 TIM_SetCompareX; ESP32 使用 ledcWrite
            TIM_SetCompare1(TIM1, (uint16_t)current_duty);

            // 延时以控制启动速度
            Delay_Us(100); 
        }
    }
}

/**
 * @brief 根据10倍温度计算风扇占空比
 * @param temp_x10 输入的10倍温度 (例如: 35度输入350, 70度输入700)
 * @return uint8_t 风扇Duty百分比 (0 - 100)
 */
uint8_t calculate_fan_duty(int16_t temp_x10)
{
    // 低于 35.0℃ (350)，风扇关闭
    if (temp_x10 <= 350) {
        return 0;
    }
    // 高于等于 70.0℃ (700)，风扇全速
    if (temp_x10 >= 700) {
        return 100;
    }

    // 中间设置 2 个控制点进行分段线性插值
    // 点1: 45.0℃ (450) -> 设占空比 30%
    // 点2: 60.0℃ (600) -> 设占空比 70%
    
    if (temp_x10 < 450) {
        // 区间 35.0℃ ~ 45.0℃ (Duty: 0% ~ 30%)
        return (uint8_t)(0L + (int32_t)(temp_x10 - 350) * (30 - 0) / (450 - 350));
    }
    else if (temp_x10 < 600) {
        // 区间 45.0℃ ~ 60.0℃ (Duty: 30% ~ 70%)
        return (uint8_t)(30L + (int32_t)(temp_x10 - 450) * (70 - 30) / (600 - 450));
    }
    else {
        // 区间 60.0℃ ~ 70.0℃ (Duty: 70% ~ 100%)
        return (uint8_t)(70L + (int32_t)(temp_x10 - 600) * (100 - 70) / (700 - 600));
    }
}



/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    u8 i = 0;
    //u8 button_switch=0;
    u8 oled_changed = 0;
    u8 set_voltage = 0;
    u8 brake_retry_count= 0;
    u8 duty_percent= 0;

    //uint8_t color_state = 0; // 0:红色, 1:绿色, 2:蓝色
    uint16_t prediff = 0,circle = 0,precnt = 0,cnt = 0;
    char buf[32]={0};
    uint16_t buttoncnt = 0;
    uint32_t pre_systemtick=0,peroid=0;
    uint32_t led_systemtick=0,oled_systemtick=0;
    uint32_t encoder_systemtick=0;
    uint32_t v12_mv=0,input_mv = 0,ntc_mv=0;
    int32_t temperature_10x=0;
    uint16_t adc_cnt_p=0,adc_cnt=0;
    uint32_t pre_release_ticks=0;

    SystemCoreClockUpdate();
    Delay_Init();


#if (SDI_PRINT == SDI_PR_OPEN)
    SDI_Printf_Enable();
#else
    USART_Printf_Init(115200);
#endif
    printf("SystemClk:%d\r\n", SystemCoreClock);
    printf( "ChipID:%08x\r\n", DBGMCU_GetCHIPID() );
    printf("GPIO Toggle TEST\r\n");
    GPIO_Toggle_INIT();

    //程序开始延时8秒，以便死机时可以刷写程序
    //同时点亮绿色LED，延时结束后熄灭
    GPIO_WriteBit(GPIOB, GPIO_Pin_12, Bit_SET);
    for(int i = 0; i < 20; i++) {
        Delay_Ms(100); // 100ms为单位可以自由调整延时长度
    }
    GPIO_WriteBit(GPIOB, GPIO_Pin_12, Bit_RESET);


    PD_Init( );

    USBFS_RCC_Init();
    USBFS_Device_Init(ENABLE); 

    CDC_ParaInit(true);
        
    myADC_Init();
     
    Fan_Init();  //会初始化T3中断，T3中断里会启动ADC转换

    
    //Set PB4 to GPIO Mode
    //RCC_PB1PeriphClockCmd(RCC_PB1Periph_PWR,ENABLE);
    //PWR_PB4_Config(PWR_PB4_FLOATING);
    PB4_Init();

    GPIO_WriteBit(GPIOB, GPIO_Pin_4, Bit_RESET);
    output_enabled = 0;

    WS2812B_DMA_SPI_Init();
    WS2812B_Send_GRB(0, 0, 0); //让ws2812B不再点亮，以免保持复位前状态

    //FanGPIO_Init();
    
    //Button_Init();
    EXTI4_INT_INIT();   //init button interupter

    InputEnable_Init();
    OLED_Init();
    Encoder_Init();
 
    ADC_OPA_Configure();
    OPA4_Init();
 

    Timer_Configure();  //开始注入组采样

    //OLED_Clear();
    OLED_Clear_DMA();
    //OLED_ShowString(0,0,"test",16,0);
    //OLED_Refresh();
    OLED_ShowString_fast(0,32,"OFF",16);

    //设置风扇转速为0%
    Fan_SetDuty(0); //7199为100%，10KHz频率

    //peroid = systemtick - pre_systemtick;
    //pre_systemtick = systemtick;

    //ADC_SoftwareStartInjectedConvCmd(ADC1,ENABLE);

    //确定设置占空比0
    TIM_SetCompare1(TIM1, (uint16_t)0);

    //Update_CFG();
    Load_Default_CFG();

    ADC_SetOutputVoltage_mv(config_buffer.VoltTarget[config_buffer.selected]);
    ADC_SetCurrent_mA(config_buffer.CurrentTarget[config_buffer.selected]);
    brake_retry_count = config_buffer.brakecount;

    while(1)
    {
        peroid = systemtick - pre_systemtick;
        
        
        //32bit counter will go back to 0 at about 50days
        //main loop run time must less than 255ms
        
        Tmr_Ms_Dlt = (uint8_t)(Tim_Ms_Cnt - Tmr_Ms_Cnt_Last);
        Tmr_Ms_Cnt_Last = Tim_Ms_Cnt;
        PD_Ctl.Det_Timer += Tmr_Ms_Dlt;
        if( PD_Ctl.Det_Timer > 4 )
        {
            PD_Ctl.Det_Timer = 0;
            PD_Det_Proc( );
        }
        PD_Main_Proc( );
        
        
        //sprintf(buf," %u ",peroid);
        //OLED_ShowString(0,32,buf,16,0);
        //OLED_Refresh();

        //int32_t ek = ADC_GetVEk();
        //printf("Ek: %d\r\n",ek);

        if(output_enabled == 0){
            pwm_soft_start(0);
        }

        //if(peroid >= 3000){
            //pre_systemtick = systemtick;
            //printf("current: %d\r\n",current_buffer);
        //}

        adc_cnt = ADC_IsDataReady();
        if(adc_cnt_p != adc_cnt){
            adc_cnt_p = adc_cnt;
            v12_mv = ADC_Get12VVoltage_mV();
            input_mv = ADC_GetInputVoltage_mV();
            //似乎计算结果低400mV?，修正一下
            //input_mv +=400;
            temperature_10x = ADC_GetTemperature_x10();
            if(peroid >= 20000){
                duty_percent = calculate_fan_duty(temperature_10x);
                Fan_SetDuty((FAN_PWM_PERIOD/100)*duty_percent); //7200为100%，10KHz频率,这样计算只能到99%，先乘会溢出
                pre_systemtick = systemtick;
            }
            sprintf(buf,"%d.%01d,%d.%01d,%d.%01d",v12_mv/1000,(v12_mv%1000)/100,input_mv/1000,(input_mv%1000)/100,temperature_10x/10,temperature_10x%10);
            if(menu_activated == 0) OLED_ShowString_fast(0,48,buf,16);
            //ntc_mv=ADC_GetVoltage_mV(ADC_CH_NTC);
            //sprintf(buf,"12: %d ",v12_mv);
            //OLED_ShowString(0,0,buf,16,0);
            //sprintf(buf,"in: %d ",temperature_10x);
            //OLED_ShowString(0,16,buf,16,0);
            //oled_changed = 1;

            //voltage_buffer = ADC_GetInjectedConversionValue(ADC1, ADC_InjectedChannel_1);
        }

        
        //sprintf(buf,"V: %d ",voltage_buffer);
        //OLED_ShowString(0,32,buf,16,0);
        //sprintf(buf,"I: %d ",current_buffer);
        //OLED_ShowString(0,48,buf,16,0);
        //oled_changed = 1;

         /* The code block you provided is checking if the encoder position or count has changed. If
        there is a change, it prints the new encoder position and calculates the encoder speed. */
        if(systemtick - encoder_systemtick > 200){
            encoder_systemtick = systemtick;
            cnt = TIM2->CNT;
            circle = cnt>>2;
            int16_t diff = (int16_t)(cnt - precnt);

            //消除编码器停转后的回跳一格现象
            //不确定是编码器还是干扰，这个办法似乎没有什么用
            /*if (diff*prediff <0){
                diff = 0;
                prediff = 0;
            }*/
            if (diff != 0)
            {
                //sprintf(buf," %u cir %u st", circle, TIM2->CNT);
                //OLED_ShowString(0,16,buf,16,0);
                //OLED_Refresh();
                //printf("diff %d\r\n",diff);
                if(menu_activated){
                    Process_Menu(4,diff);
                }else{
                    if(set_voltage){
                        OLED_ShowVoltage(diff);  
                    }else{
                        OLED_ShowCurrent(diff);
                    } 
                }

                oled_changed = 1;

                prediff = diff;
                precnt = cnt;
                

            }
        }
        /*if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4) == Bit_RESET) 
        {
            // 按钮被按下，执行动作
            buttoncnt++;
            sprintf(buf," %u", buttoncnt);
            //OLED_ShowString(0, 48, buf, 16, 0);
            //OLED_Refresh();
            //WS2812B_Send_GRB(0, 50, 0);
        }*/
        if (event_flag) {
            // 停止中断或清除标志位，防止计算期间数据改变
            event_flag = 0; 

            // 计算按下持续时间（处理 Tick 溢出的逻辑视具体系统而定）
            uint32_t duration = 0;
            if (release_ticks >= press_ticks) {
                duration = release_ticks - press_ticks;
            }
            
            

            // 消抖逻辑：如果按下时间太短（如小于 20ms），视为噪声
            if (duration > 200) {
                printf("按键触发！持续时间: %u ms\n", duration);
                
                // 你可以在这里根据 duration 判断是“短按”还是“长按”
                if (duration > 6000 && menu_activated == 0) {
                    //长按开启或关闭输出，开启输出可能还需要复位TIM1_BREAK
                
                    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_4) == Bit_RESET) {
                        //开启输入并等待输入电容充电
                        //电压高于6V才使用34V输出，否则使用默认18V输出
                        int32_t target_v=ADC_GetTargetOutputVoltage_mv();
                        if(target_v >= 6000){
                            GPIO_WriteBit(GPIOC, GPIO_Pin_2, Bit_SET);
                            Delay_Ms(200);
                        }
                        //测试期间，长按开启输出，duty 10%
                        //打开eg2104s使能信号，pid算法开始调整占空比
                        GPIO_WriteBit(GPIOB, GPIO_Pin_4, Bit_SET);
                        //pwm_soft_start(TIMER1_PERIOD/10);
                        output_enabled = 1;
                        //pwm_soft_start((uint16_t)SOFT_START_PWM);
                    
                        //WS2812B的颜色设置为绿色
                        led_color = 2;
                        //if(menu_activated == 0){
                        OLED_ShowString_fast(0,32,"ON ",16);
                        oled_changed = 1;
                        //}
                    }else{
                        //关闭输出eg2104s使能信号
                        GPIO_WriteBit(GPIOB, GPIO_Pin_4, Bit_RESET);
                        //关闭pwm和复位积累pid的余量
                        output_enabled = 0;
                        //pwm_soft_start((uint16_t)0);
                        //关闭输入电压
                        GPIO_WriteBit(GPIOC, GPIO_Pin_2, Bit_RESET);

                    
                        //WS2812B的颜色设置为红色
                        led_color = 1;   
                        OLED_ShowString_fast(0,32,"OFF",16);
                        oled_changed = 1;
                    }
                } else {
                    //双击，两次release之间小于800ms
                    if(buttoncnt != 0 && release_ticks - pre_release_ticks < 8000){
                        Enter_Menu();
                        menu_activated = 1;
                        buttoncnt = 0;
                        pre_release_ticks = 0;
                        oled_changed = 1;
                    } else {
                        pre_release_ticks = release_ticks;
                        buttoncnt++;
                    }
                }
            }
        }

        //按键单击处理，不能放在event_flag循环里
        //release以后900ms无其他事件，按单击处理
        if(pre_release_ticks != 0 && systemtick - pre_release_ticks > 9000){
            if(menu_activated){
                Process_Menu(1,0);
                oled_changed = 1;
            }else{
                set_voltage = set_voltage?0:1;
                //下面逻辑用于单击切换电压电流设置和存储配置
                //感觉使用比较别扭故不采用此方式
                //printf("button_switch: %d\r\n",button_switch);
                /*switch(button_switch){
                    case 0:
                        set_voltage = set_voltage?0:1;
                        button_switch++;
                        break;
                    default:
                        config_buffer.selected = button_switch-1;
                        Load_Buffer_V_I();
                        button_switch++;
                        if(button_switch > ConfigVoltageCount) button_switch = 0;
                        break;
                }*/
            }
            buttoncnt = 0;
            pre_release_ticks = 0;
        }

        if(tim1_break){
            //确定设置占空比0
            TIM_SetCompare1(TIM1, (uint16_t)0);
            tim1_break = 0;

            if(config_buffer.autorecovery && brake_retry_count){
                //尝试从brake状态恢复
                //先关闭输出
                brake_retry_count--;
                

                Recovery_From_Brake();

                //打开前级电源34.2V输出
                GPIO_SetBits(GPIOC, GPIO_Pin_2);             

                Delay_Us(100);

                //打开eg2104s输出使能信号
                GPIO_WriteBit(GPIOB, GPIO_Pin_4, Bit_SET);

                Delay_Us(50);
                pwm_soft_start(TIMER1_PERIOD/12); 

                Delay_Us(300);
                output_enabled = 1;              

            } else{
                output_enabled = 0;         

                OLED_ShowString_fast(120,32,"B",16);
                oled_changed = 1;
                brake_retry_count = config_buffer.brakecount;
            }
        }

        //OLED_Refresh_Poll(); 

        
        USB_CDC_TX_Deal(); 
        USB_CDC_RX_Deal();

        if(oled_changed && ((systemtick - oled_systemtick) > 1000)) {
            //OLED_Refresh();
            OLED_Refresh_DMA();
            oled_changed = 0;
            oled_systemtick = systemtick;
        }else if(systemtick - oled_systemtick > 10000){ //every 1000ms refresh OLED
            
            if(menu_activated == 0){
                OLED_ShowVoltage(0);
                OLED_ShowCurrent(0);
            }
            //OLED_Refresh();

            OLED_Refresh_DMA();
            oled_systemtick = systemtick;
        }

        //Delay_Ms(500);
        //300ms改变led状态一次
        if (systemtick -led_systemtick > 3000){

            led_systemtick = systemtick;

            //shrink of blue Led
            GPIO_WriteBit(GPIOB, GPIO_Pin_12, (i == 0) ? (i = Bit_SET) : (i = Bit_RESET));
            //printf("state:%d\r\n",PD_Ctl.PD_State);
            //printf("PDO:%x,%x,%x,%x\r\n",Adapter_SrcCap[0],Adapter_SrcCap[1],Adapter_SrcCap[2],Adapter_SrcCap[3]);

            //printf("PDO1:%x,%x,%x,%x\r\n",Adapter_SrcCap[4],Adapter_SrcCap[5],Adapter_SrcCap[6],Adapter_SrcCap[7]);
            //ws2812b change color 
            switch (led_color) 
            {
                case 1:
                    WS2812B_Send_GRB(0, 25, 0);   // 设置为红色(G=0, R=255, B=0)
                    led_updated = 1;               // 下一次设置为绿色
                    break;
                
                case 2:
                    WS2812B_Send_GRB(25, 0, 0);   // 设置为绿色 (G=255, R=0, B=0)
                    led_updated = 1;             // 下一次设置为蓝色
                    break;
                
                case 3:
                    WS2812B_Send_GRB(0, 0, 25);   // 设置为蓝色 (G=0, R=0, B=255)
                    led_updated = 1;             // 下一次设置为红色
                    break;
                
                default:
                    led_updated = 1;   
                    break;
            }   

        }

    }
}
