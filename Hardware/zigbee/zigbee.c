#include "zigbee.h"

/*
 * ZigBee CC2530 通信模块驱动
 * 通信方式：USART3 (PB10-TX, PB11-RX)
 * 波特率：115200
 * 
 * STM32 路灯节点通过 USART3 与本地 CC2530 终端模块通信，
 * CC2530 终端模块再通过 ZigBee 无线网络将数据发送给协调器。
 * 
 * 自定义协议帧格式：
 * | AA 55 | NodeID | Type | Len | Data... | Checksum | 0D 0A |
 * |  帧头  | 节点号 | 类型 | 长度|  数据   |   校验   |  帧尾  |
 */

// 全局接收缓冲区
uint8_t  zigbee_rx_buf[ZIGBEE_RX_BUF_SIZE];
uint8_t  zigbee_rx_len = 0;
uint8_t  zigbee_rx_flag = 0;

// 内部接收状态机变量
static uint8_t rx_state = 0;
static uint8_t rx_index = 0;
static uint8_t rx_data_len = 0;

/**
  * @brief  初始化 USART3 用于 ZigBee CC2530 通信
  * @note   PB10-TX, PB11-RX, 波特率 115200
  */
void ZigBee_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 开启时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // PB10 - USART3_TX 复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // PB11 - USART3_RX 浮空输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // USART3 参数配置
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    // 开启接收中断
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    // NVIC 中断优先级配置
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART3, ENABLE);
}

/**
  * @brief  通过 USART3 发送一个字节
  * @param  byte: 要发送的字节
  */
void ZigBee_SendByte(uint8_t byte)
{
    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
    USART_SendData(USART3, byte);
    while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
}

/**
  * @brief  通过 USART3 发送字符串
  * @param  str: 要发送的字符串
  */
void ZigBee_SendString(char *str)
{
    while (*str)
    {
        ZigBee_SendByte(*str++);
    }
}

/**
  * @brief  通过 USART3 发送数据包 (原始字节流)
  * @param  data: 数据指针
  * @param  len:  数据长度
  */
void ZigBee_SendData(uint8_t *data, uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++)
    {
        ZigBee_SendByte(data[i]);
    }
}

/**
  * @brief  计算校验和 (所有数据字节异或)
  * @param  data: 数据指针
  * @param  len:  数据长度
  * @retval 校验和
  */
static uint8_t CalcChecksum(uint8_t *data, uint8_t len)
{
    uint8_t checksum = 0;
    uint8_t i;
    for (i = 0; i < len; i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}

/**
  * @brief  发送路灯节点传感器数据帧
  * @param  node: 指向节点数据结构体的指针
  * 
  * @note   数据域格式 (共14字节):
  *         [0-3]  光照强度 float (4B)
  *         [4-7]  温度 float (4B)
  *         [8-11] 湿度 float (4B)
  *         [12]   运动检测 uint8_t (1B)
  *         [13]   亮度百分比 uint8_t (1B)
  */
void ZigBee_SendNodeData(ZigBee_NodeData *node)
{
    uint8_t frame[22]; // 帧头2 + 节点1 + 类型1 + 长度1 + 数据14 + 校验1 + 帧尾2 = 22
    uint8_t *p;
    uint8_t idx = 0;
    
    // 帧头
    frame[idx++] = ZIGBEE_FRAME_HEAD1;  // 0xAA
    frame[idx++] = ZIGBEE_FRAME_HEAD2;  // 0x55
    
    // 节点 ID
    frame[idx++] = node->node_id;
    
    // 数据类型：传感器数据上报
    frame[idx++] = ZIGBEE_TYPE_SENSOR;
    
    // 数据长度 = 14 字节
    frame[idx++] = 14;
    
    // 光照强度 (float, 4字节)
    p = (uint8_t *)&(node->lux);
    frame[idx++] = p[0]; frame[idx++] = p[1];
    frame[idx++] = p[2]; frame[idx++] = p[3];
    
    // 温度 (float, 4字节)
    p = (uint8_t *)&(node->temperature);
    frame[idx++] = p[0]; frame[idx++] = p[1];
    frame[idx++] = p[2]; frame[idx++] = p[3];
    
    // 湿度 (float, 4字节)
    p = (uint8_t *)&(node->humidity);
    frame[idx++] = p[0]; frame[idx++] = p[1];
    frame[idx++] = p[2]; frame[idx++] = p[3];
    
    // 运动检测状态
    frame[idx++] = node->motion_detected;
    
    // 亮度百分比
    frame[idx++] = node->brightness;
    
    // 校验和 (从节点ID到数据末尾的所有字节异或)
    frame[idx] = CalcChecksum(&frame[2], idx - 2);
    idx++;
    
    // 帧尾
    frame[idx++] = ZIGBEE_FRAME_TAIL1;  // 0x0D
    frame[idx++] = ZIGBEE_FRAME_TAIL2;  // 0x0A
    
    // 发送帧
    ZigBee_SendData(frame, idx);
}

/**
  * @brief  检查是否收到协调器下发的控制命令
  * @retval 命令码: 0=无命令, 0x01=开灯, 0x02=关灯, 0x03=设置亮度, 0xFF=查询状态
  * 
  * @note   控制帧格式:
  *         AA 55 | NodeID | 0x02 | Len | CmdCode [Param] | Checksum | 0D 0A
  */
uint8_t ZigBee_CheckCommand(void)
{
    uint8_t cmd = 0;
    
    if (zigbee_rx_flag == 1)
    {
        zigbee_rx_flag = 0;
        
        // 验证帧头
        if (zigbee_rx_buf[0] == ZIGBEE_FRAME_HEAD1 && 
            zigbee_rx_buf[1] == ZIGBEE_FRAME_HEAD2)
        {
            // 检查数据类型是否为控制命令
            if (zigbee_rx_buf[3] == ZIGBEE_TYPE_CONTROL)
            {
                // 提取命令码 (数据域第一个字节)
                cmd = zigbee_rx_buf[5];
            }
        }
        
        // 清空接收缓冲区
        zigbee_rx_len = 0;
        memset(zigbee_rx_buf, 0, ZIGBEE_RX_BUF_SIZE);
    }
    
    return cmd;
}

/**
  * @brief  USART3 中断服务函数 - ZigBee 数据接收
  * @note   使用状态机解析自定义协议帧
  * 
  *   状态机：
  *   0: 等待帧头1 (0xAA)
  *   1: 等待帧头2 (0x55)
  *   2: 接收节点ID
  *   3: 接收数据类型
  *   4: 接收数据长度
  *   5: 接收数据域
  *   6: 接收校验和
  *   7: 等待帧尾1 (0x0D)
  *   8: 等待帧尾2 (0x0A)
  */
void USART3_IRQHandler(void)
{
    uint8_t res;
    
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        res = USART_ReceiveData(USART3);
        
        switch (rx_state)
        {
            case 0: // 等待帧头1
                if (res == ZIGBEE_FRAME_HEAD1)
                {
                    rx_index = 0;
                    zigbee_rx_buf[rx_index++] = res;
                    rx_state = 1;
                }
                break;
                
            case 1: // 等待帧头2
                if (res == ZIGBEE_FRAME_HEAD2)
                {
                    zigbee_rx_buf[rx_index++] = res;
                    rx_state = 2;
                }
                else
                {
                    rx_state = 0; // 帧头不匹配，重新等待
                }
                break;
                
            case 2: // 节点ID
                zigbee_rx_buf[rx_index++] = res;
                rx_state = 3;
                break;
                
            case 3: // 数据类型
                zigbee_rx_buf[rx_index++] = res;
                rx_state = 4;
                break;
                
            case 4: // 数据长度
                zigbee_rx_buf[rx_index++] = res;
                rx_data_len = res;
                if (rx_data_len > 0 && rx_data_len < (ZIGBEE_RX_BUF_SIZE - 8))
                {
                    rx_state = 5;
                }
                else if (rx_data_len == 0)
                {
                    rx_state = 6; // 无数据域，直接到校验
                }
                else
                {
                    rx_state = 0; // 长度非法
                }
                break;
                
            case 5: // 接收数据域
                zigbee_rx_buf[rx_index++] = res;
                rx_data_len--;
                if (rx_data_len == 0)
                {
                    rx_state = 6;
                }
                break;
                
            case 6: // 校验和
                zigbee_rx_buf[rx_index++] = res;
                rx_state = 7;
                break;
                
            case 7: // 帧尾1
                if (res == ZIGBEE_FRAME_TAIL1)
                {
                    zigbee_rx_buf[rx_index++] = res;
                    rx_state = 8;
                }
                else
                {
                    rx_state = 0;
                }
                break;
                
            case 8: // 帧尾2
                if (res == ZIGBEE_FRAME_TAIL2)
                {
                    zigbee_rx_buf[rx_index++] = res;
                    zigbee_rx_len = rx_index;
                    zigbee_rx_flag = 1; // 标记收到完整帧
                }
                rx_state = 0; // 无论成功失败，复位状态机
                break;
                
            default:
                rx_state = 0;
                break;
        }
    }
}
