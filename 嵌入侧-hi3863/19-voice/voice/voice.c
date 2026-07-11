/*
 * Copyright (c) 2024 Beijing HuaQingYuanJian Education Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pinctrl.h"
#include "uart.h"
#include "debug.h"  // 替换osal_debug.h，适配Hi3863调试宏
#include "soc_osal.h"
#include "app_init.h"
#include "string.h"
#include "cmsis_os2.h"
#include "math.h"
#include "gpio.h"
#include "../led/led.h"
#include "../dht11/dht11.h"
#include "../hcsr04/hcsr04.h"

int32_t distance;  // 存储测量距离（-1表示失败）

// 宏定义配置
#define DELAY_TIME_MS 10
#define UART_RECV_SIZE 14
#define UART_INT_MODE 1
#define UART_TASK_STACK_SIZE 0x1000
#define UART_TASK_PRIO 28

// 接收缓冲区与反馈数据包缓冲区
uint8_t uart_recv[UART_RECV_SIZE] = {0};
uint8_t usart_su_TXpacket[14] = {0};

// 中断模式接收标志
#if (UART_INT_MODE)
static uint8_t uart_rx_flag = 0;
#endif

// UART缓冲区配置
uart_buffer_config_t g_app_uart_buffer_config = {
    .rx_buffer = uart_recv,
    .rx_buffer_size = UART_RECV_SIZE
};

/**
 * @brief 初始化UART相关GPIO引脚
 */
void uart_gpio_init(void)
{
    uapi_pin_set_mode(GPIO_08, PIN_MODE_1); // GPIO08 -> UART TX
    uapi_pin_set_mode(GPIO_07, PIN_MODE_1); // GPIO07 -> UART RX

    // 设置IO引脚方向：TX输出，RX输入
    uapi_gpio_set_dir(GPIO_08, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_dir(GPIO_07, GPIO_DIRECTION_INPUT);
}

/**
 * @brief 初始化UART控制器配置（115200波特率，8N1）
 */
void uart_init_config(void)
{
    uart_attr_t attr = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };

    uart_pin_config_t pin_config = {
        .tx_pin = S_MGPIO0,   // 对应GPIO08（TX）
        .rx_pin = S_MGPIO1,   // 对应GPIO07（RX）
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };
    
    uapi_uart_deinit(UART_BUS_2);
    int ret = uapi_uart_init(UART_BUS_2, &pin_config, &attr, NULL, &g_app_uart_buffer_config);
    if (ret != 0) {
        // 替换OSAL_DEBUG_ERR为Hi3863支持的调试打印函数
        printf("UART init failed! ret = %02x\n", ret);
    }

    // 移除uapi_uart_enable_rx_intr，Hi3863通过注册回调自动使能中断
#if (UART_INT_MODE)
    // 无需单独调用使能函数，注册回调后自动生效
#endif
}

/**
 * @brief 语音指令解析与设备控制核心函数
 */
void voice_analysis(uint8_t *info)
{
    memset(usart_su_TXpacket, 0, 14);
    printf("Voice input: 0x%02x\n", info[0]);
    
    uint8_t temp = info[0];
    switch (temp) {
        case 0x01: // 打开灯
            printf("Voice cmd: Turn on lamp\r\n");
            led_on();
            break;
        
        case 0x02: // 关闭灯
            printf("Voice cmd: Turn off lamp\r\n");
            led_off();
            break;
        
           case 0x0A: // 询问当前温度
            printf("Voice cmd: Query temperature\r\n");
            DHT11_Data_TypeDef dht_temp_data; // 定义DHT11数据结构体
            // 读取DHT11数据（直接使用dht11.c的接口，无需全局变量）
            if (dht11_read_data(&dht_temp_data) == ERRCODE_SUCC) {
                // 温度反馈数据包组装（对应原wendu=temp_high8bit，wendul=temp_low8bit）
                usart_su_TXpacket[0] = 0XAA;
                usart_su_TXpacket[1] = 0X55;
                usart_su_TXpacket[2] = 0x03;
                usart_su_TXpacket[3] = dht_temp_data.temp_high8bit; // 温度整数部分
                usart_su_TXpacket[4] = 0x00;
                usart_su_TXpacket[5] = 0x00;
                usart_su_TXpacket[6] = 0x00;
                usart_su_TXpacket[7] = dht_temp_data.temp_low8bit;  // 温度小数部分
                usart_su_TXpacket[8] = 0x00;
                usart_su_TXpacket[9] = 0x00;
                usart_su_TXpacket[10] = 0x00;
                usart_su_TXpacket[11] = 0X55;
                usart_su_TXpacket[12] = 0XAA;
                // 发送反馈（UART_BUS_2与初始化一致）
                uapi_uart_write(UART_BUS_2, usart_su_TXpacket, 13, 0);
                printf("Send temp: %d.%d ℃\n", dht_temp_data.temp_high8bit, dht_temp_data.temp_low8bit);
            } else {
                printf("Read DHT11 temperature failed!\n");
            }
            break;
        
        case 0x0B: // 询问当前湿度
            printf("Voice cmd: Query humidity\r\n");
            DHT11_Data_TypeDef dht_humi_data;
            if (dht11_read_data(&dht_humi_data) == ERRCODE_SUCC) {
                // 湿度反馈数据包组装（对应原shidu=humi_high8bit，shidul=humi_low8bit）
                usart_su_TXpacket[0] = 0XAA;
                usart_su_TXpacket[1] = 0X55;
                usart_su_TXpacket[2] = 0x04;
                usart_su_TXpacket[3] = dht_humi_data.humi_high8bit; // 湿度整数部分
                usart_su_TXpacket[4] = 0x00;
                usart_su_TXpacket[5] = 0x00;
                usart_su_TXpacket[6] = 0x00;
                usart_su_TXpacket[7] = dht_humi_data.humi_low8bit;  // 湿度小数部分
                usart_su_TXpacket[8] = 0x00;
                usart_su_TXpacket[9] = 0x00;
                usart_su_TXpacket[10] = 0x00;
                usart_su_TXpacket[11] = 0X55;
                usart_su_TXpacket[12] = 0XAA;
                uapi_uart_write(UART_BUS_2, usart_su_TXpacket, 13, 0);
                printf("Send humidity: %d.%d%  %\n", dht_humi_data.humi_high8bit, dht_humi_data.humi_low8bit);
            }else {
                printf("Read DHT11 humirature failed!\n");
            }
            break;
        
        case 0x0C: // 询问当前湿度
            printf("Voice cmd: Query dist\r\n");
            distance = hcsr04_get_distance();
            if (distance >0) {
                // 湿度反馈数据包组装（对应原shidu=humi_high8bit，shidul=humi_low8bit）
                usart_su_TXpacket[0] = 0XAA;
                usart_su_TXpacket[1] = 0X55;
                usart_su_TXpacket[2] = 0x03;
                usart_su_TXpacket[3] = distance & 0xFF;  // 距离数据低位字节
                usart_su_TXpacket[4] = (distance >> 8) & 0xFF;         // 距离数据高位字节
                usart_su_TXpacket[5] = 0x00;
                usart_su_TXpacket[6] = 0x00;
                usart_su_TXpacket[7] = 0X55;
                usart_su_TXpacket[8] = 0XAA;
                uapi_uart_write(UART_BUS_2, usart_su_TXpacket, 9, 0);
                printf("Send dist: %d  mm\n", distance);
            }else {
                printf("Read Hcsr04 failed!\n");
            }
            break;
        // 可根据需求启用其他注释指令（如距离、亮度查询、洗衣机控制等）
        default:
            printf("Unknown voice cmd: 0x%02x\n", temp);
            break;
        
    }
}

/**
 * @brief UART中断接收回调函数
 */
#if (UART_INT_MODE)
void uart_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    if (buffer == NULL || length == 0 || length > UART_RECV_SIZE) {
        return;
    }
    
    // 使用memcpy替代memcpy_s，避免兼容性问题
    memcpy(uart_recv, buffer, length);
    uart_rx_flag = 1; // 置位接收完成标志
}
#endif

/**
 * @brief UART语音控制任务主函数
 */
void *uart_voice_task(const char *arg)
{
    unused(arg);
    
    // 注册中断回调
#if (UART_INT_MODE)
    if (uapi_uart_register_rx_callback(UART_BUS_2, 
                                      UART_RX_CONDITION_MASK_IDLE, 
                                      1, 
                                      uart_read_handler) != ERRCODE_SUCC) {
        printf("UART rx callback register failed!\n");
    }
#endif

    // 任务主循环
    while (1) {
#if (UART_INT_MODE)
        // 等待接收完成
        while (!uart_rx_flag) {
            osal_msleep(DELAY_TIME_MS);
        }
        printf("cgh get the char is %0x \r\n",uart_recv);
        uart_rx_flag = 0; // 清除标志
        voice_analysis(uart_recv); // 解析语音指令
        memset(uart_recv, 0, UART_RECV_SIZE); // 清空缓冲区
#else
        // 轮询模式
        uint16_t read_len = uapi_uart_read(UART_BUS_2, uart_recv, UART_RECV_SIZE, 100);
        if (read_len > 0) {
            voice_analysis(uart_recv);
            memset(uart_recv, 0, UART_RECV_SIZE);
        }
        osal_msleep(10);
#endif
    }
    return NULL;
}
