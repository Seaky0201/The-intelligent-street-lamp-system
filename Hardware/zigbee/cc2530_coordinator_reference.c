/*
 * ============================================================================
 * CC2530 ZigBee 协调器端参考代码
 * ============================================================================
 * 
 * 功能说明：
 *   CC2530 协调器通过 ZigBee 无线网络接收各路灯终端节点上报的传感器数据，
 *   然后通过串口将数据转发给连接的 STM32 (或直接连接电脑)。
 *   同时，协调器也可以接收串口下发的控制命令，通过 ZigBee 发送给指定节点。
 * 
 * 硬件连接：
 *   CC2530 P0.2 (UART0_TX) --> STM32 或 电脑 USB 串口 RX
 *   CC2530 P0.3 (UART0_RX) <-- STM32 或 电脑 USB 串口 TX
 *   波特率：115200
 * 
 * ZigBee 网络角色：协调器 (Coordinator)
 * 
 * 自定义协议帧格式 (与 STM32 端 zigbee.h 一致):
 *   | AA 55 | NodeID | Type | Len | Data... | Checksum | 0D 0A |
 *   |  帧头  | 节点号 | 类型 | 长度 |  数据   |   校验   |  帧尾  |
 * 
 * 注意：此代码为参考框架，需要在 IAR for 8051 或 Z-Stack 开发环境中编译。
 *       实际使用时需要结合 TI Z-Stack 协议栈进行开发。
 * 
 * ============================================================================
 */


#if 0  // 整个文件为参考代码，不参与 STM32 Keil 工程编译

/* ======================== 基于 Z-Stack 的协调器应用代码 ======================== */

/*
 * 以下代码展示在 Z-Stack 框架下，协调器应用层的核心实现。
 * 文件位置通常为：Z-Stack/Projects/zstack/Samples/StreetLight/Source/
 */

// ============================================================================
// 文件: StreetLight_Coordinator.h
// ============================================================================

#ifndef STREETLIGHT_COORDINATOR_H
#define STREETLIGHT_COORDINATOR_H

#include "ZComDef.h"
#include "AF.h"

// 应用层端点号和簇ID定义
#define STREETLIGHT_ENDPOINT        20
#define STREETLIGHT_PROFID          0x0F08
#define STREETLIGHT_DEVICEID        0x0001
#define STREETLIGHT_DEVICE_VERSION  0
#define STREETLIGHT_FLAGS           0

// 簇 ID 定义
#define STREETLIGHT_CLUSTERID_SENSOR   0x0001  // 传感器数据上报
#define STREETLIGHT_CLUSTERID_CONTROL  0x0002  // 控制命令下发
#define STREETLIGHT_CLUSTERID_STATUS   0x0003  // 状态查询

// 最大支持节点数
#define MAX_NODES   16

// 节点数据结构 (与 STM32 端一致)
typedef struct {
    uint8  node_id;
    float  lux;
    float  temperature;
    float  humidity;
    uint8  motion_detected;
    uint8  light_status;
    uint8  brightness;
    uint16 short_addr;      // ZigBee 短地址
    uint8  online;          // 在线状态
} NodeInfo_t;

// 函数声明
extern void StreetLight_Coord_Init(uint8 task_id);
extern uint16 StreetLight_Coord_ProcessEvent(uint8 task_id, uint16 events);
extern void StreetLight_Coord_MessageCB(afIncomingMSGPacket_t *pkt);
extern void StreetLight_Coord_SendControl(uint16 dest_addr, uint8 cmd, uint8 param);

#endif


// ============================================================================
// 文件: StreetLight_Coordinator.c
// ============================================================================

/*
 * 注意：以下代码需要 Z-Stack 头文件支持，此处仅作参考框架。
 * 实际编译需在 IAR Embedded Workbench for 8051 中配合 Z-Stack 工程。
 */

// ---- 以下为协调器应用层 .c 文件实现 ----

#include "OSAL.h"
#include "ZGlobals.h"
#include "AF.h"
#include "aps_groups.h"
#include "ZDApp.h"
#include "StreetLight_Coordinator.h"
#include "hal_uart.h"
#include "hal_led.h"
#include "hal_key.h"

// ---- 全局变量 ----
static uint8 StreetLight_TaskID;
static NodeInfo_t nodeTable[MAX_NODES];  // 节点信息表
static uint8 nodeCount = 0;

// AF 端点描述符
static const cId_t StreetLight_ClusterList[] = {
    STREETLIGHT_CLUSTERID_SENSOR,
    STREETLIGHT_CLUSTERID_CONTROL,
    STREETLIGHT_CLUSTERID_STATUS
};

static const SimpleDescriptionFormat_t StreetLight_SimpleDesc = {
    STREETLIGHT_ENDPOINT,
    STREETLIGHT_PROFID,
    STREETLIGHT_DEVICEID,
    STREETLIGHT_DEVICE_VERSION,
    STREETLIGHT_FLAGS,
    sizeof(StreetLight_ClusterList) / sizeof(cId_t),
    (cId_t *)StreetLight_ClusterList,
    sizeof(StreetLight_ClusterList) / sizeof(cId_t),
    (cId_t *)StreetLight_ClusterList
};

static endPointDesc_t StreetLight_epDesc;

// ---- 串口配置 ----
static void UART_Init(void)
{
    halUARTCfg_t uartConfig;
    
    uartConfig.configured    = TRUE;
    uartConfig.baudRate      = HAL_UART_BR_115200;
    uartConfig.flowControl   = FALSE;
    uartConfig.flowControlThreshold = 64;
    uartConfig.rx.maxBufSize = 128;
    uartConfig.tx.maxBufSize = 128;
    uartConfig.idleTimeout   = 6;
    uartConfig.intEnable     = TRUE;
    uartConfig.callBackFunc  = UART_RxCallback;
    
    HalUARTOpen(HAL_UART_PORT_0, &uartConfig);
}

// ---- 串口接收回调 (接收电脑/STM32下发的控制命令) ----
static void UART_RxCallback(uint8 port, uint8 event)
{
    uint8 buf[64];
    uint16 len;
    
    len = HalUARTRead(HAL_UART_PORT_0, buf, sizeof(buf));
    if (len > 0)
    {
        // 解析串口收到的控制帧
        // 格式：AA 55 NodeID 02 Len CmdCode [Param] Checksum 0D 0A
        if (buf[0] == 0xAA && buf[1] == 0x55 && buf[3] == 0x02)
        {
            uint8 target_node = buf[2];
            uint8 cmd_code = buf[5];
            uint8 param = (buf[4] > 1) ? buf[6] : 0;
            
            // 在节点表中查找目标节点的 ZigBee 短地址
            uint8 i;
            for (i = 0; i < nodeCount; i++)
            {
                if (nodeTable[i].node_id == target_node && nodeTable[i].online)
                {
                    // 通过 ZigBee 无线发送控制命令给目标节点
                    StreetLight_Coord_SendControl(nodeTable[i].short_addr, cmd_code, param);
                    break;
                }
            }
        }
    }
}

// ---- 串口发送数据帧 (将节点数据转发给电脑/STM32) ----
static void UART_SendNodeData(NodeInfo_t *node)
{
    uint8 frame[22];
    uint8 idx = 0;
    uint8 *p;
    uint8 i, checksum = 0;
    
    // 帧头
    frame[idx++] = 0xAA;
    frame[idx++] = 0x55;
    
    // 节点ID
    frame[idx++] = node->node_id;
    
    // 类型：传感器数据
    frame[idx++] = 0x01;
    
    // 数据长度
    frame[idx++] = 14;
    
    // 光照 float
    p = (uint8 *)&(node->lux);
    frame[idx++] = p[0]; frame[idx++] = p[1];
    frame[idx++] = p[2]; frame[idx++] = p[3];
    
    // 温度 float
    p = (uint8 *)&(node->temperature);
    frame[idx++] = p[0]; frame[idx++] = p[1];
    frame[idx++] = p[2]; frame[idx++] = p[3];
    
    // 湿度 float
    p = (uint8 *)&(node->humidity);
    frame[idx++] = p[0]; frame[idx++] = p[1];
    frame[idx++] = p[2]; frame[idx++] = p[3];
    
    // 运动检测
    frame[idx++] = node->motion_detected;
    
    // 亮度
    frame[idx++] = node->brightness;
    
    // 校验和
    for (i = 2; i < idx; i++) checksum ^= frame[i];
    frame[idx++] = checksum;
    
    // 帧尾
    frame[idx++] = 0x0D;
    frame[idx++] = 0x0A;
    
    HalUARTWrite(HAL_UART_PORT_0, frame, idx);
}

// ---- 应用层初始化 ----
void StreetLight_Coord_Init(uint8 task_id)
{
    StreetLight_TaskID = task_id;
    
    // 注册 AF 端点
    StreetLight_epDesc.endPoint = STREETLIGHT_ENDPOINT;
    StreetLight_epDesc.task_id = &StreetLight_TaskID;
    StreetLight_epDesc.simpleDesc = (SimpleDescriptionFormat_t *)&StreetLight_SimpleDesc;
    StreetLight_epDesc.latencyReq = noLatencyReqs;
    afRegister(&StreetLight_epDesc);
    
    // 初始化串口
    UART_Init();
    
    // 初始化节点表
    osal_memset(nodeTable, 0, sizeof(nodeTable));
    nodeCount = 0;
    
    // LED 指示灯
    HalLedSet(HAL_LED_1, HAL_LED_MODE_ON);  // 表示协调器已启动
}

// ---- OSAL 事件处理 ----
uint16 StreetLight_Coord_ProcessEvent(uint8 task_id, uint16 events)
{
    afIncomingMSGPacket_t *MSGpkt;
    
    if (events & SYS_EVENT_MSG)
    {
        MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive(StreetLight_TaskID);
        while (MSGpkt)
        {
            switch (MSGpkt->hdr.event)
            {
                case AF_INCOMING_MSG_CMD:
                    // 收到 ZigBee 无线数据
                    StreetLight_Coord_MessageCB(MSGpkt);
                    break;
                    
                case ZDO_STATE_CHANGE:
                    // 网络状态变化
                    if ((devStates_t)(MSGpkt->hdr.status) == DEV_ZB_COORD)
                    {
                        // 协调器网络建立成功
                        HalLedSet(HAL_LED_2, HAL_LED_MODE_ON);
                    }
                    break;
                    
                default:
                    break;
            }
            
            osal_msg_deallocate((uint8 *)MSGpkt);
            MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive(StreetLight_TaskID);
        }
        
        return (events ^ SYS_EVENT_MSG);
    }
    
    return 0;
}

// ---- 处理来自终端节点的无线消息 ----
void StreetLight_Coord_MessageCB(afIncomingMSGPacket_t *pkt)
{
    uint8 *data = pkt->cmd.Data;
    uint16 len = pkt->cmd.DataLength;
    
    switch (pkt->clusterId)
    {
        case STREETLIGHT_CLUSTERID_SENSOR:
        {
            // 收到终端节点上报的传感器数据 (14字节)
            if (len >= 14)
            {
                NodeInfo_t tempNode;
                uint8 *p;
                uint8 i, found = 0;
                
                tempNode.node_id = data[0];
                
                // 解析 float 数据
                p = (uint8 *)&(tempNode.lux);
                p[0] = data[1]; p[1] = data[2]; p[2] = data[3]; p[3] = data[4];
                
                p = (uint8 *)&(tempNode.temperature);
                p[0] = data[5]; p[1] = data[6]; p[2] = data[7]; p[3] = data[8];
                
                p = (uint8 *)&(tempNode.humidity);
                p[0] = data[9]; p[1] = data[10]; p[2] = data[11]; p[3] = data[12];
                
                tempNode.motion_detected = data[13];
                tempNode.brightness = (len > 14) ? data[14] : 0;
                tempNode.short_addr = pkt->srcAddr.addr.shortAddr;
                tempNode.online = 1;
                
                // 更新节点表
                for (i = 0; i < nodeCount; i++)
                {
                    if (nodeTable[i].node_id == tempNode.node_id)
                    {
                        nodeTable[i] = tempNode;
                        found = 1;
                        break;
                    }
                }
                if (!found && nodeCount < MAX_NODES)
                {
                    nodeTable[nodeCount++] = tempNode;
                }
                
                // 通过串口转发给电脑/STM32
                UART_SendNodeData(&tempNode);
                
                // LED 闪烁表示收到数据
                HalLedSet(HAL_LED_1, HAL_LED_MODE_TOGGLE);
            }
            break;
        }
        
        case STREETLIGHT_CLUSTERID_STATUS:
        {
            // 收到节点状态回复
            // 同样通过串口转发
            HalUARTWrite(HAL_UART_PORT_0, data, len);
            break;
        }
        
        default:
            break;
    }
}

// ---- 向指定终端节点发送控制命令 ----
void StreetLight_Coord_SendControl(uint16 dest_addr, uint8 cmd, uint8 param)
{
    uint8 buf[3];
    afAddrType_t dstAddr;
    
    buf[0] = cmd;    // 命令码
    buf[1] = param;  // 参数
    buf[2] = cmd ^ param; // 简单校验
    
    dstAddr.addrMode = (afAddrMode_t)Addr16Bit;
    dstAddr.addr.shortAddr = dest_addr;
    dstAddr.endPoint = STREETLIGHT_ENDPOINT;
    
    AF_DataRequest(&dstAddr, 
                   &StreetLight_epDesc,
                   STREETLIGHT_CLUSTERID_CONTROL,
                   3,    // 数据长度
                   buf,
                   NULL, // TransID
                   AF_DISCV_ROUTE,
                   AF_DEFAULT_RADIUS);
}


// ============================================================================
// CC2530 终端节点 (End Device) 参考代码
// ============================================================================
// 
// 终端节点的 CC2530 连接在路灯节点的 STM32 旁边，
// 通过串口接收 STM32 发来的传感器数据帧，再通过 ZigBee 无线发送给协调器。
// 
// 核心逻辑：
// 1. 串口中断接收 STM32 的数据帧 (AA 55 ... 0D 0A)
// 2. 解析帧中的传感器数据
// 3. 通过 AF_DataRequest() 无线发送给协调器
// 4. 接收协调器下发的控制命令
// 5. 通过串口转发控制命令给 STM32
//
// 终端节点的代码结构与协调器类似，
// 主要区别：
//   - 协调器用 AF_DataRequest 发送给所有终端
//   - 终端节点用 AF_DataRequest 发送给协调器 (目标地址 0x0000)
//   - 终端节点的串口回调接收 STM32 数据并无线转发
//   - 终端节点的 MessageCB 接收协调器命令并串口转发
// ============================================================================
#endif // #if 0
