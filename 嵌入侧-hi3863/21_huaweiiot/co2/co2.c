/*****************************************************************************************/
/*                                                                                       */
/*                  版权所有：沈阳市网联通信规划设计有限公司                                 */
/*                  开发人员：程国辉 刘艳                                                  */
/*                  联系方式：908536420  3512904489                                       */
/*                  文件名称：co2.c                                                      */
/*                  功能描述：JW01/TVOC-301 空气质量传感器驱动实现文件                     */
/*                           支持硬件 UART 和软件 UART（GPIO 位冲）两种模式              */
/*                  协议：主动上报，每1秒发9字节帧                                        */
/*                        [0]=0x2C [1]=0xE4 [2-3]=TVOC [4-5]=HCHO [6-7]=eCO2 [8]=CS       */
/*                  软件 UART：9600 波特率，GPIO 中断 + 延时采样                         */
/*                  开发时间：2026年7月                                                   */
/*                  版本：V2.0                                                           */
/*                  版权所有，盗版必究                                                    */
/*                                                                                       */
/*****************************************************************************************/

#include "co2.h"
#include "soc_osal.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "tcxo.h"          /* uapi_tcxo_get_us()——硬件级微秒时间戳 */

#ifndef unused
#define unused(var)     ((void)(var))
#endif

/* ====== 全局变量 ====== */
volatile uint16_t g_co2_tvoc = 0;
volatile uint16_t g_co2_formaldehyde = 0;
volatile uint16_t g_co2_eco2 = 0;
volatile uint8_t  g_co2_data_ready = 0;

/* ====== 内部变量 ====== */
static bool g_co2_inited = false;
static bool g_co2_sw_mode = false;  /* true=软件UART, false=硬件UART */
static pin_t g_co2_sw_pin = CO2_SW_PIN_DEFAULT;

/* UART 接收缓存（硬件 UART 模式用） */
static uint8_t g_co2_rx_cache[64];
static uart_buffer_config_t g_co2_uart_buffer_config = {
    .rx_buffer = g_co2_rx_cache,
    .rx_buffer_size = sizeof(g_co2_rx_cache)
};

/* 帧接收状态 */
static uint8_t g_co2_frame[CO2_FRAME_LEN];
static uint8_t g_co2_frame_idx = 0;

/* ====== 软件 UART（TCXO 时间戳驱动） ====== */
/* 使用 uapi_tcxo_get_us() 做精确微秒延时，无累积误差 */
#define CO2_SW_BAUDRATE         9600
#define CO2_SW_BIT_US           (1000000 / CO2_SW_BAUDRATE)   /* 104μs per bit */
#define CO2_SW_HALF_BIT_US      (CO2_SW_BIT_US / 2)           /* 52μs half bit */
#define CO2_SW_DEBUG            1       /* 打印每个接收到的字节 */

/* 前向声明 */
static void co2_input_byte(uint8_t data);

/* 防重入标记（不用注册/注销中断，避免耗时）*/
static volatile uint8_t g_co2_sw_busy = 0;

/**
 * @brief 微秒级忙等延时（基于 uapi_tcxo_get_us() 硬件时间戳）
 * @param start_us 起始时间戳
 * @param delay_us 需要延时的微秒数
 */
static inline void co2_wait_us(uint64_t start_us, uint32_t delay_us)
{
    uint64_t target = start_us + delay_us;
    while (uapi_tcxo_get_us() < target) { }
}

/**
 * @brief GPIO 中断回调（软件 UART 模式）
 *
 * WS63 SDK 只有双边沿中断(DEDGE)，ISR 中判断电平，
 * 只在下降沿（起始位）处理。
 * 使用 uapi_tcxo_get_us() 硬件时间戳做精确位采样。
 * 用 volatile 防重入标记代替注册/注销中断，避免耗时。
 */
static void co2_sw_isr_callback(pin_t pin, uintptr_t param)
{
    uint8_t byte = 0;

    unused(param);
    if (pin != g_co2_sw_pin) return;
    if (g_co2_sw_busy) return;  /* 正在接收中，忽略重复触发 */
    if (uapi_gpio_get_val(g_co2_sw_pin) != GPIO_LEVEL_LOW) return;  /* 上升沿忽略 */

    g_co2_sw_busy = 1;

    /* 记录起始时间（下降沿 = 起始位开始）*/
    uint64_t t0 = uapi_tcxo_get_us();

    /* 等待半个 bit 周期，到达起始位中点 */
    co2_wait_us(t0, CO2_SW_HALF_BIT_US);

    /* 再次确认仍是低电平（有效起始位）*/
    if (uapi_gpio_get_val(g_co2_sw_pin) != GPIO_LEVEL_LOW) {
        g_co2_sw_busy = 0;
        return;
    }

    /* 采样 8 个数据位，LSB 优先，每 bit 间隔 104μs */
    for (int i = 0; i < 8; i++) {
        co2_wait_us(t0, CO2_SW_HALF_BIT_US + (uint32_t)(i + 1) * CO2_SW_BIT_US);
        if (uapi_gpio_get_val(g_co2_sw_pin) == GPIO_LEVEL_HIGH) {
            byte |= (uint8_t)(1 << i);
        }
    }

    /* 等待停止位结束 */
    co2_wait_us(t0, CO2_SW_HALF_BIT_US + 9 * CO2_SW_BIT_US);

#if CO2_SW_DEBUG
    printf("[CO2_SW] raw byte: 0x%02x (%c)\r\n",
           byte, (byte >= 0x20 && byte < 0x7f) ? byte : '.');
#endif

    /* 喂给帧解析器 */
    co2_input_byte(byte);

    g_co2_sw_busy = 0;
}

/**
 * @brief 计算校验和：前8字节累加
 */
static uint8_t co2_checksum(const uint8_t *data, uint8_t len)
{
    uint8_t sum = 0;
    if (data == NULL) return 0;
    for (uint8_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + data[i]);
    }
    return sum;
}

/**
 * @brief 重置帧接收状态
 */
static void co2_reset_frame(void)
{
    g_co2_frame_idx = 0;
    (void)memset(g_co2_frame, 0, sizeof(g_co2_frame));
}

/**
 * @brief 解析一帧完整数据
 */
static void co2_parse_frame(void)
{
    /* 校验帧头 */
    if (g_co2_frame[0] != CO2_FRAME_HEAD_0 || g_co2_frame[1] != CO2_FRAME_HEAD_1) {
        return;
    }

    /* 校验校验和 */
    uint8_t cs = co2_checksum(g_co2_frame, CO2_FRAME_LEN - 1);
    if (cs != g_co2_frame[CO2_FRAME_LEN - 1]) {
        printf("[CO2] checksum error: calc=0x%02x, recv=0x%02x\r\n",
               cs, g_co2_frame[CO2_FRAME_LEN - 1]);
        return;
    }

    /* 解析数据 */
    g_co2_tvoc = (uint16_t)(((uint16_t)g_co2_frame[2] << 8) | g_co2_frame[3]);
    g_co2_formaldehyde = (uint16_t)(((uint16_t)g_co2_frame[4] << 8) | g_co2_frame[5]);
    g_co2_eco2 = (uint16_t)(((uint16_t)g_co2_frame[6] << 8) | g_co2_frame[7]);
    g_co2_data_ready = 1;

    printf("[CO2] TVOC=%d ug/m3, HCHO=%d ug/m3, eCO2=%d ppm\r\n",
           g_co2_tvoc, g_co2_formaldehyde, g_co2_eco2);
}

/**
 * @brief 逐字节输入到帧解析器
 */
static void co2_input_byte(uint8_t data)
{
    /* 等待帧头 0x2C */
    if (g_co2_frame_idx == 0) {
        if (data != CO2_FRAME_HEAD_0) {
            return;
        }
        g_co2_frame[g_co2_frame_idx++] = data;
        return;
    }

    /* 第二字节：必须为 0xE4；若又收到 0x2C 则重新开始 */
    if (g_co2_frame_idx == 1) {
        if (data == CO2_FRAME_HEAD_1) {
            g_co2_frame[g_co2_frame_idx++] = data;
        } else if (data == CO2_FRAME_HEAD_0) {
            g_co2_frame[0] = data;
            g_co2_frame_idx = 1;
        } else {
            co2_reset_frame();
        }
        return;
    }

    /* 填充剩余字节 */
    if (g_co2_frame_idx < CO2_FRAME_LEN) {
        g_co2_frame[g_co2_frame_idx++] = data;
    }

    /* 帧满 → 解析 */
    if (g_co2_frame_idx >= CO2_FRAME_LEN) {
        co2_parse_frame();
        co2_reset_frame();
    }
}

/* ====================================================================
 *  模式1：硬件 UART（UART1）
 * ==================================================================== */

static void co2_uart_rx_handler(const void *buffer, uint16_t length, bool error)
{
    if (error || buffer == NULL || length == 0) {
        return;
    }

    const uint8_t *data = (const uint8_t *)buffer;
    for (uint16_t i = 0; i < length; i++) {
        co2_input_byte(data[i]);
    }
}

static void co2_uart_pin_init(void)
{
#if defined(CONFIG_PINCTRL_SUPPORT_IE)
    uapi_pin_set_ie(S_AGPIO16, PIN_IE_1);
#endif
    /* TX (GPIO15) 不配置——CO2 传感器不收数据 */
    uapi_pin_set_mode(S_AGPIO16, PIN_MODE_1);
}

/* ====================================================================
 *  初始化 & 控制接口
 * ==================================================================== */

errcode_t co2_init(uint32_t baudrate)
{
    if (g_co2_inited) {
        printf("[CO2] already initialized\r\n");
        return ERRCODE_SUCC;
    }

    g_co2_sw_mode = false;

    uart_attr_t attr = {
        .baud_rate = (baudrate == 0) ? 9600 : baudrate,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };
    uart_pin_config_t pin_config = {
        .tx_pin = S_AGPIO15,     /* 必须配一个有效引脚（悬空不接线）*/
        .rx_pin = S_AGPIO16,     /* RX1 飞线 = UART1_RXD */
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };

    co2_reset_frame();
    g_co2_tvoc = 0;
    g_co2_formaldehyde = 0;
    g_co2_eco2 = 0;
    g_co2_data_ready = 0;

    co2_uart_pin_init();

    (void)uapi_uart_unregister_rx_callback(UART_BUS_1);
    (void)uapi_uart_deinit(UART_BUS_1);

    errcode_t ret = uapi_uart_init(UART_BUS_1, &pin_config, &attr, NULL, &g_co2_uart_buffer_config);
    if (ret != ERRCODE_SUCC) {
        printf("[CO2] uart init failed, ret=%d\r\n", ret);
        return ret;
    }

    ret = uapi_uart_register_rx_callback(UART_BUS_1,
                                         UART_RX_CONDITION_FULL_OR_SUFFICIENT_DATA_OR_IDLE,
                                         1,
                                         co2_uart_rx_handler);
    if (ret != ERRCODE_SUCC) {
        printf("[CO2] rx callback failed, ret=%d\r\n", ret);
        (void)uapi_uart_deinit(UART_BUS_1);
        return ret;
    }

    g_co2_inited = true;
    printf("[CO2] HW UART mode: UART_BUS_1 @%d\r\n", attr.baud_rate);
    return ERRCODE_SUCC;
}

errcode_t co2_init_sw(pin_t rx_pin)
{
    if (g_co2_inited) {
        printf("[CO2] already initialized\r\n");
        return ERRCODE_SUCC;
    }

    g_co2_sw_mode = true;
    g_co2_sw_pin = rx_pin;

    co2_reset_frame();
    g_co2_tvoc = 0;
    g_co2_formaldehyde = 0;
    g_co2_eco2 = 0;
    g_co2_data_ready = 0;

    /* 配置引脚为 GPIO 输入，上拉 */
    uapi_pin_set_mode(g_co2_sw_pin, PIN_MODE_0);
    uapi_gpio_set_dir(g_co2_sw_pin, GPIO_DIRECTION_INPUT);
    uapi_pin_set_pull(g_co2_sw_pin, PIN_PULL_TYPE_UP);

    /* 注册下降沿中断（SDK 用双边沿 DEDGE，在 ISR 中判断是下降沿才处理） */
    uapi_gpio_unregister_isr_func(g_co2_sw_pin);
    errcode_t ret = uapi_gpio_register_isr_func(g_co2_sw_pin,
                                                 GPIO_INTERRUPT_DEDGE,
                                                 co2_sw_isr_callback);
    if (ret != ERRCODE_SUCC) {
        printf("[CO2] sw uart isr register failed, ret=%d\r\n", ret);
        return ret;
    }

    g_co2_inited = true;
    printf("[CO2] SW UART mode: pin=GPIO_%d @%d, using tcxo_get_us\r\n",
           rx_pin, CO2_SW_BAUDRATE);
    return ERRCODE_SUCC;
}

errcode_t co2_deinit(void)
{
    if (!g_co2_inited) {
        return ERRCODE_SUCC;
    }

    if (g_co2_sw_mode) {
        uapi_gpio_unregister_isr_func(g_co2_sw_pin);
        uapi_pin_set_mode(g_co2_sw_pin, PIN_MODE_0);
    } else {
        (void)uapi_uart_unregister_rx_callback(UART_BUS_1);
        (void)uapi_uart_deinit(UART_BUS_1);
    }

    g_co2_inited = false;
    co2_reset_frame();
    printf("[CO2] deinit\r\n");
    return ERRCODE_SUCC;
}

/* ====== 数据读取接口 ====== */

uint16_t co2_get_tvoc(void)
{
    return g_co2_tvoc;
}

uint16_t co2_get_formaldehyde(void)
{
    return g_co2_formaldehyde;
}

uint16_t co2_get_eco2(void)
{
    return g_co2_eco2;
}

uint8_t co2_is_data_ready(void)
{
    return g_co2_data_ready;
}

int co2_get_data(CO2_Data_TypeDef *data)
{
    if (data == NULL) return -1;
    data->tvoc = g_co2_tvoc;
    data->formaldehyde = g_co2_formaldehyde;
    data->eco2 = g_co2_eco2;
    data->data_ready = g_co2_data_ready;
    return 0;
}

void co2_clear_flag(void)
{
    g_co2_data_ready = 0;
}
