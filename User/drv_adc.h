/**
 * @file    drv_adc.h
 * @brief   ADC 多通道采样驱动头文件
 * @version 1.0
 * @date    2024-01-01
 */

#ifndef __DRV_ADC_H
#define __DRV_ADC_H

#include "ch32m030.h"
#include "OPA.h"
#include <stdlib.h>
#include <stdbool.h>

/* ==================== 配置参数 ==================== */

/* ADC 通道定义 */
#define ADC_CHANNEL_COUNT   5               // 总通道数
#define ADC_REGULAR_CHANNEL_COUNT   3
#define ADC_DMA_BUFFER_SIZE ADC_REGULAR_CHANNEL_COUNT

#define OPA_ISP1_SEL_IO_BIT     5   //OPA ISP_CTRL的第5位控制OPA到ADC9的开关
#define OPA_ISP1_QDET1_BIT     6   //OPA ISP_CTRL的第5位控制OPA到ADC9的开关

// 定义放大倍数 (10位精度，即1024)
#define PID_SCALE_SHIFT  10
#define PID_SCALE        (1 << PID_SCALE_SHIFT)

/* 通道映射 (按 DMA 传输顺序) */
//#define ADC_CH_CURRENT      0               // 电流采样 (OPA 输出)
//#define ADC_CH_VOUT         1               // 输出电压 (PA14/ADC9)
#define ADC_CH_VIN          0               // 输入电压 (PA12/ADC12)
#define ADC_CH_V12          1               // 12V 供电 (PA13/ADC18)
#define ADC_CH_NTC          2               // NTC 温度 (PA11/ADC8)

/* 引脚定义 */
#define ADC_CURRENT_PORT    GPIOA           // 电流采样引脚 (需确认具体引脚)
#define ADC_CURRENT_PIN     GPIO_Pin_8      // PA8 (ISN1)

#define ADC_VOUT_PORT       GPIOA
#define ADC_VOUT_PIN        GPIO_Pin_7     // PA14 (ADC9)

#define ADC_VIN_PORT        GPIOA
#define ADC_VIN_PIN         GPIO_Pin_12     // PA12 (ADC19)

#define ADC_V12_PORT        GPIOA
#define ADC_V12_PIN         GPIO_Pin_13     // PA13 (ADC18)

#define ADC_NTC_PORT        GPIOA
#define ADC_NTC_PIN         GPIO_Pin_11     // PA11 (ADC8)

/* ADC 配置 */
#define ADC_RESOLUTION      ADC_Resolution_12b  // 12 位分辨率
#define ADC_CLOCK_DIV       ADC_PCLK2_Div8      // ADC 时钟分频 (24MHz/8=3MHz)
#define ADC_DMA_CHANNEL     DMA1_Channel1       // 使用 DMA1 通道 1
#define ADC_DMA_IRQ         DMA1_Channel1_IRQn  // DMA 中断通道

/* 采样触发源 */
#define ADC_INJECTED_TRIGGER_SRC     ADC_ExternalTrigConv_T1_CC5  // TIM1 CC5 触发

/* 中断优先级 */
#define ADC_NVIC_PREEMPT    0               // 最高优先级
#define ADC_NVIC_SUB        1               // 略低于 TIM1

#define ADC_DMA_NVIC_PREEMPT    0xC0               // 最低优先级组

/* 采样时间 */
//ADC_SAMPLE_TIME_FAST用于输出电压采样，PWM中点下管打开后，需要59Cycles5延迟后采样电流
//电流采样只能快速完成，采用5Cycles5
//输出电压和电流采样时间与下管打开后的采样时机有关，目前的数值已经优化过，改变后电流采样数值会不准
#define ADC_SAMPLE_TIME     ADC_SampleTime_59Cycles5  // 采样时间
#define ADC_SAMPLE_TIME_FAST     ADC_SampleTime_59Cycles5  // 快速采样时间
#define ADC_CURRENT_SAMPLE_TIME   ADC_SampleTime_5Cycles5

//超调范围200mV
#define OVERLOAD_V_LIMIT            300
#define K_COMP                      0.1
//轻载电流100mA
#define LIGHTLOAD_CURRENT           100

#define DEAD_ZONE                   1
//#define STABLE_CHECK                1
//#define DEAD_ZONE_CURRENT           1
//电压范围对应75mv,迟滞约25mv
#define DEAD_ZONE_V_LIMIT           75
#define DEAD_ZONE_V_HYS             25

//电流范围，25ma,迟滞10ma
#define DEAD_ZONE_I_LIMIT           25
#define DEAD_ZONE_I_HYS             10

//电流死区记录范围
#define WINDOW_SIZE 10



#ifdef REVISED_HARDWARE
#define OFFSET_1V_6_ADC            1995
//5A对应的ADC读数，25*5*8*4095/(3.3*1000)

#define MAX_I_LIMIT                5000
#define MIN_I_LIMIT       RATE      100
#define RATE_V_ADC              0.085
#define RATE_I_ADC             0.24

#else
#define OFFSET_1V_6_ADC            1895
//4A对应的ADC读数，25*5*8*4095/(3.3*1000)

#define MAX_I_LIMIT                4000
#define MIN_I_LIMIT                 100

#define RATE_V_ADC              0.075
#define RATE_I_ADC             0.34

#endif


// 设定积分限幅（防止定点数累加溢出，根据你的目标电流范围设置）
#define V_INTEGRAL_MAX  (MAX_I_LIMIT*PID_SCALE*0.1) // 例如：放大1024倍后的积分限幅
#define V_INTEGRAL_MIN  (-MAX_I_LIMIT*PID_SCALE*0.1)

#define FEED_FORWARD    1


/* ==================== 数据结构 ==================== */

/**
 * @brief ADC 通道配置结构体
 */
typedef struct {
    uint8_t channel;            // ADC 通道号
    GPIO_TypeDef *port;         // GPIO 端口
    uint16_t pin;               // GPIO 引脚
    uint32_t adc_ch;            // ADC 通道定义
} ADC_Channel_Config_t;

/**
 * @brief ADC 采样数据缓存结构体
 */
typedef struct {
    volatile uint16_t raw[ADC_DMA_BUFFER_SIZE];   // 原始 ADC 值
    volatile uint32_t voltage_mv[ADC_DMA_BUFFER_SIZE]; // 转换后电压 (mV)
    volatile uint32_t current_ma[ADC_DMA_BUFFER_SIZE]; // 转换后电流 (mA)
    volatile uint16_t ready;                       // 数据就绪标志
} ADC_Data_Buffer_t;


// 定义一个结构体来保存状态
typedef struct {
    int16_t buffer[WINDOW_SIZE]; // 固定的 10 个元素数组，分配在静态内存中
    uint8_t head;              // 当前写入位置的指针
    uint8_t count;             // 当前已收到的有效数据个数（最多到10）
    int16_t current_sum;         // 维护当前的滑动窗口总和
} WindowSumChecker;

typedef struct {
    uint8_t is_abs_one[WINDOW_SIZE]; // 记录最近10次每次是否满足 abs(val) == 1 (1 或 0)
    uint8_t head;                    // 循环队列指针
    uint8_t count;                   // 当前窗口内的数据总数（最多到10）
    uint8_t sum_abs_ones;           // 过去10次中绝对值为1的累计次数
    bool initialized;            
} StrictAbsChecker;

typedef struct {
    int8_t buffer[WINDOW_SIZE]; // 存整型值 (0, 1, -1)
    uint8_t head;               // 循环队列指针
    uint8_t count;
    bool initialized;             // 当前有效数据个数
} IntWindowChecker;

/* ==================== 全局变量声明 ==================== */
extern volatile uint16_t g_adc_dma_buffer[ADC_DMA_BUFFER_SIZE];  // DMA 缓冲区
extern ADC_Data_Buffer_t g_adc_data;                              // 数据处理缓冲区


// 目标值
extern int32_t Target_V; // 目标电压 ADC 值
extern int32_t Target_I; // 限制电流 ADC 值

extern volatile uint8_t output_enabled;

extern volatile int32_t voltage_buffer,current_buffer;

volatile extern uint8_t led_color;
volatile extern uint8_t led_updated;

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化 ADC 及 DMA
 */
void myADC_Init(void);

/**
 * @brief 启动 ADC 连续采样
 */
void ADC_StartConversion(void);

/**
 * @brief 停止 ADC 采样
 */
void ADC_StopConversion(void);

/**
 * @brief 获取指定通道的原始 ADC 值
 * @param channel 通道索引 (0-4)
 * @return ADC 原始值 (0-4095)
 */
uint16_t ADC_GetRawValue(uint8_t channel);

/**
 * @brief 获取指定通道的电压值 (毫伏)
 * @param channel 通道索引
 * @return 电压值 (mV)
 */
uint32_t ADC_GetVoltage_mV(uint8_t channel);

/**
 * @brief 获取输出电流值 (毫安)
 * @return 电流值 (mA)
 */
uint32_t ADC_GetCurrent_mA(void);
uint32_t ADC_GetTargetCurrent_mA(void);

/**
 * @brief 获取输出电压值 (毫伏)
 * @return 输出电压 (mV)
 */
uint32_t ADC_GetOutputVoltage_mV(void);
uint32_t ADC_GetTargetOutputVoltage_mv(void);

/**
 * @brief 设置输出电流值 (毫安)
 */
void ADC_SetCurrent_mA(int32_t i);


/**
 * @brief 获取输入电压值 (毫伏)
 * @return 输入电压 (mV)
 */
uint32_t ADC_GetInputVoltage_mV(void);


/**
 * @brief 设置输出电压值 (毫伏)
 */

void ADC_SetOutputVoltage_mv(int32_t v);

/**
 * @brief 获取 12V 供电电压值 (毫伏)
 * @return 12V 电压 (mV)
 */
uint32_t ADC_Get12VVoltage_mV(void);

/**
 * @brief 获取 NTC 温度值 (摄氏度 x10)
 * @return 温度值 (℃ * 10)
 */
int32_t ADC_GetTemperature_x10(void);

/**
 * @brief 获取数据就绪标志
 * @return 1-就绪，0-未就绪
 */
uint16_t ADC_IsDataReady(void);

/**
 * @brief 清除数据就绪标志
 */
void ADC_ClearDataReady(void);

/**
 * @brief ADC 校准
 */
void ADC_Calibration(void);

int32_t ADC_GetVEk(void);

#endif /* __DRV_ADC_H */