#include "usart_radar.h"

#include "usart_radar.h"

// 依然保留这个全局变量，与你的 main.c 兼容
uint8_t radar_target_detected = 0; 

/**
  * @brief  初始化 USART1 (PA9/PA10，用于 printf 打印光照等调试信息)
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
  * @brief 重定向 printf 函数
  */
int fputc(int ch, FILE *f)
{
    while((USART1->SR & 0X40) == 0); 
    USART1->DR = (u8)ch;      
    return ch;
}

/**
  * @brief  将原本的雷达串口初始化，爆改为 SR501 的 IO 引脚初始化
  */
void USART2_Radar_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // SR501 的 OUT 引脚连接到 PA3
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    // 使用下拉输入：防止引脚悬空时受到静电干扰产生误报
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
  * @brief  读取 SR501 状态，更新全局变量
  */
void SR501_Update_Status(void)
{
    // 如果 PA3 读到高电平，说明检测到人
    if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) == Bit_SET) 
    {
        radar_target_detected = 1;
    } 
    else 
    {
        radar_target_detected = 0;
    }
}
/*

// 定义雷达检测状态全局变量
uint8_t radar_target_detected = 0; 


  * //@brief  初始化 USART1 (PA9/PA10，用于 printf 打印调试信息)

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
    // 【修改点1】：将浮空输入改为上拉输入，防止串口线没接好时产生大量乱码(00/FF)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; 
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200; // 电脑端依然保持 115200
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);

    USART_Cmd(USART1, ENABLE);
}


  //@brief  初始化 USART2 (PA2/PA3，接雷达)

void USART2_Radar_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // PA2 TX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA3 RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    // 【修改点2】：同理，防干扰上拉输入
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; 
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 【修改点3】：海凌科的高级雷达有时默认波特率是 256000，我们改用 256000 试一次
    USART_InitStructure.USART_BaudRate = 256000; 
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    // 开启接收中断
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    // 中断优先级配置
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART2, ENABLE);
}


  //@brief 重定向 printf 函数到 USART1

int fputc(int ch, FILE *f)
{
    while((USART1->SR & 0X40) == 0); 
    USART1->DR = (u8)ch;      
    return ch;
}


  //@brief  串口2中断服务函数：收到雷达数据，直接无脑转发给电脑！

void USART2_IRQHandler(void)
{
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) 
    {
        uint8_t res = USART_ReceiveData(USART2); 
        
        // 【透传模式】：直接把收到的雷达字节，扔给USART1（发往电脑）
        while((USART1->SR & 0X40) == 0); 
        USART1->DR = res;
    } 
}
*/

