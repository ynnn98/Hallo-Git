#ifndef DHT11_H
#define DHT11_H


// DHT11 数据类型定义
typedef struct
{
	uint8_t humi_high8bit; // 原始数据：湿度高8位
	uint8_t humi_low8bit;  // 原始数据：湿度低8位
	uint8_t temp_high8bit; // 原始数据：温度高8位
	uint8_t temp_low8bit;  // 原始数据：温度高8位
	uint8_t check_sum;	   // 校验和
	float humidity;		   // 实际湿度
	float temperature;	   // 实际温度
} DHT11_Data_TypeDef;


// 函数声明
void dht11_init(void);
errcode_t dht11_read_data(DHT11_Data_TypeDef *DHT11_Data);

#endif