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
#include "led/led.h"
#include "dht11/dht11.h"
#include "hcsr04/hcsr04.h"
#include "voice/voice.h"


// 任务栈大小和优先级
#define UART_TASK_STACK_SIZE 0x1000
#define UART_TASK_PRIO 28

/**
 * @brief 应用入口函数（按osal_kthread_create风格创建任务）
 */
static void uart_voice_entry(void)
{
    printf("UART voice control entry!\r\n");

    osal_task *task_handle = NULL;
    osal_kthread_lock();
    
    // 1. 初始化硬件
    // 这个初始化函数应该包含 UART, GPIO, LED, DHT11, HCSR04 等所有必要的初始化
     // 硬件初始化
    uart_gpio_init();
    uart_init_config();
    
    led_init();
    dht11_init();
    hcsr04_init();
    printf("Hardware initialization completed.\r\n");
    
    // 2. 创建UART语音控制任务
    printf("Creating UartVoiceTask...\r\n");
    task_handle = osal_kthread_create((osal_kthread_handler)uart_voice_task,
                                      0,
                                      "UartVoiceTask",
                                      UART_TASK_STACK_SIZE);
    
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, UART_TASK_PRIO);
        printf("UartVoiceTask created successfully!\n");
    } else {
        printf("UartVoiceTask create failed!\n");
    }
    
    osal_kthread_unlock();
}

// 注册应用入口
app_run(uart_voice_entry);