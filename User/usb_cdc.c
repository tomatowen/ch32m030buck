#include "usb_cdc.h"


__attribute__ ((aligned(4))) uint8_t  CDC_Rx_Buffer[USB_CDC_BUF_NUM_MAX][USB_CDC_PACK_LEN_MAX]; // 数据存储大数组（按包对齐）
__attribute__ ((aligned(4))) uint8_t  CDC_Tx_Buffer[USB_CDC_TX_BUF_NUM_MAX][USB_CDC_PACK_LEN_MAX];

volatile USB_CDC_Context_TypeDef User_USB_CDC_CTRL={0};

void CDC_ParaInit( uint8_t mode )
{
    //uint8_t i;

    // 1. 安全清空结构体
    memset((void *)&User_USB_CDC_CTRL, 0, sizeof(User_USB_CDC_CTRL)); 
    
    // 2. 用 memset 替代错误且危险的 memcpy 来清空缓冲区
    memset(CDC_Rx_Buffer, 0, sizeof(CDC_Rx_Buffer));
    memset(CDC_Tx_Buffer, 0, sizeof(CDC_Tx_Buffer));

    /*User_USB_CDC_CTRL.Rx_LoadNum = 0x00;
    User_USB_CDC_CTRL.Rx_DealNum = 0x00;
    User_USB_CDC_CTRL.Rx_RemainNum = 0x00;
  
    User_USB_CDC_CTRL.Tx_LoadNum = 0x00;
    User_USB_CDC_CTRL.Tx_SendNum = 0x00;
    User_USB_CDC_CTRL.Tx_RemainNum = 0x00;
    for( i = 0; i < USB_CDC_TX_BUF_NUM_MAX ; i++ )
    {
        User_USB_CDC_CTRL.Tx_PackLen[ i ] = 0x00;
    }
    Uart.Tx_Flag = 0x00;
    Uart.Tx_CurPackLen = 0x00;
    Uart.Tx_CurPackPtr = 0x00;

    User_USB_CDC_CTRL.USB_Up_IngFlag = 0x00;
    User_USB_CDC_CTRL.USB_Up_TimeOut = 0x00;
    User_USB_CDC_CTRL.USB_Up_Pack0_Flag = 0x00;
    User_USB_CDC_CTRL.FlowControl_StopFlag = 0x00;
    UARTx_Rx_DMACurCount = 0x00;
    UARTx_Rx_DMALastCount = 0x00;*/

    if( mode )
    {
        User_USB_CDC_CTRL.Com_Cfg[ 0 ] = (uint8_t)( DEF_UARTx_BAUDRATE );
        User_USB_CDC_CTRL.Com_Cfg[ 1 ] = (uint8_t)( DEF_UARTx_BAUDRATE >> 8 );
        User_USB_CDC_CTRL.Com_Cfg[ 2 ] = (uint8_t)( DEF_UARTx_BAUDRATE >> 16 );
        User_USB_CDC_CTRL.Com_Cfg[ 3 ] = (uint8_t)( DEF_UARTx_BAUDRATE >> 24 );
        User_USB_CDC_CTRL.Com_Cfg[ 4 ] = DEF_UARTx_STOPBIT;
        User_USB_CDC_CTRL.Com_Cfg[ 5 ] = DEF_UARTx_PARITY;
        User_USB_CDC_CTRL.Com_Cfg[ 6 ] = DEF_UARTx_DATABIT;
     
    }
}



uint8_t USB_CDC_SendData(uint8_t *data, uint16_t len)
{
    NVIC_DisableIRQ( USBFS_IRQn );
    NVIC_DisableIRQ( USBFS_IRQn );
   
    // 1. 检查发送队列是否已满 (对于 2 个槽位，如果积压数 >= 2，说明全忙)
    if (User_USB_CDC_CTRL.Tx_RemainNum >= USB_CDC_TX_BUF_NUM_MAX)
    {
        return 0; // 发送队列满，返回失败或等待
    }

    // 2. 临界区保护（防止在写数据时被 USB 发送中断打断）
    

    // 3. 当前 Tx_LoadNum 指向的就是【当前空闲可写】的槽位
    uint8_t target_slot = User_USB_CDC_CTRL.Tx_LoadNum;

    // 4. 拷贝数据到该空闲槽位，并记录实际长度
    // （注意：如果 len 超过 64 字节，需要分包，这里假设单次发送 <= 64）
    memcpy(CDC_Tx_Buffer[target_slot], data, len);
    User_USB_CDC_CTRL.Tx_PackLen[target_slot] = len;

    // 5. 推进写指针（环形回绕）
    User_USB_CDC_CTRL.Tx_LoadNum++;
    if (User_USB_CDC_CTRL.Tx_LoadNum >= USB_CDC_TX_BUF_NUM_MAX)
    {
        User_USB_CDC_CTRL.Tx_LoadNum = 0;
    }

    // 6. 待发送包计数 +1
    User_USB_CDC_CTRL.Tx_RemainNum++;
    NVIC_EnableIRQ( USBFS_IRQn );

    return 1;
}

void USB_CDC_TX_Deal(void){
 
    uint16_t packlen;

    NVIC_DisableIRQ( USBFS_IRQn );
    NVIC_DisableIRQ( USBFS_IRQn );
      

    if( User_USB_CDC_CTRL.Tx_RemainNum )
    {
        if( User_USB_CDC_CTRL.USB_Up_IngFlag == 0 )
        {
            /* Calculate the length of this upload */
            packlen = User_USB_CDC_CTRL.Tx_PackLen[User_USB_CDC_CTRL.Tx_SendNum];
            
            /* Upload data via usb */
            if( packlen )
            {
                //NVIC_DisableIRQ( USBFS_IRQn );
                //NVIC_DisableIRQ( USBFS_IRQn );
                User_USB_CDC_CTRL.USB_Up_IngFlag = 0x01;
                User_USB_CDC_CTRL.USB_Up_TimeOut = 0x00;
                // 这样写完全等价，而且更清爽：
                
                USBFS_Endp_DataUp( DEF_UEP3, &CDC_Tx_Buffer[User_USB_CDC_CTRL.Tx_SendNum][0], packlen, DEF_UEP_CPY_LOAD );
                /* Calculate the variables of interest */
             
                /* Start 0-length packet timeout timer */
                if( packlen == DEF_USBD_FS_PACK_SIZE )
                {
                    User_USB_CDC_CTRL.USB_Up_Pack0_Flag = 0x01;
                }

                //NVIC_EnableIRQ( USBFS_IRQn );
            }
        }
        else
        {
            /* Set the upload success flag directly if the upload is not successful after the timeout */
            if( User_USB_CDC_CTRL.USB_Up_TimeOut >= DEF_CDC_USB_UP_TIMEOUT )
            {
                User_USB_CDC_CTRL.USB_Up_IngFlag = 0x00;
                USBFS_Endp_Busy[ DEF_UEP3 ] = 0;
            }
        }
    }

    NVIC_EnableIRQ( USBFS_IRQn );

    /*****************************************************************/
    /* Determine if a 0-length packet needs to be uploaded (required for CDC mode) */
    if( User_USB_CDC_CTRL.USB_Up_Pack0_Flag )
    {
        if( User_USB_CDC_CTRL.USB_Up_IngFlag == 0 )
        {
            if( User_USB_CDC_CTRL.USB_Up_TimeOut >= ( DEF_CDC_ZERO_PACK_TIMEOUT ) )
            {
                NVIC_DisableIRQ( USBFS_IRQn );
                NVIC_DisableIRQ( USBFS_IRQn );
                User_USB_CDC_CTRL.USB_Up_IngFlag = 0x01;
                User_USB_CDC_CTRL.USB_Up_TimeOut = 0x00;
                USBFS_Endp_DataUp( DEF_UEP3, &CDC_Tx_Buffer[User_USB_CDC_CTRL.Tx_SendNum][0], 0, DEF_UEP_CPY_LOAD );
                User_USB_CDC_CTRL.USB_Up_IngFlag = 0;
                User_USB_CDC_CTRL.USB_Up_Pack0_Flag = 0x00;
                NVIC_EnableIRQ( USBFS_IRQn );
            }
        }
    }
    
}

void USB_CDC_RX_Deal(void){
    //uint16_t  count;

    // uart1 transmission processing 
   
    NVIC_DisableIRQ( USBFS_IRQn );
    NVIC_DisableIRQ( USBFS_IRQn );

  
    if(User_USB_CDC_CTRL.Rx_RemainNum){
        if(USB_CDC_SendData(CDC_Rx_Buffer[User_USB_CDC_CTRL.Rx_DealNum],User_USB_CDC_CTRL.Rx_PackLen[User_USB_CDC_CTRL.Rx_DealNum])){
            User_USB_CDC_CTRL.Rx_DealNum++;
            if(User_USB_CDC_CTRL.Rx_DealNum >= USB_CDC_BUF_NUM_MAX ){
                User_USB_CDC_CTRL.Rx_DealNum = 0;
            }
            if(User_USB_CDC_CTRL.Rx_RemainNum>0) User_USB_CDC_CTRL.Rx_RemainNum--;
        }
    }

    // If the current serial port has suspended the downlink, restart the driver downlink 
    if( (User_USB_CDC_CTRL.FlowControl_StopFlag == 0x01 ) && ( User_USB_CDC_CTRL.Rx_RemainNum < 2 ) )
    {
        USBFSD->UEP2_RX_CTRL &= ~USBFS_UEP_R_RES_MASK;
        USBFSD->UEP2_RX_CTRL |= USBFS_UEP_R_RES_ACK;
        User_USB_CDC_CTRL.FlowControl_StopFlag = 0x00;
    }

    NVIC_EnableIRQ( USBFS_IRQn );
       
   
}


/*void USB_CDC_RX_Deal(void)
{
    NVIC_DisableIRQ( USBFS_IRQn );
    NVIC_DisableIRQ( USBFS_IRQn );
    // 如果剩余数量为 0 或者异常大于最大队列数，直接强行修正并退出
    if(User_USB_CDC_CTRL.Rx_RemainNum == 0 || User_USB_CDC_CTRL.Rx_RemainNum > USB_CDC_BUF_NUM_MAX)
    {
        User_USB_CDC_CTRL.Rx_RemainNum = 0;
        return;
    }

    uint8_t idx = User_USB_CDC_CTRL.Rx_DealNum;
    uint16_t len = User_USB_CDC_CTRL.Rx_PackLen[idx];

    if(len > 0)
    {
        // 尝试发送
        if(USB_CDC_SendData(CDC_Rx_Buffer[idx], len))
        {
            // 成功发送后，立刻清空长度防止重复触发
            User_USB_CDC_CTRL.Rx_PackLen[idx] = 0; 

            // 推进读指针
            User_USB_CDC_CTRL.Rx_DealNum++;
            if(User_USB_CDC_CTRL.Rx_DealNum >= USB_CDC_BUF_NUM_MAX){
                User_USB_CDC_CTRL.Rx_DealNum = 0;
            }

            // 安全减减，带防下溢保护
            if(User_USB_CDC_CTRL.Rx_RemainNum > 0) {
                User_USB_CDC_CTRL.Rx_RemainNum--;
            }
        }
    }
    else
    {
        // 如果长度异常为0，说明这个槽位是空的，推进指针跳过它，防止死锁
        User_USB_CDC_CTRL.Rx_DealNum++;
        if(User_USB_CDC_CTRL.Rx_DealNum >= USB_CDC_BUF_NUM_MAX){
            User_USB_CDC_CTRL.Rx_DealNum = 0;
        }
        if(User_USB_CDC_CTRL.Rx_RemainNum > 0) {
            User_USB_CDC_CTRL.Rx_RemainNum--;
        }
    }
    NVIC_EnableIRQ( USBFS_IRQn );
}*/