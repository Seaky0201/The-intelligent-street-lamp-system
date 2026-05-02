#include "air790e.h"
#include "Delay.h"

/*
 * Air790E 4G Cat.1 通信模块驱动
 * 通信方式：USART2 (PA2-TX, PA3-RX)
 * 波特率：115200
 *
 * 本驱动实现 TCP DTU 透传模式，
 * 用于将路灯传感器数据透传至云平台。
 */

// 全局接收缓冲区
uint8_t  air790e_rx_buf[AIR790E_RX_BUF_SIZE];
uint16_t air790e_rx_len = 0;
uint8_t  air790e_rx_flag = 0;

/**
  * @brief  初始化 USART2 用于 Air790E 通信
  * @note   PA2-TX, PA3-RX, 波特率 115200
  */
void Air790E_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 开启时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // PA2 - USART2_TX 复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA3 - USART2_RX 浮空输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // USART2 参数配置
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    // 开启接收中断
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    // NVIC 中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART2, ENABLE);
}

/**
  * @brief  通过 USART2 发送一个字节
  */
static void Air790E_SendByte(uint8_t byte)
{
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, byte);
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
}

/**
  * @brief  通过 USART2 发送字符串
  */
static void Air790E_SendString(char *str)
{
    while (*str)
    {
        Air790E_SendByte(*str++);
    }
}

/**
  * @brief  清空接收缓冲区
  */
void Air790E_ClearRxBuf(void)
{
    memset(air790e_rx_buf, 0, AIR790E_RX_BUF_SIZE);
    air790e_rx_len = 0;
    air790e_rx_flag = 0;
}

/**
  * @brief  在接收缓冲区中查找目标字符串
  * @param  str: 要查找的字符串
  * @retval 1=找到, 0=未找到
  */
static uint8_t Air790E_FindString(char *str)
{
    if (strstr((const char *)air790e_rx_buf, str) != NULL)
    {
        return 1;
    }
    return 0;
}

/**
  * @brief  发送 AT 命令并等待期望响应
  * @param  cmd:        AT 命令字符串 (如 "AT\r\n")
  * @param  expect:     期望的响应字符串 (如 "OK")
  * @param  timeout_ms: 超时时间 (毫秒)
  * @retval Air790E_Status
  */
Air790E_Status Air790E_SendAT(char *cmd, char *expect, uint16_t timeout_ms)
{
    uint16_t wait = 0;

    Air790E_ClearRxBuf();
    Air790E_SendString(cmd);

    // 等待响应
    while (wait < timeout_ms)
    {
        Delay_ms(10);
        wait += 10;

        if (air790e_rx_len > 0)
        {
            // 检查是否收到期望的响应
            if (Air790E_FindString(expect))
            {
                return AIR790E_OK;
            }
            // 检查是否返回错误
            if (Air790E_FindString("ERROR"))
            {
                return AIR790E_ERROR;
            }
        }
    }

    return AIR790E_TIMEOUT;
}

/**
  * @brief  Air790E 模块初始化
  * @note   依次执行：AT握手 -> 关回显 -> 检查SIM卡 -> 检查网络注册 -> 检查信号
  * @retval Air790E_Status
  */
Air790E_Status Air790E_ModuleInit(void)
{
    uint8_t retry;

    printf("[Air790E] Module initializing...\r\n");

    // 1. AT 握手测试 (最多重试5次)
    for (retry = 0; retry < 5; retry++)
    {
        if (Air790E_SendAT("AT\r\n", "OK", AIR790E_TIMEOUT_SHORT) == AIR790E_OK)
        {
            printf("[Air790E] AT handshake OK\r\n");
            break;
        }
        Delay_ms(1000);
    }
    if (retry >= 5)
    {
        printf("[Air790E] AT handshake FAILED!\r\n");
        return AIR790E_ERROR;
    }

    // 2. 关闭回显
    Air790E_SendAT("ATE0\r\n", "OK", AIR790E_TIMEOUT_SHORT);
    Delay_ms(100);

    // 3. 检查 SIM 卡
    if (Air790E_SendAT("AT+CPIN?\r\n", "READY", AIR790E_TIMEOUT_SHORT) != AIR790E_OK)
    {
        printf("[Air790E] SIM card not detected!\r\n");
        return AIR790E_ERROR;
    }
    printf("[Air790E] SIM card OK\r\n");
    Delay_ms(100);

    // 4. 检查网络注册状态 (等待注网，最多30秒)
    for (retry = 0; retry < 15; retry++)
    {
        // CREG? 返回 +CREG: 0,1 或 +CREG: 0,5 表示已注册
        if (Air790E_SendAT("AT+CREG?\r\n", ",1", AIR790E_TIMEOUT_SHORT) == AIR790E_OK ||
            Air790E_SendAT("AT+CREG?\r\n", ",5", AIR790E_TIMEOUT_SHORT) == AIR790E_OK)
        {
            printf("[Air790E] Network registered\r\n");
            break;
        }
        Delay_ms(2000);
    }
    if (retry >= 15)
    {
        printf("[Air790E] Network registration FAILED!\r\n");
        return AIR790E_NO_NETWORK;
    }

    // 5. 查询信号质量
    Air790E_SendAT("AT+CSQ\r\n", "OK", AIR790E_TIMEOUT_SHORT);
    printf("[Air790E] Signal: %s\r\n", air790e_rx_buf);
    Delay_ms(100);

    // 6. 激活 PDP 上下文 (CGATT 附着网络)
    Air790E_SendAT("AT+CGATT=1\r\n", "OK", AIR790E_TIMEOUT_LONG);
    Delay_ms(500);

    printf("[Air790E] Module init complete!\r\n");
    return AIR790E_OK;
}

/**
  * @brief  Air790E DTU 透传模式初始化
  * @retval Air790E_Status
  */
Air790E_Status Air790E_DTU_Init(void)
{
    char cmd_buf[100];

    printf("[Air790E] DTU initializing...\r\n");

    // 1. 基础模块初始化 (AT握手, SIM卡, 注网等)
    if (Air790E_ModuleInit() != AIR790E_OK)
    {
        return AIR790E_ERROR;
    }

    // 2. 设置为透传模式 (0:非透传, 1:透传)
    if (Air790E_SendAT("AT+CIPMODE=1\r\n", "OK", AIR790E_TIMEOUT_SHORT) != AIR790E_OK)
    {
        printf("[Air790E] Set CIPMODE=1 failed\r\n");
        return AIR790E_ERROR;
    }
    Delay_ms(100);

    // 3. 连接 TCP 服务器
    sprintf(cmd_buf, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", DTU_SERVER_IP, DTU_SERVER_PORT);
    if (Air790E_SendAT(cmd_buf, "CONNECT", AIR790E_TIMEOUT_LONG) != AIR790E_OK)
    {
        printf("[Air790E] TCP connect failed\r\n");
        return AIR790E_ERROR;
    }
    Delay_ms(500);

    // 4. 进入透传状态
    Air790E_ClearRxBuf();
    Air790E_SendString("AT+CIPSEND\r\n");
    Delay_ms(200);

    printf("[Air790E] DTU Mode Ready!\r\n");
    return AIR790E_OK;
}

/**
  * @brief  DTU 模式发送数据
  * @param  data: 要发送的字符串数据
  */
void Air790E_DTU_Send(char *data)
{
    Air790E_SendString(data);
}

/**
  * @brief  USART2 中断服务函数 - Air790E 数据接收
  * @note   按字节接收存入缓冲区，收到换行符时标记帧完成
  */
void USART2_IRQHandler(void)
{
    uint8_t res;

    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        res = USART_ReceiveData(USART2);

        // 防止缓冲区溢出
        if (air790e_rx_len < AIR790E_RX_BUF_SIZE - 1)
        {
            air790e_rx_buf[air790e_rx_len++] = res;
            air790e_rx_buf[air790e_rx_len] = '\0'; // 保持字符串结尾
        }
    }
}
