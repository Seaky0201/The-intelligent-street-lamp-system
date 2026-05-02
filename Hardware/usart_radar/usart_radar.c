#include "usart_radar.h"

// SR501 人体红外传感器检测状态全局变量
uint8_t radar_target_detected = 0; 

/**
  * @brief  初始化 USART1 (PA9/PA10，用于 printf 打印调试信息)
  */
void USART1_Debug_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    // PA9 TX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA10 RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);

    USART_Cmd(USART1, ENABLE);
}

/**
  * @brief 重定向 printf 函数到 USART1
  */
int fputc(int ch, FILE *f)
{
    while((USART1->SR & 0X40) == 0); 
    USART1->DR = (u8)ch;      
    return ch;
}

/**
  * @brief  初始化 SR501 人体红外传感器 IO 引脚
  * @note   SR501 OUT 引脚连接到 PA4 (已从 PA3 改为 PA4，因为 PA3 被 Air790E USART2_RX 占用)
  */
void SR501_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // SR501 OUT --> PA4，下拉输入防止悬空误报
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
  * @brief  读取 SR501 状态，更新全局变量 radar_target_detected
  * @note   PA4 高电平 = 检测到人，低电平 = 无人
  */
void SR501_Update_Status(void)
{
    if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4) == Bit_SET) 
    {
        radar_target_detected = 1;
    } 
    else 
    {
        radar_target_detected = 0;
    }
}
