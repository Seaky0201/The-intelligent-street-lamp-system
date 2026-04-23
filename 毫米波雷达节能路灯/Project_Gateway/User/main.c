#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "zigbee.h"
#include "air790e.h"
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
    Air790E_Init();
    
    if (Air790E_ModuleInit() == AIR790E_OK) {
        if (Air790E_MQTT_Connect() == AIR790E_OK) {
            OLED_ShowString(2, 1, "4G/MQTT: OK ");
            Air790E_MQTT_Subscribe(MQTT_SUB_TOPIC);
        } else {
            OLED_ShowString(2, 1, "MQTT: FAILED");
        }
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

// 4G 上云
void Gateway_Upload_MQTT(ZigBee_NodeData *data) {
    char json_buf[200];
    sprintf(json_buf, 
        "{\"id\":%d,\"lux\":%.1f,\"temp\":%.1f,\"humi\":%.1f,\"motion\":%d,\"bright\":%d}",
        data->node_id, data->lux, data->temperature, data->humidity, 
        data->motion_detected, data->brightness);
    
    Air790E_MQTT_Publish(MQTT_PUB_TOPIC, json_buf);
}

int main(void) {
    System_Gateway_Init();

    while (1) {
        // 1. 轮询检查是否收到 Zigbee 数据帧
        // 注意：你需要在 zigbee.c 中实现解析逻辑，当收到完整帧时，将数据存入 latest_node_data 并置位标志
        if (zigbee_rx_flag == 1) { 
            // 假设 zigbee.c 中的串口中断已经把数据按协议解析到了 latest_node_data
            zigbee_rx_flag = 0; // 清除标志位
            new_data_received = 1;
            
            printf("Received Zigbee data from Node %d\r\n", latest_node_data.node_id);
            
            // 2. 刷新本地 OLED 显示
            Gateway_Update_OLED(&latest_node_data);
            
            // 3. 将收到的节点数据转发到云端
            Gateway_Upload_MQTT(&latest_node_data);
        }

        // 4. 处理云端下发的 MQTT 指令 (例如云端强制开关灯)
        // 这里的逻辑依赖你 air790e.c 中的 MQTT 接收回调
        // 如果收到指令，可以通过 ZigBee_SendByte() 转发给节点

        Delay_ms(100); 
    }
}
