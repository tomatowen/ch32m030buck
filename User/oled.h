#ifndef __OLED_H
#define __OLED_H

#include "ch32m030.h" // 请确保引用了正确的CH32M030标准库头文件

// OLED I2C 地址 (通常是 0x78, 如果不通尝试 0x7A)
#define OLED_ADDRESS    0x78

// OLED 屏幕分辨率
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_DMA_BUFFER_SIZE 1025

/**关于字符串最大长度的宏，用于格式化输出字符串*/
#define  MAX_STRING_LENGTH          128
/*中文字符字节宽度*/
#define OLED_CHN_CHAR_WIDTH			(2)		//UTF-8编码格式给3，GB2312编码格式给2

// 定义 OLED 刷新的发送状态
typedef enum {
    OLED_IDLE = 0,
    OLED_SENDING
} OLED_State_TypeDef;


// 接口函数声明
void OLED_Init(void);

void OLED_Clear(void);
void OLED_ClearArea(int16_t X, int16_t Y, int16_t Width, int16_t Height);

void OLED_Refresh(void);
void OLED_ColorTurn(uint8_t mode); // 0:正常, 1:反色
void OLED_DisplayTurn(uint8_t mode); // 0:正常, 1:翻转180度

void OLED_Clear_DMA(void);
void OLED_Refresh_DMA(void);
//void OLED_Refresh_Poll(void);

void OLED_Refresh_Slice(void);


// 绘图函数
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t t);
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size, uint8_t mode);
void OLED_ShowString(uint8_t x, uint8_t y, char *chr, uint8_t size, uint8_t mode);

void OLED_ShowChar_fast(uint8_t x, uint8_t y, char chr, uint8_t size);
void OLED_ShowString_fast(uint8_t x, uint8_t y, char *chr, uint8_t size);

//屏幕反色
void OLED_Reverse(void);
void OLED_ReverseArea(int16_t X, int16_t Y, int16_t Width, int16_t Height);
//绘制位图
void OLED_ShowImage(int16_t X, int16_t Y, uint16_t Width, uint16_t Height, const uint8_t *Image);
//显示ASCII字符与数字

void OLED_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize);
//显示中英文字符串

void Backup_GRAM(uint8_t *buffer);
void Restore_GRAM(uint8_t *buffer);


#endif
