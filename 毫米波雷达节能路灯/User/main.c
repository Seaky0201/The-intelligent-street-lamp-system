#include "stm32f10x.h"
#include "Delay.h"
#include "pwm_led.h"
#include "usart_radar.h"
#include "bh1750.h"  
#include "OLED.h"
#include "aht20.h"
#include "zigbee.h"
#include "air790e.h"

// --- 参数配置 ---
#define LIGHT_THRESHOLD_DAY 50.0   // 光照阈值(Lux)：大于50认为是白天
#define BRIGHTNESS_OFF      0      // 关灯 (0%)
#define BRIGHTNESS_IDLE     20     // 夜间无人微亮 (20%)
#define BRIGHTNESS_FULL     100    // 有人全亮 (100%)
#define DELAY_TIME_MS       5000   // 人走后延时时间(毫秒)

// --- 节点配置 ---
#define NODE_ID             1      // 本路灯节点编号 (1~255)
#define DATA_UPLOAD_INTERVAL 50    // 数据上传间隔 (单位：主循环次数，50*100ms=5秒)
#define MQTT_UPLOAD_INTERVAL 100   // MQTT上传间隔 (单位：主循环次数，100*100ms=10秒)

// --- 全局变量 ---
uint32_t last_detect_time = 0;     // 记录最后一次有人的时间戳
uint32_t system_tick_ms = 0;       // 模拟系统时间戳
uint8_t  current_brightness = 0;   // 当前亮度百分比

// --- 函数声明 ---
void System_Init(void);
void OLED_UpdateDisplay(float lux, AHT20_Data *env, uint8_t motion, char *status);
void Upload_Data_ZigBee(float lux, AHT20_Data *env, uint8_t motion);
void Upload_Data_MQTT(float lux, AHT20_Data *env, uint8_t motion);

int main(void)
{
    float current_light_lux = 0.0;
    AHT20_Data env_data = {0.0, 0.0};  // 温湿度数据
    uint32_t upload_counter = 0;        // 数据上传计数器
    uint8_t zigbee_cmd = 0;             // ZigBee 收到的控制命令
    char status_str[6] = "OFF  ";       // 当前状态字符串

    // 1. 系统初始化
    System_Init();

    // 2. 主循环
    while (1)
    {
        system_tick_ms += 100; // 模拟系统时间累加 (每次循环约100ms)
        upload_counter++;

        // ==================== 传感器数据采集 ====================
        
        // 读取光照强度 (BH1750)
        current_light_lux = BH1750_ReadLux();
        
        // 读取温湿度 (AHT20)
        AHT20_ReadData(&env_data);
        
        // 读取人体检测 (SR501, PA4)
        SR501_Update_Status(); 
        
        // 调试串口输出
        printf("Lux:%.1f T:%.1fC H:%.1f%% Motion:%d Bright:%d%%\r\n", 
               current_light_lux, env_data.temperature, env_data.humidity,
               radar_target_detected, current_brightness);

        // ==================== 核心控制逻辑 ====================
        
        if (current_light_lux > LIGHT_THRESHOLD_DAY) 
        {
            // 【白天模式】光照充足，强制关灯节能
            current_brightness = BRIGHTNESS_OFF;
            Set_Streetlight_Brightness(current_brightness);
            sprintf(status_str, "DAY  ");
        }
        else 
        {
            // 【黑夜模式】根据人体感应联动
            if (radar_target_detected == 1)
            {
                // 有人 → 路灯全亮
                current_brightness = BRIGHTNESS_FULL;
                Set_Streetlight_Brightness(current_brightness);
                last_detect_time = system_tick_ms;
                sprintf(status_str, "ON   ");
            }
            else
            {
                // 无人 → 判断延时
                if ((system_tick_ms - last_detect_time) > DELAY_TIME_MS) 
                {
                    // 超过5秒无人 → 变暗微亮节能
                    current_brightness = BRIGHTNESS_IDLE;
                    Set_Streetlight_Brightness(current_brightness);
                    sprintf(status_str, "IDLE ");
                } 
                else 
                {
                    // 5秒内 → 保持全亮延时
                    current_brightness = BRIGHTNESS_FULL;
                    Set_Streetlight_Brightness(current_brightness);
                    sprintf(status_str, "DELAY");
                }
            }			
        }

        // ==================== OLED 显示更新 ====================
        OLED_UpdateDisplay(current_light_lux, &env_data, radar_target_detected, status_str);

        // ==================== ZigBee 数据上传 ====================
        // 每5秒通过 ZigBee 上传一次传感器数据给协调器
        if (upload_counter % DATA_UPLOAD_INTERVAL == 0)
        {
            Upload_Data_ZigBee(current_light_lux, &env_data, radar_target_detected);
        }

        // ==================== 4G MQTT 数据上传 ====================
        // 每10秒通过 Air790E 上传一次数据到云平台
        if (upload_counter % MQTT_UPLOAD_INTERVAL == 0)
        {
            Upload_Data_MQTT(current_light_lux, &env_data, radar_target_detected);
        }

        // ==================== ZigBee 命令检查 ====================
        // 检查协调器是否下发了控制命令
        zigbee_cmd = ZigBee_CheckCommand();
        if (zigbee_cmd != 0)
        {
            switch (zigbee_cmd)
            {
                case 0x01: // 远程开灯
                    Set_Streetlight_Brightness(BRIGHTNESS_FULL);
                    current_brightness = BRIGHTNESS_FULL;
                    printf("[CMD] Remote ON\r\n");
                    break;
                case 0x02: // 远程关灯
                    Set_Streetlight_Brightness(BRIGHTNESS_OFF);
                    current_brightness = BRIGHTNESS_OFF;
                    printf("[CMD] Remote OFF\r\n");
                    break;
                case 0x03: // 设置亮度 (后续可从数据域获取具体值)
                    printf("[CMD] Set brightness\r\n");
                    break;
                case 0xFF: // 查询状态
                    Upload_Data_ZigBee(current_light_lux, &env_data, radar_target_detected);
                    printf("[CMD] Status query\r\n");
                    break;
                default:
                    break;
            }
        }
        
        Delay_ms(100); // 主循环间隔 100ms
    }
}

/**
  * @brief  系统初始化函数，依次初始化所有硬件模块
  */
void System_Init(void)
{
    // 设置 NVIC 中断分组 (整个系统只需设置一次)
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    
    // 初始化调试串口 (USART1, PA9/PA10, 115200)
    USART1_Debug_Init();
    printf("\r\n========================================\r\n");
    printf("  Smart Street Light System v2.0\r\n");
    printf("  Node ID: %d\r\n", NODE_ID);
    printf("========================================\r\n");
    
    // 初始化路灯 PWM 控制 (TIM1_CH1, PA8)
    PWM_LED_Init();
    Set_Streetlight_Brightness(BRIGHTNESS_OFF); // 默认关灯
    printf("[INIT] PWM LED OK\r\n");
    
    // 初始化 SR501 人体红外传感器 (PA4)
    SR501_GPIO_Init();
    printf("[INIT] SR501 (PA4) OK\r\n");
    
    // 初始化 OLED 显示屏 (I2C, PB8-SCL, PB9-SDA)
    OLED_Init();
    OLED_Clear();
    printf("[INIT] OLED OK\r\n");
    
    // 初始化 BH1750 光照传感器 (I2C, PB6-SCL, PB7-SDA)
    BH1750_Init();
    printf("[INIT] BH1750 OK\r\n");
    
    // 初始化 AHT20 温湿度传感器 (I2C, PB6-SCL, PB7-SDA, 与BH1750共享总线)
    AHT20_Init();
    printf("[INIT] AHT20 OK\r\n");
    
    // 初始化 ZigBee 通信模块 (USART3, PB10-TX, PB11-RX)
    ZigBee_Init();
    printf("[INIT] ZigBee (USART3) OK\r\n");
    
    // 初始化 Air790E 4G 模块 (USART2, PA2-TX, PA3-RX)
    Air790E_Init();
    printf("[INIT] Air790E (USART2) OK\r\n");
    
    // Air790E 模块初始化 (AT握手、检查SIM卡、注网)
    printf("[INIT] Air790E module init...\r\n");
    if (Air790E_ModuleInit() == AIR790E_OK)
    {
        printf("[INIT] Air790E network OK\r\n");
        // MQTT 连接
        if (Air790E_MQTT_Connect() == AIR790E_OK)
        {
            printf("[INIT] MQTT connected!\r\n");
            // 订阅命令主题
            Air790E_MQTT_Subscribe(MQTT_SUB_TOPIC);
        }
    }
    else
    {
        printf("[INIT] Air790E init failed (will retry later)\r\n");
    }
    
    // 初始化 OLED 静态界面
    OLED_ShowString(1, 1, "StreetLight");
    OLED_ShowString(2, 1, "Lx:    T:");
    OLED_ShowString(3, 1, "Hu:    M:");
    OLED_ShowString(4, 1, "St:");
    
    printf("[INIT] System ready!\r\n\r\n");
}

/**
  * @brief  更新 OLED 显示内容
  * @param  lux:    光照强度
  * @param  env:    温湿度数据
  * @param  motion: 运动检测状态
  * @param  status: 状态字符串
  * 
  * @note   OLED 显示布局 (16列 x 4行):
  *         行1: StreetLight
  *         行2: Lx:9999 T:99
  *         行3: Hu:99   M:YES
  *         行4: St:IDLE  B:100
  */
void OLED_UpdateDisplay(float lux, AHT20_Data *env, uint8_t motion, char *status)
{
    // 光照值 (最大9999)
    uint32_t lux_val = (uint32_t)lux;
    if (lux_val > 9999) lux_val = 9999;
    OLED_ShowNum(2, 4, lux_val, 4);
    
    // 温度 (整数部分，2位)
    int32_t temp_val = (int32_t)env->temperature;
    if (temp_val < 0) temp_val = 0;
    if (temp_val > 99) temp_val = 99;
    OLED_ShowNum(2, 11, (uint32_t)temp_val, 2);
    
    // 湿度 (整数部分，2位)
    uint32_t humi_val = (uint32_t)env->humidity;
    if (humi_val > 99) humi_val = 99;
    OLED_ShowNum(3, 4, humi_val, 2);
    
    // 人体检测
    if (motion) {
        OLED_ShowString(3, 11, "YES");
    } else {
        OLED_ShowString(3, 11, "NO ");
    }
    
    // 状态
    OLED_ShowString(4, 4, status);
    
    // 亮度百分比
    OLED_ShowString(4, 11, "B:");
    OLED_ShowNum(4, 13, current_brightness, 3);
}

/**
  * @brief  通过 ZigBee 上传传感器数据到协调器
  */
void Upload_Data_ZigBee(float lux, AHT20_Data *env, uint8_t motion)
{
    ZigBee_NodeData node;
    
    node.node_id = NODE_ID;
    node.lux = lux;
    node.temperature = env->temperature;
    node.humidity = env->humidity;
    node.motion_detected = motion;
    node.light_status = (current_brightness == 0) ? 0 : 
                        (current_brightness <= BRIGHTNESS_IDLE) ? 1 : 2;
    node.brightness = current_brightness;
    
    ZigBee_SendNodeData(&node);
}

/**
  * @brief  通过 Air790E 4G MQTT 上传数据到云平台
  * @note   数据格式为 JSON 字符串
  */
void Upload_Data_MQTT(float lux, AHT20_Data *env, uint8_t motion)
{
    char json_buf[200];
    
    // 构造 JSON 数据
    sprintf(json_buf, 
        "{\\\"id\\\":%d,\\\"lux\\\":%.1f,\\\"temp\\\":%.1f,\\\"humi\\\":%.1f,\\\"motion\\\":%d,\\\"bright\\\":%d}",
        NODE_ID, lux, env->temperature, env->humidity, motion, current_brightness);
    
    // 通过 MQTT 发布
    Air790E_MQTT_Publish(MQTT_PUB_TOPIC, json_buf);
}
