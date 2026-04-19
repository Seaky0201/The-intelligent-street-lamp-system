#include "stm32f10x.h"
#include "Delay.h"
#include "pwm_led.h"
#include "usart_radar.h" // 这里面现在装的是 SR501 的驱动
#include "bh1750.h"  
#include "OLED.h"

// --- 参数配置 ---
#define LIGHT_THRESHOLD_DAY 50.0  // 光照阈值(Lux)：大于10认为是白天
#define BRIGHTNESS_OFF      0     // 关灯 (0%)
#define BRIGHTNESS_IDLE     20    // 夜间无人微亮 (20%)
#define BRIGHTNESS_FULL     100   // 有人全亮 (100%)
#define DELAY_TIME_MS       5000  // 人走后延时时间(毫秒)

uint32_t last_detect_time = 0;    // 记录最后一次有人的时间戳
uint32_t system_tick_ms = 0;      // 模拟系统时间戳

int main(void)
{
    float current_light_lux = 0.0;

    // 1. 初始化各硬件模块
    PWM_LED_Init();        // 初始化路灯 PWM 控制 (PA8)
    USART1_Debug_Init();   // 初始化调试串口 (PA9/PA10)
    USART2_Radar_Init();   // 初始化传感器引脚 (现在是初始化 PA3 作为输入)
    OLED_Init();           // OLED 屏幕初始化
    OLED_Clear();          // 清屏
    BH1750_Init();         // 初始化光照传感器

    printf("The intelligent street lamp system has been activated...\r\n");
    Set_Streetlight_Brightness(BRIGHTNESS_OFF); // 上电默认先关灯
	
    // 2. 初始化 OLED 静态显示界面
    OLED_ShowString(1, 1, "Smart StreetLight"); 
    OLED_ShowString(2, 1, "Lux   :");
    OLED_ShowString(3, 1, "Motion: "); 
    OLED_ShowString(4, 1, "Status: ");
	
    // 3. 主循环
    while (1)
    {
        system_tick_ms += 100; // 模拟系统时间累加 (每次循环加100ms)

        // 读取光照传感器数据
        current_light_lux = BH1750_ReadLux();
        
        // 【关键】读取 SR501 传感器状态，更新 radar_target_detected 变量
        SR501_Update_Status(); 
        
        // 打印调试信息到电脑串口助手
        printf("Lux: %.1f | Motion: %d | Time: %d ms\r\n", current_light_lux, radar_target_detected, system_tick_ms);
		
        // 更新 OLED 光照数值
        OLED_ShowNum(2, 9, (uint32_t)current_light_lux, 4);
		
        // 更新 OLED 传感器状态显示
        if (radar_target_detected) {
            OLED_ShowString(3, 9, "YES ");
        } else {
            OLED_ShowString(3, 9, "NO  ");
        }

        // --- 核心控制逻辑 ---
        if (current_light_lux > LIGHT_THRESHOLD_DAY) 
        {
            // 【白天模式】强制关灯
            Set_Streetlight_Brightness(BRIGHTNESS_OFF);
            OLED_ShowString(4, 9, "DAY  "); // 后面加空格为了覆盖掉旧字符
        }
        else 
        {
            // 【黑夜模式】根据人进行联动
            if (radar_target_detected == 1)
            {
                Set_Streetlight_Brightness(BRIGHTNESS_FULL); // 有人，路灯全亮
                last_detect_time = system_tick_ms;           // 刷新最后一次检测到人的时间
                OLED_ShowString(4, 9, "ON   ");
            }
            else
            {
                // 如果没人，判断是不是已经过去了 5 秒
                if ((system_tick_ms - last_detect_time) > DELAY_TIME_MS) 
                {
                    Set_Streetlight_Brightness(BRIGHTNESS_IDLE); // 超过5秒没人，变暗微亮
                    OLED_ShowString(4, 9, "IDLE ");
                } 
                else 
                {
                    Set_Streetlight_Brightness(BRIGHTNESS_FULL); // 5秒内，保持全亮延时
                    OLED_ShowString(4, 9, "DELAY");
                }
            }			
        }
        
        Delay_ms(100); // 循环间隔 100ms
    }
}

/*

#include "stm32f10x.h"
#include "Delay.h"
#include "pwm_led.h"
#include "usart_radar.h"
#include "bh1750.h"  
#include "oled.h"
// #include "aht20.h"

// --- 参数配置 ---
#define LIGHT_THRESHOLD_DAY 100.0  // 光照阈值(Lux)：大于100认为是白天
#define BRIGHTNESS_OFF      0      // 关灯
#define BRIGHTNESS_IDLE     20     // 夜间无人微亮
#define BRIGHTNESS_FULL     100    // 有车全亮
#define DELAY_TIME_MS       5000   // 延时时间(毫秒)

uint32_t last_detect_time = 0;     // 记录最后一次有车的时间戳
uint32_t system_tick_ms = 0;       // 系统时间戳

int main(void)
{
    //初始化各硬件模块
    PWM_LED_Init();        // 初始化路灯 PWM 控制
    USART1_Debug_Init();   // 初始化调试串口
    USART2_Radar_Init();   // 初始化雷达串口
    OLED_Init();           // OLED 屏幕初始化
    OLED_Clear();          // 清空屏幕上的噪点
    BH1750_Init();         // 初始化光照传感器
    // AHT20_Init();       // 初始化温湿度传感器

    printf("The intelligent street lamp system has been activatde...\r\n");//智能路灯系统启动...
    Set_Streetlight_Brightness(BRIGHTNESS_OFF); // 默认先关灯
	
	OLED_ShowString(1, 1, "StreetLight"); 
    OLED_ShowString(2, 1, "Lux  :");
    OLED_ShowString(3, 1, "Radar: ERROR"); // 雷达目前坏了，先写死状态
    OLED_ShowString(4, 1, "Light:");
	
    float current_light_lux = 0.0;
    
    //主循环
    while (1)
    {
		
        system_tick_ms += 100; // 模拟系统时间累加

        current_light_lux = BH1750_ReadLux();
        //current_light_lux = 50.0; // 测试用：强制设定为黑夜
        
        //printf("illumination: %.1f Lux | radar: %d\r\n", current_light_lux, radar_target_detected);
		OLED_ShowNum(2, 8,current_light_lux, 4);
		
        // 核心控制逻辑
        if (current_light_lux > LIGHT_THRESHOLD_DAY) 
        {
            // 白天强制关灯
            Set_Streetlight_Brightness(BRIGHTNESS_OFF);
			OLED_ShowString(4, 8, "OFF ");
        }
        else 
        {
            // 黑夜雷达联动
            if (radar_target_detected == 1)
            {
                Set_Streetlight_Brightness(BRIGHTNESS_FULL); 
                last_detect_time = system_tick_ms; // 有车，刷新时间
				OLED_ShowString(4, 8, "OFF ");
            }
            else
            {
                if ((system_tick_ms - last_detect_time) > DELAY_TIME_MS) {
                    Set_Streetlight_Brightness(BRIGHTNESS_IDLE); // 超过5秒没车，变暗
					OLED_ShowString(4, 8, "IDLE");
                } else {
                    Set_Streetlight_Brightness(BRIGHTNESS_FULL); // 5秒内，保持全亮
					OLED_ShowString(4, 8, "ON  ");
                }
            }			
        }
        
        Delay_ms(100); 
    }
}
*/
/*
// 路灯硬件独立测试
    printf("Start test...\r\n");//开始执行路灯硬件独立测试

    while (1)
    {
        // 1. 强制路灯全亮
        printf("State : bright (100%%)\r\n");
        Set_Streetlight_Brightness(100);
        
        // 粗暴延时大概 2 秒钟 (避免使用未初始化的Delay)
        for(uint32_t i = 0; i < 10000000; i++); 

        // 2. 强制路灯关闭
        printf("State : dark (0%%)\r\n");
        Set_Streetlight_Brightness(0);
        
        // 粗暴延时大概 2 秒钟
        for(uint32_t i = 0; i < 10000000; i++); 
    }
}
*/
/*
// 路灯、光照、oled测试
int main(void)
{
    // 1. 初始化各硬件模块
    PWM_LED_Init();        // 初始化路灯 PWM 控制
    USART1_Debug_Init();   // 初始化调试串口
    USART2_Radar_Init();   // 初始化雷达串口
    OLED_Init();           // OLED 屏幕初始化
    OLED_Clear();          // 清空屏幕上的噪点
    BH1750_Init();         // 初始化光照传感器
    // AHT20_Init();       // 初始化温湿度传感器

    printf("The intelligent street lamp system has been activatde...\r\n");//智能路灯系统启动...
    Set_Streetlight_Brightness(BRIGHTNESS_OFF); // 默认先关灯
	
	OLED_ShowString(1, 1, "StreetLight"); 
    OLED_ShowString(2, 1, "Lux  :");
    OLED_ShowString(3, 1, "Radar: ERROR "); // 雷达目前坏了，先写死状态
    OLED_ShowString(4, 1, "Light:");
	
    float current_light_lux = 0.0;
    
    // 2. 主循环
    while (1)
    {
		
        system_tick_ms += 100; // 模拟系统时间累加

        current_light_lux = BH1750_ReadLux();
        //current_light_lux = 50.0; // 测试用：强制设定为黑夜
        
        printf("illumination: %.1f Lux | radar: %d\r\n", current_light_lux, radar_target_detected);
		OLED_ShowNum(2, 8, current_light_lux, 4);
		
        // 核心控制逻辑
        if (current_light_lux > LIGHT_THRESHOLD_DAY) 
        {
            // 白天强制关灯
            Set_Streetlight_Brightness(BRIGHTNESS_OFF);
			OLED_ShowString(4, 8, "OFF");
        }
        else 
        { 
			//夜晚强制关灯
            Set_Streetlight_Brightness(BRIGHTNESS_FULL); 
           	OLED_ShowString(4, 8, "ON ");
        }
        
        Delay_ms(100); 
    }
}
*/

