#ifndef __USART_RADAR_H
#define __USART_RADAR_H

#include "stm32f10x.h"
#include <stdio.h>

// SR501 人体红外传感器检测状态
extern uint8_t radar_target_detected; 

// 函数声明
void USART1_Debug_Init(void);     // 初始化调试串口 (PA9-TX, PA10-RX)
void SR501_GPIO_Init(void);       // 初始化 SR501 传感器引脚 (PA4)
void SR501_Update_Status(void);   // 读取 SR501 状态，更新 radar_target_detected

#endif
