#ifndef __BH1750_H
#define __BH1750_H

#include "stm32f10x.h"

// 引脚定义：PB6-SCL, PB7-SDA
#define SCL_PIN     GPIO_Pin_6
#define SDA_PIN     GPIO_Pin_7
#define I2C_PORT    GPIOB

void BH1750_Init(void);        // 初始化引脚和传感器
float BH1750_ReadLux(void);    // 读取光照强度(Lux)

#endif
