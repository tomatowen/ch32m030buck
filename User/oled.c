#include "oled.h"
#include "oledfont.h" // 必须包含字库头文件
#include "string.h" // 用于 memset


static OLED_State_TypeDef oled_state = OLED_IDLE;

// 显存缓冲 (128 * 64 / 8)
uint8_t OLED_GRAM[8][128];

// 1. 定义大数组，刚好 1025 字节
uint8_t OLED_DMA_TxBuf[OLED_DMA_BUFFER_SIZE];

// 在全局或静态定义一个页索引变量
static uint8_t current_page = 0;


/**
 * @brief DMA1 通道 1 中断服务程序
 * @note  在 TIM1 更新中断后触发，处理采样数据
 */
void DMA1_Channel4_IRQHandler(void)  __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel4_IRQHandler(void) {
    if(DMA_GetITStatus(DMA1_IT_TC4) != RESET) {
        oled_state = OLED_IDLE;
        DMA_Cmd(DMA1_Channel4, DISABLE);
        I2C_GenerateSTOP(I2C1, ENABLE);
        DMA_ClearITPendingBit(DMA1_IT_TC4);
    }
}


 /*********************************************************************
 * @fn      DMA_Tx_Init
 *
 * @brief   Initializes the SPI1 DMAy Channelx configuration.
 *
 * @param   DMA_CHx -
 *          ppadr - Peripheral base address.
 *          memadr - Memory base address.
 *          bufsize - DMA channel buffer size.
 *
 * @return  none
 */
void DMA_Tx_Init(DMA_Channel_TypeDef *DMA_CHx, u32 ppadr, u32 memadr, u16 bufsize)
{
    DMA_InitTypeDef DMA_InitStructure={0};

    RCC_HBPeriphClockCmd( RCC_HBPeriph_DMA1, ENABLE );

    DMA_DeInit(DMA_CHx);

    DMA_InitStructure.DMA_PeripheralBaseAddr = ppadr;
    DMA_InitStructure.DMA_MemoryBaseAddr = memadr;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = bufsize;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init( DMA_CHx, &DMA_InitStructure );



}

void OLED_DMA_Config_Init(void) {
    // 假设你的 DMA 目标地址和源地址是固定不变的
    // 这里把配置执行一次，之后只需要开关通道即可
    DMA_Tx_Init(DMA1_Channel4, (u32)&I2C1->DATAR, (u32)OLED_DMA_TxBuf, OLED_DMA_BUFFER_SIZE);
    
    NVIC_SetPriority(DMA1_Channel4_IRQn, 0xC0);
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    
    /* 使能 DMA 传输完成中断 */
    DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE);

    // 注意：初次配完后可以先不使能，或者保持 Disable 状态
    DMA_Cmd(DMA1_Channel4, DISABLE);


}

// 初始化 I2C1 GPIO 和 外设
static void I2C_Configuration(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2C_InitTypeDef I2C_InitTSturcture = {0};

    // 1. 开启时钟 (GPIOB 和 I2C1)
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOB | RCC_PB2Periph_AFIO, ENABLE);
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_I2C1, ENABLE);

    // 2. 配置 GPIO (PB2-SDA, PB3-SCL) 为复用开漏输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 3. 配置 I2C 参数
    I2C_InitTSturcture.I2C_ClockSpeed = 400000; // 400kHz 快速模式
    //I2C_InitTSturcture.I2C_ClockSpeed = 80000; // 100kHz 模式
    I2C_InitTSturcture.I2C_Mode = I2C_Mode_I2C;
    I2C_InitTSturcture.I2C_DutyCycle = I2C_DutyCycle_2;
    //I2C_InitTSturcture.I2C_DutyCycle = I2C_DutyCycle_16_9;
    I2C_InitTSturcture.I2C_OwnAddress1 = 0x00; // 主机模式下自身地址不重要
    I2C_InitTSturcture.I2C_Ack = I2C_Ack_Enable;
    I2C_InitTSturcture.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &I2C_InitTSturcture);

    I2C_DMACmd(I2C1, ENABLE);

    // 4. 使能 I2C
    I2C_Cmd(I2C1, ENABLE);
}

// 发送一个字节数据
// addr: 设备地址
// reg:  寄存器 (0x00 命令, 0x40 数据)
// data: 内容
static void OLED_Write_Byte(uint8_t reg, uint8_t data) {
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY));

    I2C_GenerateSTART(I2C1, ENABLE);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C1, OLED_ADDRESS, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C1, reg);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2C1, data);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(I2C1, ENABLE);
}

// 写命令
void OLED_WR_Cmd(uint8_t cmd) {
    OLED_Write_Byte(0x00, cmd);
}

// 写数据
void OLED_WR_Data(uint8_t data) {
    OLED_Write_Byte(0x40, data);
}

/* =========================================================================
 *  SSD1315 驱动逻辑
 * ========================================================================= */

void OLED_Refresh_DMA(void) {

    if (oled_state != OLED_IDLE) return; // 上一帧还没发完，直接跳过
    // 1. 先通过常规或单次写发送定位命令（页寻址或水平寻址：定位到 Page0, Col 0）
    OLED_WR_Cmd(0x20); // 设置寻址模式
    OLED_WR_Cmd(0x00); // 水平寻址模式
    //OLED_WR_Cmd(0x40); // 【关键】设置显示起始行（Display Start Line）为 0 (0x40 表示起始行 0)
    //OLED_WR_Cmd(0xB0); // 页地址 0
    //OLED_WR_Cmd(0x00); // 列低地址
    //OLED_WR_Cmd(0x10); // 列高地址

    OLED_WR_Cmd(0x21); // 设置列地址范围
    OLED_WR_Cmd(0x00); // 列起始 = 0
    OLED_WR_Cmd(0x7F); // 列结束 = 127

    OLED_WR_Cmd(0x22); // 设置页地址范围
    OLED_WR_Cmd(0x00); // 页起始 = 0
    OLED_WR_Cmd(0x07); // 页结束 = 7

    // 2. 准备发送数据：确保 TxData[0] = 0x40，后面 1024 字节是你的 OLED_GRAM 展开数据
    OLED_DMA_TxBuf[0] = 0x40; 
    
    // 将列优先的 GRAM 重新打包为水平寻址所需的页优先格式

    /*uint16_t idx = 1;
    for (uint8_t page = 0; page < 8; page++) {
        for (uint8_t col = 0; col < 128; col++) {
            OLED_DMA_TxBuf[idx++] = OLED_GRAM[page][col]; // 注意维度的对应关系
        }
    }*/

    memcpy(&OLED_DMA_TxBuf[1], &OLED_GRAM[0][0], OLED_DMA_BUFFER_SIZE-1);


    // 3. 配置 DMA 并启动
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) != RESET);
  
    I2C_GenerateSTART(I2C1, ENABLE);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));
    
    I2C_Send7bitAddress(I2C1, OLED_ADDRESS, I2C_Direction_Transmitter); // 注意换成你的OLED实际I2C地址
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    //I2C_SendData(I2C1, 0x40); // 显式手动发控制字节，绝不丢包！
    //while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTING));


    // 1. 先关闭 DMA（修改配置前必须先 Disable）
    DMA_Cmd(DMA1_Channel4, DISABLE);

    // 2. 重新把内存起始指针指向你的缓冲区头部
    DMA1_Channel4->MADDR = (uint32_t)OLED_DMA_TxBuf; 

    // 3. 重新加载传输总字节数（1025）
    DMA_SetCurrDataCounter(DMA1_Channel4, OLED_DMA_BUFFER_SIZE);

    // 4. 再次使能 DMA，开始搬运
    DMA_Cmd(DMA1_Channel4, ENABLE);      

    // 切换状态为“正在发送”，立刻退出函数！
    oled_state = OLED_SENDING;

    // 如果追求极致的非阻塞，甚至连最后的 while 都可以改用 DMA 传输完成中断（TCIF）来触发 STOP
    //while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    //I2C_GenerateSTOP(I2C1, ENABLE);
}


// 2. 放在主循环（Main Loop）里高频轮询的检查函数
void OLED_Refresh_Poll(void) {
    if (oled_state == OLED_SENDING) {
        // 非阻塞检查：最后一个字节发完了吗？
        if (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
            // 发完了！拉起停止信号
            I2C_GenerateSTOP(I2C1, ENABLE);
            DMA_Cmd(DMA1_Channel4, DISABLE);
            
            // 恢复空闲状态
            oled_state = OLED_IDLE;
        }
        // 如果还没发完，函数直接返回，绝不卡死主循环！
    }
}
 
// 每次主循环只调用一次这个函数，8次主循环才能刷完一屏
void OLED_Refresh_Slice(void) {
    uint8_t n;
    
    // 刷当前这一页 (current_page: 0 ~ 7)
    OLED_WR_Cmd(0xB0 + current_page); 
    OLED_WR_Cmd(0x00); 
    OLED_WR_Cmd(0x10); 
    
    for (n = 0; n < 128; n++) {
        OLED_WR_Data(OLED_GRAM[current_page][n]);
    }
    
    // 页指针自增，下次主循环进来时刷下一页
    current_page++;
    if (current_page >= 8) {
        current_page = 0; // 一屏刷完，归零
    }
}

// 更新显存到 OLED
void OLED_Refresh(void) {
    uint8_t i, n;
    for (i = 0; i < 8; i++) {
        OLED_WR_Cmd(0xB0 + i); // 设置页地址 (Page 0~7)
        OLED_WR_Cmd(0x00);     // 设置列低地址
        OLED_WR_Cmd(0x10);     // 设置列高地址
        
        // I2C 连续写入优化：发送 Start -> Addr -> Data_Reg -> Data...Data -> Stop
        // 这里为了简单直接调用单字节写，如果追求速度可改为连续写
        for (n = 0; n < 128; n++) {
            OLED_WR_Data(OLED_GRAM[i][n]);
        }
    }
}

// 清屏
void OLED_Clear(void) {
    memset(OLED_GRAM, 0, sizeof(OLED_GRAM));
    OLED_Refresh();
}

void OLED_Clear_DMA(void){
    memset(OLED_GRAM, 0, sizeof(OLED_GRAM));
    OLED_Refresh_DMA();
}

/**
  * 函    数：将OLED显存数组部分清零
  * 参    数：X 指定区域左上角的横坐标，范围：0~OLED_WIDTH-1
  * 参    数：Y 指定区域左上角的纵坐标，范围：0~OLED_HEIGHT-1
  * 参    数：Width 指定区域的宽度，范围：0~OLED_WIDTH
  * 参    数：Height 指定区域的高度，范围：0~OLED_HEIGHT
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
 
  void OLED_ClearArea(int16_t X, int16_t Y, int16_t Width, int16_t Height)
  {
	  int16_t x_start, y_start, x_end, y_end;
	  int16_t i, j;
  
	  if (Width <= 0 || Height <= 0) return;
  
	  // 计算X方向的起始和结束位置
	  x_start = (X < 0) ? 0 : X;
	  x_end = X + Width;
	  if (x_end > OLED_WIDTH) x_end = OLED_WIDTH;
	  if (x_start >= x_end) return;
  
	  // 计算Y方向的起始和结束位置
	  y_start = (Y < 0) ? 0 : Y;
	  y_end = Y + Height;
	  if (y_end > OLED_HEIGHT) y_end = OLED_HEIGHT;
	  if (y_start >= y_end) return;
  
	  // 调整Width和Height为实际需要清除的区域
	  Width = x_end - x_start;
	  Height = y_end - y_start;
  
	  for (j = y_start; j < y_end; j++) {
		  for (i = x_start; i < x_end; i++) {
			  OLED_GRAM[j/8][i] &= ~(0x01 << (j % 8));
		  }
	  }
  }

  /**
  * 函    数：将OLED显存数组全部取反
  * 参    数：无
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_Reverse(void)
{
	uint8_t i, j;
	for (j = 0; j < (OLED_HEIGHT/8); j ++)				//遍历页
	{
		for (i = 0; i < OLED_WIDTH; i ++)			//遍历OLED_WIDTH列
		{
			OLED_GRAM[j][i] ^= 0xFF;	//将显存数组数据全部取反
		}
	}
}

/**
  * 函    数：将OLED显存数组部分取反
  * 参    数：X 指定区域左上角的横坐标，范围：负数~OLED_WIDTH-1
  * 参    数：Y 指定区域左上角的纵坐标，范围：负数~OLED_HEIGHT
  * 参    数：Width 指定区域的宽度，范围：负数~OLED_WIDTH
  * 参    数：Height 指定区域的高度，范围：负数~OLED_HEIGHT
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_ReverseArea(int16_t X, int16_t Y, int16_t Width, int16_t Height)
{
	int16_t i, j, x, y;
	if(Width <= 0 || Height <= 0) {return; }
	/*参数检查，保证指定区域不会超出屏幕范围*/
	if (X > OLED_WIDTH-1) {return;}
	if (Y > OLED_HEIGHT-1) {return;}
	if (X + Width > OLED_WIDTH) {Width = OLED_WIDTH - X;}
	if (Y + Height > OLED_HEIGHT) {Height = OLED_HEIGHT - Y;}
	if (X + Width < 0) {return;}
	if (Y + Height < 0) {return;}
	if (X < 0) { x = 0;} else { x = X;}
	if (Y < 0) { y = 0;} else { y = Y;}
	
	for (j = y; j < Y + Height; j ++)		//遍历指定页
	{
		for (i = x; i < X + Width; i ++)	//遍历指定列
		{
			OLED_GRAM[j/8][i] ^= 0x01 << (j % 8);	//将显存数组指定数据取反
		}
	}
}

/**
  * 函    数：OLED显示图像 BY BILIBILI上nm网课呢 xy轴均可为负
  * 参    数：X 指定图像左上角的横坐标，范围：负值~OLED_WIDTH-1
  * 参    数：Y 指定图像左上角的纵坐标，范围：负值~OLED_HEIGHT-1
  * 参    数：Width 指定图像的宽度，范围：正数
  * 参    数：Height 指定图像的高度，范围：正数
  * 参    数：Image 指定要显示的图像
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_ShowImage(int16_t X, int16_t Y, uint16_t Width, uint16_t Height, const uint8_t *Image)
{
      uint8_t i, j;
    
    /* 参数检查，保证指定图像不会超出屏幕范围 */
    if (Width == 0 || Height == 0) {
        return; // 如果宽度或高度为0，直接返回
    }
    
    if (X > OLED_WIDTH-1) {
        return; // X 超出右边界，直接返回
    }
    if (Y > OLED_HEIGHT-1) {
        return; // Y 超出下边界，直接返回
    }
    
    /* 将图像所在区域清空 */
    uint8_t startX = (X < 0) ? 0 : X; // 计算实际起始显示位置的 X 坐标
    uint8_t startY = (Y < 0) ? 0 : Y; // 计算实际起始显示位置的 Y 坐标
    uint8_t endX = (X + Width - 1 > OLED_WIDTH-1) ? OLED_WIDTH-1 : X + Width - 1; // 计算实际结束显示位置的 X 坐标
    uint8_t endY = (Y + Height - 1 > OLED_HEIGHT-1) ? OLED_HEIGHT-1 : Y + Height - 1; // 计算实际结束显示位置的 Y 坐标
    
    OLED_ClearArea(startX, startY, endX - startX + 1, endY - startY + 1);
    
    /* 遍历指定图像涉及的相关页 */
    for (j = 0; j < (Height - 1) / 8 + 1; j++)
    {
        /* 遍历指定图像涉及的相关列 */
        for (i = 0; i < Width; i++)
        {
            int16_t currX = X + i;
            int16_t currY = Y + j * 8;
            
            /* 超出边界，则跳过显示 */
            if (currX < 0 || currX > OLED_WIDTH-1 ||currY < 0 || currY > OLED_HEIGHT-1) {
                continue;
            }
			/* 显示图像在当前页的内容 */
			OLED_GRAM[currX][currY / 8] |= Image[j * Width + i] << (currY % 8);
			/* 当前页下一页 */
			if (currY + 8 <= OLED_HEIGHT-1) {
				OLED_GRAM[currX][currY / 8 + 1] |= Image[j * Width + i] >> (8 - currY % 8);
			}
        }
    }
	/*if(Y<0){
		for (i = 0; i < Width; i++)
        {
            int16_t currX = X + i;
			if (currX < 0 || currX > OLED_WIDTH-1) {
                continue;
            }
			OLED_GRAM[0][currX] |= Image[ -Y/8*Width+i] >> -Y%8;
		}
	}*/
}

// 画点
// t: 1 填充, 0 清除
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t t) {
    uint8_t pos, bx, temp = 0;
    if (x > 127 || y > 63) return;
    pos = y / 8; // 标准坐标：Page 0 在上方
    bx = y % 8;
    temp = 1 << bx; // 标准逻辑：LSB (位0) 在上方
    //temp = 1 << (7 - bx);
    if (t) OLED_GRAM[pos][x] |= temp;
    else   OLED_GRAM[pos][x] &= ~temp;
}

// 显示字符 (仅支持 8x16)
// mode: 1 叠加, 0 非叠加
/* void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size, uint8_t mode) {
    uint8_t temp, t, t1;
    uint8_t y0 = y;
    // 简单的偏移计算，假设 F8X16 包含了完整的 ASCII，这里需要根据实际字库调整
    // 如果字库是从空格(32)开始的：
    uint8_t c = chr - ' '; 
    
    // 注意：此处代码依赖于你提供的字库结构，这里仅演示逻辑
    // 假设使用 8x16 字体
    if(size == 16) {
        for (t = 0; t < 16; t++) {

            // 适配标准字库格式 (前8字节为上半部分，后8字节为下半部分)
            // 绘图逻辑是先画一列的上半部分，再画下半部分，所以需要交替读取
            if (t % 2 == 0) temp = F8X16[c * 16 + (t / 2)];      // 读取上半部分字节
            else            temp = F8X16[c * 16 + (t / 2) + 8];  // 读取下半部分字节
                        
            for (t1 = 0; t1 < 8; t1++) {
                if (temp & 0x80) OLED_DrawPoint(x, y, 1);
                else if (mode == 0) OLED_DrawPoint(x, y, 0);
                temp <<= 1;
                y++;
                if ((y - y0) == 16) {
                    y = y0;
                    x++;
                    break;
                }
            }
        }
    }
    else if(size == 8) {
        for(t = 0; t < 6; t++) {
            temp = F6X8[c * 6 + t]; // 修正变量名大小写，与 oledfont.h 保持一致
            for(t1 = 0; t1 < 8; t1++) {
                if(temp & 0x01) OLED_DrawPoint(x, y, 1); // 6x8字体通常低位(LSB)在上方
                else if(mode == 0) OLED_DrawPoint(x, y, 0);
                temp >>= 1; // 低位处理完，右移
                y++;
            }
            x++; // 画完一列，X坐标右移
            y = y0; // Y坐标复位
        }
    }
}
 */

// 显示字符 (支持 6x8 和 8x16)
// x, y: 起始坐标
// chr: 字符内容
// size: 8 代表 6x8 字体, 16 代表 8x16 字体
// mode: 1 叠加, 0 非叠加
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size, uint8_t mode) {
    uint8_t temp, t, t1;
    uint8_t y0 = y;
    uint8_t c = chr - ' '; // 计算字符在字库中的索引偏移

    // --- 16 像素高度处理 (8x16) ---
    if(size == 16) {
        for (t = 0; t < 8; t++) { // 8x16 有 8 列
            // 标准 8x16 字库通常：前8字节是上半部分，后8字节是下半部分

            // --- 处理上半部分 (第0-7字节) ---
            temp = F8X16[c * 16 + t]; 
            for (t1 = 0; t1 < 8; t1++) {
                if (temp & 0x01) OLED_DrawPoint(x, y, 1); 
                else if (mode == 0) OLED_DrawPoint(x, y, 0);
                temp >>= 1;
                y++;
            }
            
            // --- 处理下半部分 (第8-15字节) ---
            temp = F8X16[c * 16 + t + 8]; 
            for (t1 = 0; t1 < 8; t1++) {
                if (temp & 0x01) OLED_DrawPoint(x, y, 1);
                else if (mode == 0) OLED_DrawPoint(x, y, 0);
                temp >>= 1;
                y++;
            }
            
            x++;    // 移动到下一列
            y = y0; // 重置 Y 坐标准备画下一列
        }
    }
    // --- 8 像素高度处理 (6x8) ---
    //如果字库高位在前
    //if(temp & 0x01) 改为 if(temp & 0x80)
    //temp >>= 1; 改为 temp <<= 1;
    else if(size == 8) {
        for(t = 0; t < 6; t++) { // 6x8 有 6 列
            temp = F6X8[c * 6 + t]; 
            for(t1 = 0; t1 < 8; t1++) {
                if(temp & 0x01) OLED_DrawPoint(x, y, 1); // LSB 低位在前
                else if(mode == 0) OLED_DrawPoint(x, y, 0);
                temp >>= 1;
                y++;
            }
            x++;    // 移动到下一列
            y = y0; // 重置 Y 坐标
        }
    }
}

// 显示字符 (支持 6x8 和 8x16)
// x, y: 起始坐标
// chr: 字符内容
// size: 8 代表 6x8 字体, 16 代表 8x16 字体
// mode: 1 叠加, 0 非叠加
void OLED_ShowChar_fast(uint8_t x, uint8_t y, char chr, uint8_t size) {
    uint8_t y0 = y;
    uint8_t c = chr - ' '; // 计算字符在字库中的索引偏移

    y0 = y / 8; // 计算起始页


    // --- 16 像素高度处理 (8x16) ---
    if(size == 16) {
            // 因为 8x16 字符宽度是 8 像素，高度是 16 像素（占 2 个页）
        for (uint8_t i = 0; i < 8; i++) {
            if (x + i >= 128) break; // 边界检查
                    
            // 直接将字库数据“复制”到显存中！
            // 上半部分存入当前页
            OLED_GRAM[y0][x+i] = F8X16[c * 16 + i];     
            // 下半部分存入下一页
            OLED_GRAM[y0+1][x+i] = F8X16[c * 16 + i + 8]; 
        }      
       
    }
    // --- 8 像素高度处理 (6x8) ---
    //如果字库高位在前
    //if(temp & 0x01) 改为 if(temp & 0x80)
    //temp >>= 1; 改为 temp <<= 1;
    else if(size == 8) {
           for (uint8_t i = 0; i < 6; i++) {
                if (x + i >= 128) break; // 边界检查
                        
                // 直接将字库数据“复制”到显存中！
                // 上半部分存入当前页
                OLED_GRAM[y0][x+i]  =  F6X8[c * 6 + i];     
               
        }
    }
}


// 显示字符串
/* void OLED_ShowString(uint8_t x, uint8_t y, char *chr, uint8_t size, uint8_t mode) {
    while ((*chr >= ' ') && (*chr <= '~')) {
        OLED_ShowChar(x, y, *chr, size, mode);
        if (size == 8) x += 6;
        else x += 8;
        chr++;
    }
} */

void OLED_ShowString_fast(uint8_t x, uint8_t y, char *chr, uint8_t size) {
    while (*chr != '\0') {
        // 检查 X 轴是否越界，如果剩余空间不足以画下一个字符则自动换行
        uint8_t width = (size == 16) ? 8 : 6;
        if (x + width > 128) {
            x = 0;
            y += size;
        }

        // 检查 Y 轴是否越界
        if (y + size > 64) break; 

        OLED_ShowChar_fast(x, y, *chr, size);
        x += width;
        chr++;
    }
}

/**
  * 函    数：OLED显示数字（十进制，正整数）
  * 参    数：X 指定数字左上角的横坐标，范围：负数~OLED_WIDTH-1
  * 参    数：Y 指定数字左上角的纵坐标，范围：负数~OLED_HEIGHT-1
  * 参    数：Number 指定要显示的数字，范围：负数~4294967295
  * 参    数：Length 指定数字的长度，范围：负数~10
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16		宽8像素，高16像素
  *                 OLED_6X8		宽6像素，高8像素
  * 				 OLED_7X12		宽7像素，高12像素
  *                 OLED_10X20		宽10像素，高20像素
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
    uint8_t width = (FontSize == 16) ? 8 : 6;
    // 优化：计算初始的最高位除数（例如 Length=3，divisor=100）
    uint32_t divisor = 1;
    for (i = 0; i < Length - 1; i++) {
        divisor *= 10;
    }

    for (i = 0; i < Length; i++)
    {
        uint8_t digit = (Number / divisor) % 10; // 提取当前最高位
        OLED_ShowChar_fast(X + i * width, Y, digit + '0', FontSize);
        divisor /= 10; // 权重降级，供下一轮循环使用
    }
}

/**
  * 函    数：OLED显示有符号数字（十进制，整数）
  * 参    数：X 指定数字左上角的横坐标，范围：负数~OLED_WIDTH-1
  * 参    数：Y 指定数字左上角的纵坐标，范围：负数~OLED_HEIGHT-1
  * 参    数：Number 指定要显示的数字，范围：-2147483648~2147483647
  * 参    数：Length 指定数字的长度，范围：负数~10
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16		宽8像素，高16像素
  *                 OLED_6X8		宽6像素，高8像素
  * 				 OLED_7X12		宽7像素，高12像素
  *                 OLED_10X20		宽10像素，高20像素
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize)
{
    uint8_t i;
    uint32_t Number1;
    uint8_t width = (FontSize == 16) ? 8 : 6;
    
    // 1. 处理符号位与绝对值转换（安全处理 INT32_MIN）
    if (Number >= 0) {
        OLED_ShowChar_fast(X, Y, '+', FontSize);
        Number1 = (uint32_t)Number;
    } else {
        OLED_ShowChar_fast(X, Y, '-', FontSize);
        // 防止 -INT32_MIN 溢出，强制转为无符号数取负
        Number1 = (uint32_t)(-Number); 
    }
    
    // 2. 优化：预先计算最高位除数，替代循环中的 OLED_Pow
    uint32_t divisor = 1;
    for (i = 0; i < Length - 1; i++) {
        divisor *= 10;
    }
    
    // 3. 循环显示数字各位（整体向右偏移一个字符宽度）
    for (i = 0; i < Length; i++)
    {
        uint8_t digit = (Number1 / divisor) % 10;
        OLED_ShowChar_fast(X + (i + 1) * width, Y, digit + '0', FontSize);
        divisor /= 10;
    }
}

/**
  * 函    数：OLED显示十六进制数字（十六进制，正整数）
  * 参    数：X 指定数字左上角的横坐标，范围：负数~OLED_WIDTH-1
  * 参    数：Y 指定数字左上角的纵坐标，范围：负数~OLED_HEIGHT-1
  * 参    数：Number 指定要显示的数字，范围：0x00000000~0xFFFFFFFF
  * 参    数：Length 指定数字的长度，范围：负数~8
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16		宽8像素，高16像素
  *                 OLED_6X8		宽6像素，高8像素
  * 				 OLED_7X12		宽7像素，高12像素
  *                 OLED_10X20		宽10像素，高20像素
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
    uint8_t i;
    uint8_t width = (FontSize == 16) ? 8 : 6;

    for (i = 0; i < Length; i++) 
    {
        // 1. 动态计算当前需要提取第几位（从高位到低位）
        // 原本的 Length - i - 1，换算成二进制右移的位数就是：(Length - 1 - i) * 4
        uint8_t shift = (Length - 1 - i) * 4;
        
        // 右移并用 0x0F 掩码提取出当前 4 bit 的十六进制值
        uint8_t SingleNumber = (Number >> shift) & 0x0F;
        
        // 2. 精简的字符转换逻辑（利用 ASCII 码规律）
        // 如果 < 10，加上 '0'；如果 >= 10，减 10 再加上 'A'
        char c = (SingleNumber < 10) ? (SingleNumber + '0') : (SingleNumber - 10 + 'A');
        
        // 3. 显示字符
        OLED_ShowChar_fast(X + i * width, Y, c, FontSize);
    }
}

/**
  * 函    数：OLED显示二进制数字（二进制，正整数）
  * 参    数：X 指定数字左上角的横坐标，范围：负数~OLED_WIDTH-1
  * 参    数：Y 指定数字左上角的纵坐标，范围：负数~OLED_HEIGHT-1
  * 参    数：Number 指定要显示的数字，范围：0x00000000~0xFFFFFFFF
  * 参    数：Length 指定数字的长度，范围：负数~16
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16		宽8像素，高16像素
  *                 OLED_6X8		宽6像素，高8像素
  * 				 OLED_7X12		宽7像素，高12像素
  *                 OLED_10X20		宽10像素，高20像素
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
    uint8_t i;
    uint8_t width = (FontSize == 16) ? 8 : 6;
    
    for (i = 0; i < Length; i++)        // 遍历二进制的每一位
    {
        // 1. 计算当前位需要右移的位数（从高位到低位）
        uint8_t shift = Length - 1 - i;
        
        // 2. 右移并用 & 0x01 提取出当前位的 0 或 1
        uint8_t bit = (Number >> shift) & 0x01;
        
        // 3. 转换为字符并显示
        OLED_ShowChar_fast(X + i * width, Y, bit + '0', FontSize);
    }
}

/**
  * 函    数：OLED显示浮点数字（十进制，小数）
  * 参    数：X 指定数字左上角的横坐标，范围：负数~OLED_WIDTH-1
  * 参    数：Y 指定数字左上角的纵坐标，范围：负数~OLED_HEIGHT-1
  * 参    数：Number 指定要显示的数字，范围：-4294967295.0~4294967295.0
  * 参    数：IntLength 指定数字的整数位长度，范围：0~10
  * 参    数：FraLength 指定数字的小数位长度，范围：0~9，小数进行四舍五入显示
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16		宽8像素，高16像素
  *                 OLED_6X8		宽6像素，高8像素
  * 				 OLED_7X12		宽7像素，高12像素
  *                 OLED_10X20		宽10像素，高20像素
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize)
{
    uint32_t PowNum, IntNum, FraNum;
    uint8_t width = (FontSize == 16) ? 8 : 6;
    uint8_t currentX = X; // 用一个游标来管理横坐标，绝对不会算错位！
    
    // 1. 显示符号位
    if (Number >= 0) {
        OLED_ShowChar_fast(currentX, Y, '+', FontSize);
    } else {
        OLED_ShowChar_fast(currentX, Y, '-', FontSize);
        Number = -Number;
    }
    currentX += width; // 符号位占用一个宽度
    
    // 2. 提取整数部分和小数部分
    IntNum = (uint32_t)Number;                    // 提取整数
    double decimalPart = Number - (double)IntNum; // 提取纯小数部分
    
    // 计算小数乘数（避免每次调用 OLED_Pow）
    PowNum = 1;
    for (uint8_t i = 0; i < FraLength; i++) {
        PowNum *= 10;
    }
    
    // 四舍五入处理小数部分
    FraNum = (uint32_t)(decimalPart * PowNum + 0.5); 
    
    // 处理小数四舍五入引起的进位
    if (FraNum >= PowNum) {
        IntNum += 1;
        FraNum = 0; // 进位后小数部分归零（或者保留有效位，这里简化处理）
    }
    
    // 3. 显示整数部分
    OLED_ShowNum(currentX, Y, IntNum, IntLength, FontSize);
    currentX += IntLength * width; // 推进整数部分占用的宽度
    
    // 4. 显示小数点
    OLED_ShowChar_fast(currentX, Y, '.', FontSize);
    currentX += width; // 小数点占用一个宽度
    
    // 5. 显示小数部分
    OLED_ShowNum(currentX, Y, FraNum, FraLength, FontSize);
}




void OLED_ShowString(uint8_t x, uint8_t y, char *chr, uint8_t size, uint8_t mode) {
    while (*chr != '\0') {
        // 检查 X 轴是否越界，如果剩余空间不足以画下一个字符则自动换行
        uint8_t width = (size == 16) ? 8 : 6;
        if (x + width > 128) {
            x = 0;
            y += size;
        }

        // 检查 Y 轴是否越界
        if (y + size > 64) break; 

        OLED_ShowChar(x, y, *chr, size, mode);
        x += width;
        chr++;
    }
}

/**
  * 函    数：OLED显示汉字串
  * 参    数：X 指定数字左上角的横坐标，范围：0~OLED_WIDTH-1
  * 参    数：Y 指定数字左上角的纵坐标，范围：0~OLED_HEIGHT-1
  * 参    数：Chinese 指定要显示的汉字串，范围：必须全部为汉字或者全角字符，不要加入任何半角字符
  *           显示的汉字需要在OLED_Data.c里的OLED_CF16x16数组定义
  *           未找到指定汉字时，会显示默认图形（一个方框，内部一个问号）
  * 参    数：FontSize 指定中文文字大小，OLED_8X8_FULL,OLED_12X12_FULL,OLED_16X16_FULL,OLED_20X20_FULL
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
/*void OLED_ShowChinese(int16_t X, int16_t Y, char *Chinese, uint8_t FontSize)
{
    uint8_t pChinese = 0;
    uint8_t pIndex;
    uint8_t i;
    char SingleChinese[OLED_CHN_CHAR_WIDTH + 1] = {0};
    
    for (i = 0; Chinese[i] != '\0'; i ++)    // 遍历汉字串
    {
        SingleChinese[pChinese] = Chinese[i];    // 提取汉字串数据到单个汉字数组
        pChinese ++;                            // 计次自增
        
        if (pChinese >= OLED_CHN_CHAR_WIDTH)    // 提取到了一个完整的汉字
        {
            pChinese = 0;    // 计次归零
            
			const void* fontArray;
			if (FontSize == OLED_8X8_FULL) {
					fontArray = (const void*) OLED_CF8x8;
			}else
			if (FontSize == OLED_12X12_FULL) {
					fontArray = (const void*) OLED_CF12x12;
			}else
			if (FontSize == OLED_16X16_FULL) {
					fontArray = (const void*) OLED_CF16x16;
			}else
			if (FontSize == OLED_20X20_FULL) {
				fontArray = (const void*) OLED_CF20x20;
			}
 	
			if(FontSize==OLED_8X8_FULL){
				for (pIndex = 0; strcmp(((const ChineseCell8x8_t*)fontArray)[pIndex].Index, "") != 0; pIndex ++)
				{
					if (strcmp(((const ChineseCell8x8_t*)fontArray)[pIndex].Index, SingleChinese) == 0)
					{
						break;
					}
				}
				OLED_ShowImage(X + ((i + 1) / OLED_CHN_CHAR_WIDTH - 1) * OLED_8X8_FULL, Y, OLED_8X8_FULL, OLED_8X8_FULL, ((const ChineseCell8x8_t*)fontArray)[pIndex].Data);
			}else
			if(FontSize==OLED_12X12_FULL){
				for (pIndex = 0; strcmp(((const ChineseCell12x12_t*)fontArray)[pIndex].Index, "") != 0; pIndex ++)
				{
					if (strcmp(((const ChineseCell12x12_t*)fontArray)[pIndex].Index, SingleChinese) == 0)
					{
						break;
					}
				}
				OLED_ShowImage(X + ((i + 1) / OLED_CHN_CHAR_WIDTH - 1) * OLED_12X12_FULL, Y, OLED_12X12_FULL, OLED_12X12_FULL, ((const ChineseCell12x12_t*)fontArray)[pIndex].Data);
			}else
			if(FontSize==OLED_16X16_FULL){
				for (pIndex = 0; strcmp(((const ChineseCell16x16_t*)fontArray)[pIndex].Index, "") != 0; pIndex ++)
				{
					if (strcmp(((const ChineseCell16x16_t*)fontArray)[pIndex].Index, SingleChinese) == 0)
					{
						break;
					}
				}
				OLED_ShowImage(X + ((i + 1) / OLED_CHN_CHAR_WIDTH - 1) * OLED_16X16_FULL, Y, OLED_16X16_FULL, OLED_16X16_FULL, ((const ChineseCell16x16_t*)fontArray)[pIndex].Data);
			}else
			if(FontSize==OLED_20X20_FULL){
				for (pIndex = 0; strcmp(((const ChineseCell20x20_t*)fontArray)[pIndex].Index, "") != 0; pIndex ++)
				{
					if (strcmp(((const ChineseCell20x20_t*)fontArray)[pIndex].Index, SingleChinese) == 0)
					{
						break;
					}
				}
				OLED_ShowImage(X + ((i + 1) / OLED_CHN_CHAR_WIDTH - 1) * OLED_20X20_FULL, Y, OLED_20X20_FULL, OLED_20X20_FULL, ((const ChineseCell20x20_t*)fontArray)[pIndex].Data);
			}
        }
    }
}*/

/**
  * @brief OLED显示混合字符串（汉字与ASCII）
  * @param X 指定汉字串左上角的横坐标，范围：负值~OLED_WIDTH-1
  * @param Y 指定汉字串左上角的纵坐标，范围：负值~OLED_HEIGHT-1
  * @param String 指定要显示的混合字符串，范围：全角字符与半角字符都可以
  *        显示的汉字需要在OLED_Data.c里的OLED_CF16x16或OLED_CF12x12数组定义
  *        未找到指定汉字时，会显示默认图形（一个方框，内部一个问号）
  * @param ChineseFontSize 指定中文文字大小，OLED_8X8_FULL,OLED_12X12_FULL,OLED_16X16_FULL,OLED_20X20_FULL
  * @param ASCIIFontSize  指定ASCII文字大小,OLED_6X8_HALF,OLED_7X12_HALF,OLED_8X16_HALF,OLED_10X20_HALF
  * @return 无
  */
/*void OLED_ShowMixString(int16_t X, int16_t Y, char *String, uint8_t ChineseFontSize, uint8_t ASCIIFontSize) 
{
    while (*String != '\0') {
        if (*String & 0x80) { // 判断是否是中文字符 (最高位为1表示中文字符)
			char Chinese[OLED_CHN_CHAR_WIDTH+1];
			for (uint8_t i=0;i<OLED_CHN_CHAR_WIDTH;i++){
				Chinese[i] = *(String+i);
			}
			Chinese[OLED_CHN_CHAR_WIDTH] = '\0';
			OLED_ShowChinese(X, Y, Chinese, ChineseFontSize);
			X += ChineseFontSize;  // 中文字符宽度
			String += OLED_CHN_CHAR_WIDTH;  // 跳过当前的两个字节的中文字符
        } else {
            // 如果是ASCII字符
            OLED_ShowChar(X, Y, *String, ASCIIFontSize);
            X += ASCIIFontSize; // ASCII字符宽度
            String++; // 指向下一个字符
        }
    }
}*/

/**
  * 函    数：OLED使用printf函数打印格式化字符串,可以是中英文混杂的字符串。此函数由bilibili@上nm网课呢 添加
  * 参    数：X 指定汉字串左上角的横坐标，范围：负值~OLED_WIDTH-1
  * 参    数：Y 指定汉字串左上角的纵坐标，范围：负值~OLED_HEIGHT-1
  * 参    数：ChineseFontSize 指定汉字字体大小，OLED_8X8_FULL,OLED_12X12_FULL,OLED_16X16_FULL,OLED_20X20_FULL
  * 参    数：ASCIIFontSize 指定ASCII字体大小,OLED_6X8_HALF,OLED_7X12_HALF,OLED_8X16_HALF,OLED_10X20_HALF
  * 参    数：format 指定要显示的格式化字符串，范围：ASCII码可见字符组成的字符串
  * 参    数：... 格式化字符串参数列表
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */

/*void OLED_PrintfMix(int16_t X, int16_t Y, uint8_t ChineseFontSize,uint8_t ASCIIFontSize,const char *format, ...)
{
	char String[MAX_STRING_LENGTH];						//定义字符数组
	va_list arg;							//定义可变参数列表数据类型的变量arg
	va_start(arg, format);					//从format开始，接收参数列表到arg变量
	vsprintf(String, format, arg);			//使用vsprintf打印格式化字符串和参数列表到字符数组中
	va_end(arg);							//结束变量arg
	OLED_ShowMixString( X, Y, String, ChineseFontSize,ASCIIFontSize);//OLED显示字符数组（字符串）
}*/

void Backup_GRAM(uint8_t *buffer){
    memcpy(buffer,&OLED_GRAM[0][0],sizeof(OLED_GRAM));
}

void Restore_GRAM(uint8_t *buffer){
    memcpy(&OLED_GRAM[0][0],buffer,sizeof(OLED_GRAM));
}


// 初始化 SSD1315
void OLED_Init(void) {
    I2C_Configuration();

    // 初始化 DMA，长度设为 1025 (1字节控制头 + 1024字节显存)
    OLED_DMA_Config_Init(); 
    
    // 延时等待屏幕上电稳定
    Delay_Ms(200); // 需要外部提供延时函数

    OLED_WR_Cmd(0xAE); // 关闭显示

    OLED_WR_Cmd(0x20); // 设置内存寻址模式
    OLED_WR_Cmd(0x10); // 00:水平, 01:垂直, 10:页(默认)
    //OLED_WR_Cmd(0x00); 

    OLED_WR_Cmd(0xB0); // 设置页起始地址 0~7

    OLED_WR_Cmd(0xC8); // 设置COM扫描方向 (C0:正常, C8:翻转)
    OLED_WR_Cmd(0x00); // 设置低列地址
    OLED_WR_Cmd(0x10); // 设置高列地址

    OLED_WR_Cmd(0x40); // 设置起始行 0

    OLED_WR_Cmd(0x81); // 对比度设置
    OLED_WR_Cmd(0xFF); // 亮度 (0x00~0xFF)

    OLED_WR_Cmd(0xA1); // 设置段重映射 (A0:正常, A1:翻转)

    OLED_WR_Cmd(0xA6); // 正常显示 (A7:反色)

    OLED_WR_Cmd(0xA8); // 设置复用率
    OLED_WR_Cmd(0x3F); // 1/64

    OLED_WR_Cmd(0xA4); // 全局显示开启 (A4:跟随RAM, A5:全亮)

    OLED_WR_Cmd(0xD3); // 设置显示偏移
    OLED_WR_Cmd(0x00);

    OLED_WR_Cmd(0xD5); // 设置晶振频率/分频
    OLED_WR_Cmd(0xF0);

    OLED_WR_Cmd(0xD9); // 预充电周期
    OLED_WR_Cmd(0x22);

    OLED_WR_Cmd(0xDA); // COM硬件引脚配置
    OLED_WR_Cmd(0x12);

    OLED_WR_Cmd(0xDB); // VCOMH 反压
    OLED_WR_Cmd(0x20);

    OLED_WR_Cmd(0x8D); // 电荷泵设置
    OLED_WR_Cmd(0x14); // 开启 (必须开启，否则屏幕不亮)

    OLED_WR_Cmd(0xAF); // 开启显示

    OLED_Clear_DMA();
}
