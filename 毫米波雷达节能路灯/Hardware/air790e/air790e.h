#ifndef __AIR790E_H
#define __AIR790E_H

#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>

/*
 * Air790E 4G Cat.1 通信模块驱动
 * 通信方式：USART2 (PA2-TX, PA3-RX)
 * 波特率：115200
 * 通信协议：AT 指令集
 * 
 * 功能：通过 4G 网络将路灯状态数据上传至云平台（MQTT/HTTP）
 */

// 接收缓冲区大小
#define AIR790E_RX_BUF_SIZE   256

// AT 命令响应超时时间 (ms)
#define AIR790E_TIMEOUT_SHORT  2000
#define AIR790E_TIMEOUT_LONG   10000

// MQTT 服务器配置 (根据实际云平台修改)
#define MQTT_SERVER_IP     "broker.emqx.io"   // MQTT 服务器地址
#define MQTT_SERVER_PORT   "1883"             // MQTT 端口
#define MQTT_CLIENT_ID     "streetlight_01"   // 客户端ID
#define MQTT_USERNAME      ""                 // 用户名 (可为空)
#define MQTT_PASSWORD      ""                 // 密码 (可为空)
#define MQTT_PUB_TOPIC     "streetlight/data" // 数据发布主题
#define MQTT_SUB_TOPIC     "streetlight/cmd"  // 命令订阅主题

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
Air790E_Status Air790E_MQTT_Connect(void);                     // MQTT 连接
Air790E_Status Air790E_MQTT_Publish(char *topic, char *data);  // MQTT 发布消息
Air790E_Status Air790E_MQTT_Subscribe(char *topic);            // MQTT 订阅主题
void Air790E_ClearRxBuf(void);                                 // 清空接收缓冲区

#endif
