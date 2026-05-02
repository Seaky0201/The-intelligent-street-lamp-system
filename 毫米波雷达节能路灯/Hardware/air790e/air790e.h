#ifndef __AIR790E_H
#define __AIR790E_H

#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>

/*
 * Air790E 4G Cat.1 通信模块驱动
 * 通信方式：USART2 (PA2-TX, PA3-RX)
 * 波特率：115200
 * 通信协议：TCP DTU 透传模式
 *
 * 功能：通过 4G 网络将路灯状态数据透传至云平台
 */

// 接收缓冲区大小
#define AIR790E_RX_BUF_SIZE   256

// AT 命令响应超时时间 (ms)
#define AIR790E_TIMEOUT_SHORT  2000
#define AIR790E_TIMEOUT_LONG   10000

// TCP 服务器配置 (DTU 模式)
#define DTU_SERVER_IP     "112.124.45.212"   // 请修改为你的服务器公网IP
#define DTU_SERVER_PORT   "8080"             // 请修改为你的服务器端口

// 模块状态枚举
typedef enum {
    AIR790E_OK = 0,       // 成功
    AIR790E_ERROR,        // 错误
    AIR790E_TIMEOUT,      // 超时
    AIR790E_NO_NETWORK    // 无网络
} Air790E_Status;

// 全局变量声明
extern uint8_t  air790e_rx_buf[AIR790E_RX_BUF_SIZE];
extern uint16_t air790e_rx_len;
extern uint8_t  air790e_rx_flag;

// 函数声明
void Air790E_Init(void);                                       // 初始化 USART2
Air790E_Status Air790E_SendAT(char *cmd, char *expect, uint16_t timeout_ms); // 发送AT命令并等待响应
Air790E_Status Air790E_ModuleInit(void);                       // 模块初始化（检测AT、SIM卡、注网）
Air790E_Status Air790E_DTU_Init(void);                         // DTU 透传模式初始化
void Air790E_DTU_Send(char *data);                             // DTU 透传发送数据
void Air790E_ClearRxBuf(void);                                 // 清空接收缓冲区

#endif
