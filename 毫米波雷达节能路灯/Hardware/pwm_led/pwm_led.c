#include "pwm_led.h"

/**
  * @brief  初始化 PA8 引脚和 TIM1_CH1 输出 PWM
  */
void PWM_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // 开启 GPIOA 和 TIM1 的时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1, ENABLE);

    // 配置 PA8 为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // TIM1 基础配置 (72MHz 主频，分频后为 1MHz，周期 1000 即 1kHz PWM)
    TIM_TimeBaseStructure.TIM_Period = 999;
    TIM_TimeBaseStructure.TIM_Prescaler = 71;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    // TIM1 通道 1 PWM 模式配置
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0; // 初始占空比 0
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);

    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    
    // 高级定时器 TIM1 必须开启这一句才能输出 PWM
    TIM_CtrlPWMOutputs(TIM1, ENABLE); 
    TIM_Cmd(TIM1, ENABLE);
}

/**
  * @brief  设置路灯亮度 (PWM 占空比控制)
  * @param  percentage: 0~100 的亮度百分比
  */
void Set_Streetlight_Brightness(uint8_t percentage)
{
    if(percentage > 100) percentage = 100;
    // 定时器 ARR=999，故比较值 = (999+1) * percentage / 100
    uint16_t compare_val = percentage * 10; 
    TIM_SetCompare1(TIM1, compare_val);
}
