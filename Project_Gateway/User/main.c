#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "zigbee.h"
#include "air780e_dtu.h"
#include "usart_radar.h"

// 用于缓存从节点接收到的最新数据
ZigBee_NodeData latest_node_data;
uint8_t new_data_received = 0;

void System_Gateway_Init(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    USART1_Debug_Init();
    printf("--- Gateway Console v1.0 ---\r\n");
    
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(1, 1, "Gateway Monitor");
    
    ZigBee_Init(); // 初始化USART3，接收节点数据
    Air780E_Init();

    if (Air780E_DTU_Init() == AIR780E_OK) {
        OLED_ShowString(2, 1, "4G DTU: OK   ");
    } else {
        OLED_ShowString(2, 1, "4G Init FAIL");
    }
}

// OLED 显示节点状态
void Gateway_Update_OLED(ZigBee_NodeData *data) {
    OLED_ShowString(3, 1, "Node:");
    OLED_ShowNum(3, 6, data->node_id, 2);
    OLED_ShowString(3, 9, " L:");
    OLED_ShowNum(3, 12, (uint32_t)data->lux, 4);

    OLED_ShowString(4, 1, "M:");
    OLED_ShowNum(4, 3, data->motion_detected, 1);
    OLED_ShowString(4, 5, " B:");
    OLED_ShowNum(4, 8, data->brightness, 3);
}

// 4G DTU 透传上云
void Gateway_Upload_DTU(ZigBee_NodeData *data) {
    char buf[200];
    // 格式化数据字符串 (例如: ID:01,Lux:123.4,Temp:25.0,Motion:1)
    sprintf(buf, "ID:%d,Lux:%.1f,Temp:%.1f,Humi:%.1f,Motion:%d,Bright:%d\n",
        data->node_id, data->lux, data->temperature, data->humidity,
        data->motion_detected, data->brightness);

    Air780E_DTU_Send(buf);
}

int main(void) {
    System_Gateway_Init();

    while (1) {
        // 1. 轮询检查是否收到 Zigbee 数据帧
        if (zigbee_rx_flag == 1) {
            zigbee_rx_flag = 0; // 清除标志位
            new_data_received = 1;

            printf("Received Zigbee data from Node %d\r\n", latest_node_data.node_id);

            // 2. 刷新本地 OLED 显示
            Gateway_Update_OLED(&latest_node_data);

            // 3. 将收到的节点数据通过 DTU 转发到服务器
            Gateway_Upload_DTU(&latest_node_data);
        }

        Delay_ms(100);
    }
}
