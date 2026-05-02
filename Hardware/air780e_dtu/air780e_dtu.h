#ifndef __AIR780E_DTU_H
#define __AIR780E_DTU_H

#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>

/*
 * Core-Y100P DTU (Air780E 银尔达 DTU 透传固件) 驱动
 * 通信方式：USART2 (PA2-TX, PA3-RX)
 * 波特率：115200
 *
 * DTU 固件特性：
 *   - 使用 config,set/get 命令语法 (非标准 AT+)
 *   - 上电自动联网并进入透传模式，无需手动建链
 *   - 服务器配置存入 NVRAM，一次配置持久生效
 */

// 接收缓冲区大小
#define AIR780E_RX_BUF_SIZE   512

// 命令响应超时时间 (ms)
#define AIR780E_TIMEOUT_SHORT     3000
#define AIR780E_TIMEOUT_LONG      15000
#define AIR780E_TIMEOUT_REBOOT    30000   // 模块保存重启需要约 25 秒

// TCP 服务器配置 (DTU 模式)
#define DTU_SERVER_IP     "112.124.45.212"
#define DTU_SERVER_PORT   "8080"

// 模块状态枚举
typedef enum {
    AIR780E_OK = 0,
    AIR780E_ERROR,
    AIR780E_TIMEOUT,
    AIR780E_NO_NETWORK
} Air780E_Status;

// 全局变量声明
extern uint8_t  air780e_rx_buf[AIR780E_RX_BUF_SIZE];
extern uint16_t air780e_rx_len;
extern uint8_t  air780e_rx_flag;

// 函数声明
void Air780E_Init(void);
Air780E_Status Air780E_SendCmd(char *cmd, char *expect, uint16_t timeout_ms);
Air780E_Status Air780E_ExitTransparent(void);
Air780E_Status Air780E_ModuleInit(void);
Air780E_Status Air780E_DTU_Init(void);
void Air780E_DTU_Send(char *data);
void Air780E_ClearRxBuf(void);

#endif
