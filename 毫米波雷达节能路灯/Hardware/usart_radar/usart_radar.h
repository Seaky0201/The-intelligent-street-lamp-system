#ifndef __USART_RADAR_H
#define __USART_RADAR_H

#include "stm32f10x.h"
#include <stdio.h>

extern uint8_t radar_target_detected; 

void USART1_Debug_Init(void);
void USART2_Radar_Init(void);
void SR501_Update_Status(void); // 新增状态更新函数声明

#endif


