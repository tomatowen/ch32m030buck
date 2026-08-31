#include "ws2812.h"
#include<stdio.h>
#include "string.h" // 用于 memset


// 在此处实际定义变量
uint8_t ws2812b_spi_buffer[WS2812B_BUFFER_SIZE];


/*********************************************************************
 * @fn      DMA_Tx_Init
 *
 * @brief   Initializes the DMAy Channelx configuration.
 *
 * @param   DMA_CHx - x can be 1 to 7.
 *          ppadr - Peripheral base address.
 *          memadr - Memory base address.
 *          bufsize - DMA channel buffer size.
 *
 * @return  none
 */
/*void DMA_Tx_Init(DMA_Channel_TypeDef *DMA_CHx, u32 ppadr, u32 memadr, u16 bufsize)
{
    DMA_InitTypeDef DMA_InitStructure = {0};

    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA_CHx);

    DMA_InitStructure.DMA_PeripheralBaseAddr = ppadr;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)ws2812b_spi_buffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = WS2812B_BUFFER_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA_CHx, &DMA_InitStructure);
}*/

/*********************************************************************
 * @fn      WS2812B_SPI_Init
 *
 * @brief   初始化WS2812B (SPI MOSI PC3) - 使用DMA
 *
 * @return  none
 */
void WS2812B_DMA_SPI_Init(void) {
    SPI_InitTypeDef SPI_InitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    DMA_InitTypeDef DMA_InitStructure = {0};

    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOC | RCC_PB2Periph_SPI1, ENABLE);

    // PC3 (SPI1_MOSI) 配置为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // DMA1 Channel3 for SPI1 TX
    DMA_DeInit(DMA1_Channel3);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI1->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)ws2812b_spi_buffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST; // 必须是发送模式
    DMA_InitStructure.DMA_BufferSize = WS2812B_BUFFER_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal; // 每次发送后停止
    DMA_InitStructure.DMA_Priority = DMA_Priority_Low;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel3, &DMA_InitStructure);
    // DMA_Cmd(DMA1_Channel3, ENABLE); // 不立即使能, 在发送时使能

    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx; // 单线发送
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge; // 建议使用 Mode 0
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    // 72MHz / 16 = 4.5MHz. 1 bit = 222.2ns
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);
    SPI_Cmd(SPI1, ENABLE);

    // 首次发送数据填充0, 用于产生RESET信号
    memset(&ws2812b_spi_buffer, 0, WS2812B_BUFFER_SIZE);
    
    // 4. 启动DMA传输
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_SetCurrDataCounter(DMA1_Channel3, WS2812B_BUFFER_SIZE);
    DMA_Cmd(DMA1_Channel3, ENABLE);
}

/*********************************************************************
 * @fn      WS2812B_Send_GRB
 *
 * @brief   填充缓冲区并通过SPI DMA发送单个LED的GRB颜色数据
 *
 * @param   green - 绿色分量 (0-255)
 * @param   red   - 红色分量 (0-255)
 * @param   blue  - 蓝色分量 (0-255)
 *
 * @return  none
 */
void WS2812B_Send_GRB(uint8_t green, uint8_t red, uint8_t blue)
{
    // 1. 等待上一次DMA传输完成
    uint32_t timeout_counter = 0xFFFF; // 约13ms，对于驱动LED已经非常宽裕
    while((!DMA_GetFlagStatus(DMA1_FLAG_TC3)))
    {
        if (timeout_counter == 0)
        {
            printf("Error: DMA transfer timeout for previous transfer!\r\n");
            return; // 超时，放弃本次发送
        }
        timeout_counter--;
    }

    // 2. 将24位颜色数据 (GRB顺序) 打包成SPI码流
    uint32_t color_data = (green << 16) | (red << 8) | blue;
    int buffer_bit_index = 0;

    // 遍历颜色的24个bit (从MSB到LSB)
    for (int i = 23; i >= 0; i--)
    {
        uint8_t code = ((color_data >> i) & 0x01) ? WS2812B_CODE_1 : WS2812B_CODE_0;

        // 将4-bit的SPI码打包进发送缓冲区
        for (int j = 3; j >= 0; j--)
        {
            if ((code >> j) & 0x01) {
                ws2812b_spi_buffer[buffer_bit_index / 8] |= (1 << (7 - (buffer_bit_index % 8)));
            } else {
                ws2812b_spi_buffer[buffer_bit_index / 8] &= ~(1 << (7 - (buffer_bit_index % 8)));
            }
            buffer_bit_index++;
        }
    }

    // 3. 在数据末尾填充0, 用于产生RESET信号
    memset(&ws2812b_spi_buffer[WS2812B_NUM_LEDS * WS2812B_DATA_BYTES_PER_LED], 0, WS2812B_RESET_BYTES);
    
    // 4. 启动DMA传输
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_SetCurrDataCounter(DMA1_Channel3, WS2812B_BUFFER_SIZE);
    DMA_Cmd(DMA1_Channel3, ENABLE);
}