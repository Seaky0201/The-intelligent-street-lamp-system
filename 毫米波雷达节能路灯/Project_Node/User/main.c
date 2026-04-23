#include "stm32f10x.h"
#include "Delay.h"
#include "pwm_led.h"
#include "usart_radar.h" // 目前内部封装的是 SR501
#include "bh1750.h"  
#include "aht20.h"
#include "zigbee.h"

#define LIGHT_THRESHOLD_DAY 50.0   
#define BRIGHTNESS_OFF      0      
#define BRIGHTNESS_IDLE     20     
#define BRIGHTNESS_FULL     100    
#define DELAY_TIME_MS       5000   

#define NODE_ID             1      
#define DATA_UPLOAD_INTERVAL 50    // Zigbee上传间隔 5秒

uint32_t last_detect_time = 0;     
uint32_t system_tick_ms = 0;       
uint8_t  current_brightness = 0;   

void System_Node_Init(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    USART1_Debug_Init();
    printf("--- Street Light Node v1.0 ---\r\n");
    
    PWM_LED_Init();
    Set_Streetlight_Brightness(BRIGHTNESS_OFF); 
    
    SR501_GPIO_Init();
    BH1750_Init();
    AHT20_Init();
    ZigBee_Init(); // 初始化USART3与Zigbee模块通信
}

int main(void) {
    float current_light_lux = 0.0;
    AHT20_Data env_data = {0.0, 0.0};  
    uint32_t upload_counter = 0;        
    uint8_t zigbee_cmd = 0;             

    System_Node_Init();

    while (1) {
        system_tick_ms += 100; 
        upload_counter++;

        // 1. 采集数据
        current_light_lux = BH1750_ReadLux();
        AHT20_ReadData(&env_data);
        SR501_Update_Status(); 

        // 2. 本地控制逻辑
        if (current_light_lux > LIGHT_THRESHOLD_DAY) {
            current_brightness = BRIGHTNESS_OFF;
        } else {
            if (radar_target_detected == 1) { // 探测到人
                current_brightness = BRIGHTNESS_FULL;
                last_detect_time = system_tick_ms;
            } else {
                if ((system_tick_ms - last_detect_time) > DELAY_TIME_MS) {
                    current_brightness = BRIGHTNESS_IDLE; // 延时后微亮
                } else {
                    current_brightness = BRIGHTNESS_FULL; // 延时中
                }
            }			
        }
        Set_Streetlight_Brightness(current_brightness);

        // 3. 通过Zigbee定时发送状态给网关
        if (upload_counter % DATA_UPLOAD_INTERVAL == 0) {
            ZigBee_NodeData node;
            node.node_id = NODE_ID;
            node.lux = current_light_lux;
            node.temperature = env_data.temperature;
            node.humidity = env_data.humidity;
            node.motion_detected = radar_target_detected;
            node.light_status = (current_brightness == 0) ? 0 : (current_brightness <= BRIGHTNESS_IDLE) ? 1 : 2;
            node.brightness = current_brightness;
            
            ZigBee_SendNodeData(&node);
            printf("Data sent to Gateway via Zigbee.\r\n");
        }

        // 4. 接收来自网关的强制控制指令 (如有)
        zigbee_cmd = ZigBee_CheckCommand();
        if (zigbee_cmd == 0x01) { current_brightness = BRIGHTNESS_FULL; Set_Streetlight_Brightness(BRIGHTNESS_FULL); }
        else if (zigbee_cmd == 0x02) { current_brightness = BRIGHTNESS_OFF; Set_Streetlight_Brightness(BRIGHTNESS_OFF); }

        Delay_ms(100); 
    }
}
