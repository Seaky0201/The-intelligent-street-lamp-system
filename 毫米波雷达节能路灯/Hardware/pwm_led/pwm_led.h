#ifndef __PWM_LED_H
#define __PWM_LED_H

#include "stm32f10x.h"

void PWM_LED_Init(void);
void Set_Streetlight_Brightness(uint8_t percentage);

#endif
