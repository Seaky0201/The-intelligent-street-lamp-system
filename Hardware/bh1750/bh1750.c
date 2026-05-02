#include "bh1750.h"

// 简单的微秒延时（用于I2C波形）
void BH1750_Delay(void) {
    uint32_t i = 50; while(i--);
}

// I2C 起始信号
void I2C_Start(void) {
    GPIO_SetBits(I2C_PORT, SCL_PIN | SDA_PIN);
    BH1750_Delay();
    GPIO_ResetBits(I2C_PORT, SDA_PIN);
    BH1750_Delay();
    GPIO_ResetBits(I2C_PORT, SCL_PIN);
}

// I2C 停止信号
void I2C_Stop(void) {
    GPIO_ResetBits(I2C_PORT, SDA_PIN);
    GPIO_SetBits(I2C_PORT, SCL_PIN);
    BH1750_Delay();
    GPIO_SetBits(I2C_PORT, SDA_PIN);
    BH1750_Delay();
}

// 发送一个字节
void I2C_SendByte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) GPIO_SetBits(I2C_PORT, SDA_PIN);
        else GPIO_ResetBits(I2C_PORT, SDA_PIN);
        GPIO_SetBits(I2C_PORT, SCL_PIN);
        BH1750_Delay();
        GPIO_ResetBits(I2C_PORT, SCL_PIN);
        byte <<= 1;
    }
    // 接收ACK应答（简化处理）
    GPIO_SetBits(I2C_PORT, SCL_PIN);
    BH1750_Delay();
    GPIO_ResetBits(I2C_PORT, SCL_PIN);
}

//I2C读取一个字节的底层函数
uint8_t I2C_ReadByte(uint8_t ack) {
    uint8_t byte = 0;
    
    // 释放SDA线（开漏输出模式下，输出高电平就相当于变成了输入模式）
    GPIO_SetBits(I2C_PORT, SDA_PIN); 
    BH1750_Delay();
    
    for (uint8_t i = 0; i < 8; i++) {
        byte <<= 1; // 左移一位，准备接收新数据
        
        GPIO_SetBits(I2C_PORT, SCL_PIN); // 拉高时钟线，此时SDA上的数据是有效的
        BH1750_Delay();
        
        // 读取SDA引脚的电平
        if (GPIO_ReadInputDataBit(I2C_PORT, SDA_PIN)) {
            byte |= 0x01; 
        }
        
        GPIO_ResetBits(I2C_PORT, SCL_PIN); // 拉低时钟线，允许传感器准备下一位数据
        BH1750_Delay();
    }
    
    // 发送应答(ACK)或非应答(NACK)信号
    if (ack) {
        GPIO_ResetBits(I2C_PORT, SDA_PIN); // 0 为 ACK，告诉传感器"我还要读"
    } else {
        GPIO_SetBits(I2C_PORT, SDA_PIN);   // 1 为 NACK，告诉传感器"我不读了"
    }
    
    GPIO_SetBits(I2C_PORT, SCL_PIN);
    BH1750_Delay();
    GPIO_ResetBits(I2C_PORT, SCL_PIN);
    BH1750_Delay();
    
    return byte;
}

// 初始化硬件
void BH1750_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = SCL_PIN | SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏输出，模拟I2C必备
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(I2C_PORT, &GPIO_InitStructure);

    // 发送启动指令
    I2C_Start();
    I2C_SendByte(0x46); // 写入器件地址
    I2C_SendByte(0x01); // 通电指令
    I2C_Stop();
    
    I2C_Start();
    I2C_SendByte(0x46); 
    I2C_SendByte(0x10); // 连续高分辨率模式
    I2C_Stop();
}


//读取光照强度函数
float BH1750_ReadLux(void) {
    uint8_t high_byte = 0;
    uint8_t low_byte = 0;
    uint16_t data = 0;
    float lux = 0;

    I2C_Start();
    I2C_SendByte(0x47); // 0x47 = 0x46(器件地址) + 1(读命令)
    
    // 连续读取两个字节
    high_byte = I2C_ReadByte(1); // 读取高8位，并发送ACK应答
    low_byte  = I2C_ReadByte(0); // 读取低8位，并发送NACK非应答结束
    
    I2C_Stop();
    
    // 将高8位和低8位合并成一个16位的数据
    data = (high_byte << 8) | low_byte;
    
    // BH1750 的计算公式：光照强度 = 读取的16位数据 / 1.2
    lux = (float)data / 1.2;
    
    return lux;
}
