/*****************************************************************************************/
/*                                                                                       */
/*                  版权所有：沈阳市网联通信规划设计有限公司                                 */
/*                  开发人员：程国辉 刘艳                                                  */
/*                  联系方式：908536420  3512904489                                       */
/*                  文件名称：co2.h                                                      */
/*                  功能描述：JW01/TVOC-301 空气质量传感器驱动头文件                       */
/*                           支持硬件 UART 和软件 UART（GPIO 位冲）两种模式              */
/*                  开发时间：2026年7月                                                   */
/*                  本程序只供学习使用，未经作者许可，不得用于其它任何用途                    */
/*                  版本：V2.0                                                           */
/*                  版权所有，盗版必究                                                    */
/*                                                                                       */
/*****************************************************************************************/

#ifndef __CO2_H__
#define __CO2_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "common_def.h"
#include "errcode.h"
#include "pinctrl.h"
#include "uart.h"
#include "gpio.h"

/* 数据帧协议：2C E4 TVOC_H TVOC_L HCHO_H HCHO_L eCO2_H eCO2_L CS */
#define CO2_FRAME_LEN               9
#define CO2_FRAME_HEAD_0            0x2C
#define CO2_FRAME_HEAD_1            0xE4

/* 软件 UART 默认引脚（I02 = GPIO_02）*/
#define CO2_SW_PIN_DEFAULT          GPIO_02
#define CO2_SW_BAUDRATE             9600

/* CO2 传感器数据类型定义 */
typedef struct {
    uint16_t tvoc;              /* TVOC 浓度，单位 ug/m^3 */
    uint16_t formaldehyde;      /* 甲醛浓度，单位 ug/m^3 */
    uint16_t eco2;              /* 等效 CO2 浓度，单位 ppm */
    uint8_t  data_ready;        /* 1：有新数据 */
} CO2_Data_TypeDef;

/* 全局变量声明 */
extern volatile uint16_t g_co2_tvoc;
extern volatile uint16_t g_co2_formaldehyde;
extern volatile uint16_t g_co2_eco2;
extern volatile uint8_t  g_co2_data_ready;

/**
 * @brief 初始化 CO2 传感器（硬件 UART 模式）
 * @param baudrate 波特率，传0使用默认值9600
 * @return ERRCODE_SUCC 成功，其他失败
 */
errcode_t co2_init(uint32_t baudrate);

/**
 * @brief 初始化 CO2 传感器（软件 UART 模式，GPIO 位冲接收）
 * @param rx_pin 接收引脚号（如 GPIO_02）
 * @return ERRCODE_SUCC 成功，其他失败
 */
errcode_t co2_init_sw(pin_t rx_pin);

/**
 * @brief 释放资源
 */
errcode_t co2_deinit(void);

/**
 * @brief 获取 TVOC 浓度 (ug/m^3)
 */
uint16_t co2_get_tvoc(void);

/**
 * @brief 获取甲醛浓度 (ug/m^3)
 */
uint16_t co2_get_formaldehyde(void);

/**
 * @brief 获取等效 CO2 浓度 (ppm)
 */
uint16_t co2_get_eco2(void);

/**
 * @brief 判断是否有新数据
 * @return 1=有新数据，0=无
 */
uint8_t co2_is_data_ready(void);

/**
 * @brief 读取数据快照
 * @param data 输出结构体指针
 * @return 0成功，-1参数错误
 */
int co2_get_data(CO2_Data_TypeDef *data);

/**
 * @brief 清除数据就绪标志
 */
void co2_clear_flag(void);

#ifdef __cplusplus
}
#endif

#endif /* __CO2_H__ */
