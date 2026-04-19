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

void Update_OLED_Display(float light_lux, uint8_t target_detected)
{
    // 更新 OLED 光照数值
    OLED_ShowNum(2, 9, (uint32_t)light_lux, 4);

    // 更新 OLED 传感器状态显示
    if (target_detected) {
        OLED_ShowString(3, 9, "YES ");
    } else {
        OLED_ShowString(3, 9, "NO  ");
    }
}

void StreetLight_ControlLogic(float light_lux, uint8_t target_detected)
{
    if (light_lux > LIGHT_THRESHOLD_DAY)
    {
        // 【白天模式】强制关灯
        Set_Streetlight_Brightness(BRIGHTNESS_OFF);
        OLED_ShowString(4, 9, "DAY  "); // 后面加空格为了覆盖掉旧字符
    }
    else
    {
        // 【黑夜模式】根据人进行联动
        if (target_detected == 1)
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
}

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

        // 更新显示
        Update_OLED_Display(current_light_lux, radar_target_detected);

        // --- 核心控制逻辑 ---
        StreetLight_ControlLogic(current_light_lux, radar_target_detected);

        Delay_ms(100); // 循环间隔 100ms
    }
}
