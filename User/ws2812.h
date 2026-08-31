#ifndef __WS2812_H
#define __WS2812_H

#include "ch32m030.h" // 请确保引用了正确的CH32M030标准库头文件
#include "ch32m030_conf.h"

// WS2812B 定义 (已修正和扩展)
#define WS2812B_NUM_LEDS    1 // 假设有1个LED
// WS2812B SPI bit patterns (at 4.5MHz SPI clock, 4-bit per logic bit)
#define WS2812B_CODE_1      0xE    // 0b1110: H=666ns, L=222ns
#define WS2812B_CODE_0      0x8    // 0b1000: H=222ns, L=666ns
// Number of zero bytes to send for a RESET signal (>50us)
#define WS2812B_RESET_BYTES 100    // 增加Reset时长到约350us，兼容更多型号灯珠
#define WS2812B_DATA_BYTES_PER_LED (24 * 4 / 8) // 12 bytes per LED
#define WS2812B_BUFFER_SIZE (WS2812B_NUM_LEDS * WS2812B_DATA_BYTES_PER_LED + WS2812B_RESET_BYTES)


extern uint8_t ws2812b_spi_buffer[WS2812B_BUFFER_SIZE]; // WS2812B SPI DMA 发送缓冲区

void WS2812B_DMA_SPI_Init(void);
void WS2812B_Send_GRB(uint8_t green, uint8_t red, uint8_t blue);

#endif