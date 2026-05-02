#ifndef __ZIGBEE_H
#define __ZIGBEE_H

#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>

/*
 * ZigBee CC2530 通信模块驱动
 * 通信方式：USART3 (PB10-TX, PB11-RX)
 * 波特率：115200
 * 
 * 功能：STM32 通过串口与 CC2530 协调器通信，
 *       实现多路灯节点数据的 ZigBee 组网上传
 */

// ZigBee 接收缓冲区大小
#define ZIGBEE_RX_BUF_SIZE  128

// ZigBee 数据帧定义 (自定义协议)
// 帧头(2B) + 节点ID(1B) + 数据类型(1B) + 数据长度(1B) + 数据(NB) + 校验(1B) + 帧尾(2B)
#define ZIGBEE_FRAME_HEAD1   0xAA
#define ZIGBEE_FRAME_HEAD2   0x55
#define ZIGBEE_FRAME_TAIL1   0x0D
#define ZIGBEE_FRAME_TAIL2   0x0A

// 数据类型定义
#define ZIGBEE_TYPE_SENSOR   0x01   // 传感器数据上报
#define ZIGBEE_TYPE_CONTROL  0x02   // 控制命令下发
#define ZIGBEE_TYPE_STATUS   0x03   // 状态查询/回复

// 路灯节点上报数据结构
typedef struct {
    uint8_t  node_id;              // 节点编号 (1~255)
    float    lux;                  // 光照强度 (Lux)
    float    temperature;          // 温度 (°C)
    float    humidity;             // 湿度 (%RH)
    uint8_t  motion_detected;     // 是否检测到人 (0/1)
    uint8_t  light_status;        // 灯状态: 0=OFF, 1=IDLE, 2=ON
    uint8_t  brightness;          // 亮度百分比 (0~100)
} ZigBee_NodeData;

// 全局变量声明
extern uint8_t  zigbee_rx_buf[ZIGBEE_RX_BUF_SIZE];
extern uint8_t  zigbee_rx_len;
extern uint8_t  zigbee_rx_flag;    // 收到完整帧标志

// 函数声明
void ZigBee_Init(void);                              // 初始化 USART3 用于 ZigBee 通信
void ZigBee_SendByte(uint8_t byte);                  // 发送一个字节
void ZigBee_SendString(char *str);                   // 发送字符串
void ZigBee_SendData(uint8_t *data, uint8_t len);    // 发送数据包
void ZigBee_SendNodeData(ZigBee_NodeData *node);     // 发送节点传感器数据帧
uint8_t ZigBee_CheckCommand(void);                   // 检查是否收到控制命令，返回命令码

#endif
