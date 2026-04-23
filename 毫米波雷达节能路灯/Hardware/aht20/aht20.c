#include "aht20.h"
#include "Delay.h"

/*
 * AHT20 温湿度传感器驱动
 * 通信方式：软件 I2C (与 BH1750 共享 PB6-SCL / PB7-SDA)
 * I2C 地址：0x38 (写: 0x70, 读: 0x71)
 * 
 * 注意：I2C 底层函数复用 bh1750.c 中已有的实现，通过 extern 引用
 */

// 复用 bh1750.c 中的 I2C 底层函数 (PB6-SCL, PB7-SDA)
extern void I2C_Start(void);
extern void I2C_Stop(void);
extern void I2C_SendByte(uint8_t byte);
extern uint8_t I2C_ReadByte(uint8_t ack);
extern void BH1750_Delay(void);

/**
  * @brief  初始化 AHT20 传感器
  * @note   上电后需等待40ms，然后发送初始化校准命令
  */
void AHT20_Init(void)
{
    // AHT20 上电后需要等待 40ms
    Delay_ms(40);
    
    // 发送初始化校准命令: 0xBE 0x08 0x00
    I2C_Start();
    I2C_SendByte(AHT20_ADDR_W);    // 0x70 写地址
    I2C_SendByte(AHT20_CMD_INIT);  // 0xBE 初始化命令
    I2C_SendByte(0x08);            // 参数1
    I2C_SendByte(0x00);            // 参数2
    I2C_Stop();
    
    Delay_ms(10); // 等待初始化完成
}

/**
  * @brief  读取 AHT20 状态字节
  * @retval 状态字节值
  */
static uint8_t AHT20_ReadStatus(void)
{
    uint8_t status;
    
    I2C_Start();
    I2C_SendByte(AHT20_ADDR_R);   // 0x71 读地址
    status = I2C_ReadByte(0);      // NACK，只读一个字节
    I2C_Stop();
    
    return status;
}

/**
  * @brief  读取 AHT20 温湿度数据
  * @param  data: 指向 AHT20_Data 结构体的指针，用于存储温湿度结果
  * @retval 0=成功, 1=失败(传感器忙)
  * 
  * @note   数据格式 (共6字节):
  *         Byte0: 状态字节
  *         Byte1: 湿度[19:12]
  *         Byte2: 湿度[11:4]
  *         Byte3: 湿度[3:0] | 温度[19:16]
  *         Byte4: 温度[15:8]
  *         Byte5: 温度[7:0]
  */
uint8_t AHT20_ReadData(AHT20_Data *data)
{
    uint8_t raw[6];
    uint32_t humi_raw, temp_raw;
    uint8_t i;
    uint8_t retry = 0;
    
    // 1. 发送触发测量命令: 0xAC 0x33 0x00
    I2C_Start();
    I2C_SendByte(AHT20_ADDR_W);        // 0x70 写地址
    I2C_SendByte(AHT20_CMD_TRIGGER);   // 0xAC 触发测量
    I2C_SendByte(0x33);                // 参数1
    I2C_SendByte(0x00);                // 参数2
    I2C_Stop();
    
    // 2. 等待测量完成 (典型值约80ms)
    Delay_ms(80);
    
    // 3. 检查状态位 Bit[7]，为0表示测量完成
    while ((AHT20_ReadStatus() & 0x80) != 0)
    {
        Delay_ms(10);
        retry++;
        if (retry > 10) // 超时保护，最多等100ms
        {
            data->temperature = 0;
            data->humidity = 0;
            return 1; // 返回失败
        }
    }
    
    // 4. 读取6字节数据
    I2C_Start();
    I2C_SendByte(AHT20_ADDR_R);   // 0x71 读地址
    for (i = 0; i < 5; i++)
    {
        raw[i] = I2C_ReadByte(1);  // 前5个字节发送ACK
    }
    raw[5] = I2C_ReadByte(0);     // 最后1个字节发送NACK
    I2C_Stop();
    
    // 5. 解析湿度数据 (20位): Byte1全部 + Byte2全部 + Byte3高4位
    humi_raw = ((uint32_t)raw[1] << 12) | ((uint32_t)raw[2] << 4) | ((uint32_t)raw[3] >> 4);
    
    // 6. 解析温度数据 (20位): Byte3低4位 + Byte4全部 + Byte5全部
    temp_raw = (((uint32_t)raw[3] & 0x0F) << 16) | ((uint32_t)raw[4] << 8) | (uint32_t)raw[5];
    
    // 7. 根据 AHT20 数据手册公式换算
    // 湿度 = (原始值 / 2^20) * 100 %RH
    data->humidity = (float)humi_raw / 1048576.0f * 100.0f;
    
    // 温度 = (原始值 / 2^20) * 200 - 50 °C
    data->temperature = (float)temp_raw / 1048576.0f * 200.0f - 50.0f;
    
    return 0; // 成功
}
