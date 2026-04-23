#ifndef __AHT20_H
#define __AHT20_H

#include "stm32f10x.h"

// AHT20 I2C 地址 (7位地址 0x38，左移1位后：写=0x70，读=0x71)
#define AHT20_ADDR_W    0x70
#define AHT20_ADDR_R    0x71

// AHT20 命令
#define AHT20_CMD_INIT        0xBE
#define AHT20_CMD_TRIGGER     0xAC
#define AHT20_CMD_SOFTRESET   0xBA

// AHT20 数据结构
typedef struct {
    float temperature;    // 温度 (°C)
    float humidity;       // 湿度 (%RH)
} AHT20_Data;

void AHT20_Init(void);                   // 初始化 AHT20
uint8_t AHT20_ReadData(AHT20_Data *data); // 读取温湿度数据，返回0成功，1失败

#endif
