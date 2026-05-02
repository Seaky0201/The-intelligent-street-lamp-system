#include "air780e_dtu.h"
#include "Delay.h"

/*
 * Core-Y100P DTU (Air780E 银尔达 DTU 透传固件) 驱动
 * 通信方式：USART2 (PA2-TX, PA3-RX)
 * 波特率：115200
 *
 * DTU 固件命令格式 (银尔达)：
 *   设置：config,set,<命令>,<参数>...\r\n  → 应答 config,set,ok\r\n
 *   查询：config,get,<命令>,<参数>...\r\n  → 应答 config,get,ok,<值>\r\n
 *   错误：config,<op>,error,<错误码>\r\n
 */

// 全局接收缓冲区
uint8_t  air780e_rx_buf[AIR780E_RX_BUF_SIZE];
uint16_t air780e_rx_len = 0;
uint8_t  air780e_rx_flag = 0;

/**
 * @brief  初始化 USART2 用于 Air780E DTU 通信
 * @note   PA2-TX, PA3-RX, 波特率 115200
 */
void Air780E_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

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
static void Air780E_SendByte(uint8_t byte)
{
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, byte);
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
}

/**
 * @brief  通过 USART2 发送字符串
 */
static void Air780E_SendString(char *str)
{
    while (*str)
    {
        Air780E_SendByte(*str++);
    }
}

/**
 * @brief  清空接收缓冲区
 */
void Air780E_ClearRxBuf(void)
{
    memset(air780e_rx_buf, 0, AIR780E_RX_BUF_SIZE);
    air780e_rx_len = 0;
    air780e_rx_flag = 0;
}

/**
 * @brief  在接收缓冲区中查找目标字符串
 * @param  str: 要查找的字符串
 * @retval 1=找到, 0=未找到
 */
static uint8_t Air780E_FindString(char *str)
{
    if (strstr((const char *)air780e_rx_buf, str) != NULL)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  发送 DTU config 命令并等待期望响应
 * @param  cmd:        命令字符串 (如 "config,set,uart,115200,8,0,1,80\r\n")
 * @param  expect:     期望的响应字符串 (如 ",ok")
 * @param  timeout_ms: 超时时间 (毫秒)
 * @retval Air780E_Status
 */
Air780E_Status Air780E_SendCmd(char *cmd, char *expect, uint16_t timeout_ms)
{
    uint16_t wait = 0;

    Air780E_ClearRxBuf();
    Air780E_SendString(cmd);

    while (wait < timeout_ms)
    {
        Delay_ms(10);
        wait += 10;

        if (air780e_rx_len > 0)
        {
            if (Air780E_FindString(expect))
            {
                return AIR780E_OK;
            }
            if (Air780E_FindString("error"))
            {
                return AIR780E_ERROR;
            }
        }
    }

    return AIR780E_TIMEOUT;
}

/**
 * @brief  退出透传模式，进入 AT 命令模式
 * @note   发送 "+++"，前后各静默 1 秒
 * @retval Air780E_Status
 */
Air780E_Status Air780E_ExitTransparent(void)
{
    uint8_t retry;

    for (retry = 0; retry < 3; retry++)
    {
        // 静默 1 秒（满足 guard time）
        Delay_ms(1100);

        Air780E_ClearRxBuf();
        // +++ 不带 \r\n
        Air780E_SendString("+++");

        // 静默 1 秒等待模块响应
        Delay_ms(1100);

        // 检查是否收到 OK 或已进入命令模式
        if (Air780E_FindString("OK") || Air780E_FindString("ok"))
        {
            Delay_ms(200);
            return AIR780E_OK;
        }

        // 发送 version 查询确认是否已在命令模式
        Air780E_ClearRxBuf();
        Air780E_SendString("config,get,version\r\n");
        Delay_ms(500);

        if (Air780E_FindString(",ok"))
        {
            return AIR780E_OK;
        }
    }

    return AIR780E_TIMEOUT;
}

/**
 * @brief  Air780E DTU 模块初始化
 * @note   退出透传 -> 确认在线 -> 查询信号
 * @retval Air780E_Status
 */
Air780E_Status Air780E_ModuleInit(void)
{
    printf("[Air780E DTU] Module initializing...\r\n");

    // 1. 退出透传，进入 AT 命令模式
    if (Air780E_ExitTransparent() != AIR780E_OK)
    {
        printf("[Air780E DTU] Exit transparent mode FAILED!\r\n");
        return AIR780E_ERROR;
    }
    printf("[Air780E DTU] Command mode OK\r\n");

    // 2. 查询固件版本，确认模块在线
    if (Air780E_SendCmd("config,get,version\r\n", ",ok", AIR780E_TIMEOUT_SHORT) != AIR780E_OK)
    {
        printf("[Air780E DTU] Module not responding!\r\n");
        return AIR780E_ERROR;
    }
    printf("[Air780E DTU] Version: %s\r\n", air780e_rx_buf);

    // 3. 查询 IMEI
    Air780E_SendCmd("config,get,imei\r\n", ",ok", AIR780E_TIMEOUT_SHORT);
    printf("[Air780E DTU] IMEI: %s\r\n", air780e_rx_buf);

    // 4. 查询信号强度
    Air780E_SendCmd("config,get,csq\r\n", ",ok", AIR780E_TIMEOUT_SHORT);
    printf("[Air780E DTU] CSQ: %s\r\n", air780e_rx_buf);

    printf("[Air780E DTU] Module init complete!\r\n");
    return AIR780E_OK;
}

/**
 * @brief  等待模块重启完成
 * @note   config,set,save 后模块自动重启，约需 25 秒
 *         通过轮询 config,get,version 检测模块是否就绪
 */
static Air780E_Status Air780E_WaitForReboot(void)
{
    uint16_t wait = 0;

    printf("[Air780E DTU] Waiting for reboot...\r\n");

    while (wait < AIR780E_TIMEOUT_REBOOT)
    {
        Delay_ms(1000);
        wait += 1000;

        // 尝试退出透传并发送 version 查询
        Air780E_ClearRxBuf();
        Air780E_SendString("+++");
        Delay_ms(1100);

        Air780E_ClearRxBuf();
        Air780E_SendString("config,get,version\r\n");
        Delay_ms(500);

        if (Air780E_FindString(",ok"))
        {
            printf("[Air780E DTU] Reboot complete (%d s)\r\n", wait / 1000);
            return AIR780E_OK;
        }
    }

    printf("[Air780E DTU] Reboot timeout!\r\n");
    return AIR780E_TIMEOUT;
}

/**
 * @brief  Air780E DTU 透传模式初始化
 * @note   配置串口 -> 配置 TCP 通道 -> 保存重启 -> 验证连接
 * @retval Air780E_Status
 */
Air780E_Status Air780E_DTU_Init(void)
{
    char cmd_buf[120];
    uint8_t retry;

    printf("[Air780E DTU] DTU initializing...\r\n");

    // 1. 退出透传，进入 AT 命令模式
    if (Air780E_ExitTransparent() != AIR780E_OK)
    {
        printf("[Air780E DTU] Cannot enter command mode\r\n");
        return AIR780E_ERROR;
    }

    // 2. 查询当前网络状态，如果已连接则无需重新配置
    Air780E_SendCmd("config,get,netstatus,1\r\n", ",ok", AIR780E_TIMEOUT_SHORT);

    if (Air780E_FindString(",ok,1") || Air780E_FindString("ok,1"))
    {
        printf("[Air780E DTU] Already connected, skip config\r\n");
        // 发送 ATO 重新进入透传
        Air780E_SendString("ATO\r\n");
        Delay_ms(200);
        return AIR780E_OK;
    }

    // 3. 配置本地串口参数
    printf("[Air780E DTU] Configuring UART...\r\n");
    if (Air780E_SendCmd("config,set,uart,115200,8,0,1,80\r\n", ",ok", AIR780E_TIMEOUT_SHORT) != AIR780E_OK)
    {
        printf("[Air780E DTU] UART config failed!\r\n");
    }
    Delay_ms(200);

    // 4. 配置 TCP 通道 1
    printf("[Air780E DTU] Configuring TCP channel...\r\n");
    sprintf(cmd_buf, "config,set,tcp,1,PING,60,%s,%s,0,,0,0\r\n",
            DTU_SERVER_IP, DTU_SERVER_PORT);

    if (Air780E_SendCmd(cmd_buf, ",ok", AIR780E_TIMEOUT_SHORT) != AIR780E_OK)
    {
        printf("[Air780E DTU] TCP config failed!\r\n");
        return AIR780E_ERROR;
    }
    Delay_ms(200);

    // 5. 保存配置到 NVRAM（模块自动重启）
    printf("[Air780E DTU] Saving config and rebooting...\r\n");
    Air780E_SendCmd("config,set,save\r\n", ",ok", AIR780E_TIMEOUT_SHORT);
    Delay_ms(500);

    // 6. 等待模块重启完成
    if (Air780E_WaitForReboot() != AIR780E_OK)
    {
        return AIR780E_TIMEOUT;
    }

    // 7. 查询网络连接状态（最多重试 10 次）
    for (retry = 0; retry < 10; retry++)
    {
        Air780E_SendCmd("config,get,netstatus,1\r\n", ",ok", AIR780E_TIMEOUT_SHORT);

        if (Air780E_FindString(",ok,1") || Air780E_FindString("ok,1"))
        {
            printf("[Air780E DTU] TCP connected!\r\n");
            // 发送 ATO 重新进入透传模式
            Air780E_SendString("ATO\r\n");
            Delay_ms(200);
            printf("[Air780E DTU] DTU Mode Ready!\r\n");
            return AIR780E_OK;
        }

        printf("[Air780E DTU] Waiting for network... (%d/10)\r\n", retry + 1);
        Delay_ms(3000);
    }

    printf("[Air780E DTU] Network connect FAILED!\r\n");
    return AIR780E_NO_NETWORK;
}

/**
 * @brief  DTU 模式发送数据
 * @param  data: 要发送的字符串数据
 * @note   透传模式下数据直接发送到服务器
 */
void Air780E_DTU_Send(char *data)
{
    Air780E_SendString(data);
}

/**
 * @brief  USART2 中断服务函数 - Air780E DTU 数据接收
 * @note   按字节接收存入缓冲区，保持字符串结尾
 */
void USART2_IRQHandler(void)
{
    uint8_t res;

    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        res = USART_ReceiveData(USART2);

        if (air780e_rx_len < AIR780E_RX_BUF_SIZE - 1)
        {
            air780e_rx_buf[air780e_rx_len++] = res;
            air780e_rx_buf[air780e_rx_len] = '\0';
        }
    }
}
