#include "drv_config.h"



CFG_params config_buffer;


/**
 * @brief  计算 CRC-16/MODBUS 校验值
 * @param  data: 指向待校验数据的指针
 * @param  length: 数据的字节长度
 * @return uint16_t 计算出的 CRC 校验码
 */
uint16_t calculate_crc16_modbus(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF; // 初始值

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i]; // 每次异或一个字节
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001; // 多项式 0x8005 的位反转形式
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void Load_Default_CFG(void){

    //0. 先初始化为0
    memset(&config_buffer,0,sizeof(config_buffer));
    
    // 1. 给配置参数赋值...
    config_buffer.magicnumber = 0x30564346; // 示例魔数
    config_buffer.VoltTarget[0] = VoltTarget1; //3.3V
    config_buffer.CurrentTarget[0] = CurrentTarget1; //1A

    config_buffer.VoltTarget[1] = VoltTarget2; //5V
    config_buffer.CurrentTarget[1] = CurrentTarget1; //1A

    config_buffer.VoltTarget[2] = VoltTarget3; //9V
    config_buffer.CurrentTarget[2] = CurrentTarget1; //1A

    config_buffer.VoltTarget[3] = VoltTarget4; //12V
    config_buffer.CurrentTarget[3] = CurrentTarget1; //1A

    config_buffer.VoltTarget[4] = VoltTarget5; //15V
    config_buffer.CurrentTarget[4] = CurrentTarget1; //1A

    //config_buffer.selected = 0;
    //config_buffer.autorecovery = 0;
    config_buffer.brakecount = BrakeCount;
    //config_buffer.Reserved0 = 0;
    //config_buffer.Reserved1 = 0;
    //config_buffer.Reserved2 = 0;
    //config_buffer.Reserved3 = 0;
    //config_buffer.Reserved4 = 0;
    //config_buffer.Reserved5 = 0;
    

    // 2. 计算 CRC
    // 计算范围：从 magicnumber 开始，到 crc 字段之前（即 sizeof(CFG_params) - sizeof(uint16_t)）
    uint16_t payload_len = sizeof(CFG_params) - sizeof(uint16_t) -sizeof(config_buffer.padding);
    config_buffer.crc = calculate_crc16_modbus((uint8_t*)&config_buffer, payload_len);

    // 3. 写入 Flash（直接写 128 字节）
    //flash_write(FLASH_ADDR, (uint8_t*)&my_config, sizeof(CFG_params));
}



void CfgUpdateToFlashFunc()
{
    CFG_params* mconfig = &config_buffer;
    //CFG_params* config =  (CFG_params*)(Fadr);

    //log_i("开始擦除Flash配\r\n");
    uint8_t s = FLASH_ROM_ERASE(Fadr, Fsize*4);
    if(s != FLASH_COMPLETE)
    {
        printf("ERROR: ERASE FAIL\r\n");
        return;
        //擦除失败
    }else {
        //擦除完成
        printf("ERASE success!\r\n");
    }
    //log_i("开始写入Flash配置\r\n");
    s = FLASH_ROM_WRITE(Fadr,(uint32_t*)mconfig, Fsize*4);
    if(s != FLASH_COMPLETE)
    {
        printf("ERROR: WRITE FAIL\r\n");
        //写入失败
    }else {
        //写入完成
        printf("Write sucess!!\r\n");
        //Update_CFG();
    }
    //printf("test buffer size: %d\r\n",sizeof(test_buffer));
    //memcpy(&test_buffer,config,sizeof(test_buffer));
}
//上电第一次更新配置，FLASH若无配置则将默认配置写入
void Update_CFG()
{
    CFG_params* mconfig =  (CFG_params*)(Fadr);

    

    if( mconfig->magicnumber != 0x30564346)
    {
        printf("Flash config invalid, writing default\r\n");
        Load_Default_CFG();
        CfgUpdateToFlashFunc();
        return;
    }else{
        memcpy(&config_buffer,mconfig,sizeof(config_buffer));
    }

    uint16_t payload_len = sizeof(CFG_params) - sizeof(uint16_t)-sizeof(mconfig->padding);
    uint16_t calculated_crc = calculate_crc16_modbus((uint8_t*)&config_buffer, payload_len);

    if (calculated_crc != config_buffer.crc) {
        // 错误处理：CRC 校验失败（数据损坏），加载默认参数
        printf("Read config from flash: CRC ERROR!\r\n");
        Load_Default_CFG();
        CfgUpdateToFlashFunc();
    } 

}

void Load_Buffer_V_I(void){
    ADC_SetOutputVoltage_mv(config_buffer.VoltTarget[config_buffer.selected]);
    ADC_SetCurrent_mA(config_buffer.CurrentTarget[config_buffer.selected]);
}

