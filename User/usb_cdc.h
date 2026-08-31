
#ifndef USER_USB_CDC_H_
#define USER_USB_CDC_H_

#include "stdio.h"
#include "string.h"
#include "ch32m030_usbfs_device.h"


#define USB_CDC_PACK_LEN_MAX    64   // 根据你的 USB 全速端点最大包长调整（通常是 64）
#define USB_CDC_BUF_NUM_MAX     6    // 环形缓冲区的包数量槽位（按需调整大小）
#define USB_CDC_TX_BUF_NUM_MAX  6
#define DEF_CDC_USB_UP_TIMEOUT   60000                                        /* Serial port receive upload timeout, in 100uS */
#define DEF_CDC_ZERO_PACK_TIMEOUT       30*20                                           /* Serial port receive timeout, in 100uS */

/* Serial port receive timeout related macro definition */
#define DEF_UARTx_BAUDRATE         115200                                       /* Default baud rate for serial port */
#define DEF_UARTx_STOPBIT          0                                            /* Default stop bit for serial port */
#define DEF_UARTx_PARITY           0                                            /* Default parity bit for serial port */
#define DEF_UARTx_DATABIT          8                                            /* Default data bit for serial port */


typedef struct {
    // --- 1. 接收方向（下行：PC -> MCU）---
    
    uint16_t Rx_PackLen[USB_CDC_BUF_NUM_MAX];                      // 每包的实际有效字节长度
    volatile uint8_t Rx_LoadNum;                                   // 硬件中断写指针（存入索引）
    volatile uint8_t Rx_DealNum;                                   // 主程序读指针（消费索引）
    volatile uint8_t Rx_RemainNum;                                 // 剩余未处理的包计数
    uint8_t  FlowControl_StopFlag;                                 // 流控暂停标志 (1: 缓存快满，发NAK; 0: 正常ACK)

    // --- 2. 发送方向（上行：MCU -> PC，如果要做回环或主动上报需要用到）---
    //uint8_t  Tx_Buffer[USB_CDC_PACK_LEN_MAX];                      // 发送暂存区
    //volatile uint8_t Tx_BusyFlag;                                  // 发送忙碌标志 (1: 正在发，0: 空闲)

    // 发送环形队列
    
    uint16_t Tx_PackLen[USB_CDC_TX_BUF_NUM_MAX];
    volatile uint8_t Tx_LoadNum;    // 主程序往里写（填充）的指针
    volatile uint8_t Tx_SendNum;    // 中断/底层往外发（消费）的指针
    volatile uint8_t Tx_RemainNum;  // 当前队列里堆积了多少包待发送
    volatile uint16_t USB_Up_TimeOut;
    volatile uint8_t USB_Up_Pack0_Flag;
    volatile uint8_t USB_Up_IngFlag;

    // --- 3. 虚拟串口控制配置（CDC-ACM 协议必须保留的参数槽位）---
    uint8_t  Com_Cfg[ 7 ]; 
    //volatile uint32_t BaudRate;                                             // 虚构的波特率
    //volatile uint8_t  StopBits;                                             // 停止位
    //volatile uint8_t  Parity;                                               // 校验位
    //volatile uint8_t  DataBits;                                             // 数据位
} USB_CDC_Context_TypeDef;


extern __attribute__ ((aligned(4))) uint8_t  CDC_Rx_Buffer[USB_CDC_BUF_NUM_MAX][USB_CDC_PACK_LEN_MAX]; // 数据存储大数组（按包对齐）
extern __attribute__ ((aligned(4))) uint8_t  CDC_Tx_Buffer[USB_CDC_TX_BUF_NUM_MAX][USB_CDC_PACK_LEN_MAX];

extern volatile USB_CDC_Context_TypeDef User_USB_CDC_CTRL;

void CDC_ParaInit( uint8_t mode);
void USB_CDC_TX_Deal(void);
void USB_CDC_RX_Deal(void);


#endif