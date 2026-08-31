#ifndef __DRV_CONFIG_H
#define __DRV_CONFIG_H

#include "stdio.h"
#include "ch32m030_flash.h"
#include "string.h"
#include "drv_adc.h"
//#include <assert.h>

#define Fadr    0x08010000-128
#define Fsize   (128>>2)

#define ConfigVoltageCount     (5)

//default target voltage in mv
#define VoltTarget1     3300
#define VoltTarget2     5000
#define VoltTarget3     9000
#define VoltTarget4     12000
#define VoltTarget5     15000

//default current target in mA
#define CurrentTarget1   1000

//brake retry count before disable output
#define BrakeCount      5

//Voltage offset
#define Voltage_OFFSET  0

//Current offset
#define Current_OFFSET  0

/*__attribute__ ((packed)) typedef struct{
    uint32_t magicnumber;//0-3, 0x30564346
    uint16_t VoltTarget[5];//4~13
    uint16_t CurrentTarget[5];//14~23

    uint8_t selected;//24
    uint8_t autorecovery;//25
    uint8_t brakecount;//26
    uint8_t Reserved0; //27

    uint16_t Reserved1;//28~29

    uint16_t Reserved2;//30~37
    uint16_t Reserved3;
    uint16_t Reserved4;
    uint16_t Reserved5;
    uint16_t crc;

} CFG_params;*/

#if defined(__GNUC__)
#pragma pack(push, 1)
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
#pragma pack(1)
#endif

typedef struct {
    uint32_t magicnumber;       // 0-3  (4 bytes),0x30564346
    uint16_t VoltTarget[5];     // 4-13 (10 bytes)
    uint16_t CurrentTarget[5];  // 14-23 (10 bytes)

    uint8_t  selected;          // 24 (1 byte)
    uint8_t  autorecovery;      // 25 (1 byte)
    uint8_t  brakecount;        // 26 (1 byte)
    uint8_t  voltage_offset;         // 27 (1 byte)

    uint8_t current_offset;         // 28-29 (2 bytes)
    uint8_t Reserved1;

    uint16_t Reserved2;         // 30-31 (2 bytes)
    uint16_t Reserved3;         // 32-33 (2 bytes)
    uint16_t Reserved4;         // 34-35 (2 bytes)
    uint16_t Reserved5;         // 36-37 (2 bytes)
    uint16_t crc;               // 38-39 (2 bytes)
    
    // 动态计算剩余需要填充的字节数
    // 总目标 128 字节 - 前面已确定的 40 字节 = 88 字节
    uint8_t  padding[128 - (
        sizeof(uint32_t) + 
        sizeof(uint16_t) * 10 + 
        sizeof(uint8_t)  * 4  + 
        sizeof(uint16_t) * 6
    )];
} CFG_params;

#if defined(__GNUC__)
#pragma pack(pop)
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
#pragma pack()
#endif

// 【双重保险】编译期断言：如果结构体大小不是 128 字节，编译直接报错！

//static_assert(sizeof(CFG_params) == 128, "CFG_params size must be exactly 128 bytes!");

extern CFG_params config_buffer;

void Update_CFG();//上电第一次配置更新
void CfgUpdateToFlashFunc();//配置写入FLASH
void Load_Default_CFG();
uint16_t calculate_crc16_modbus(const uint8_t *data, uint16_t length);
void Load_Buffer_V_I(void);

#endif