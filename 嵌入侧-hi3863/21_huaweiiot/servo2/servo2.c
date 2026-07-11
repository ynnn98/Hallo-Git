/*****************************************************************************************/
/*                                                                                       */
/*                  版权所有：沈阳市网联通信规划设计有限公司                                 */
/*                  开发人员：程国辉 刘艳                                                  */
/*                  联系方式：908536420  3512904489                                       */
/*                  文件名称：servo2.c                                                   */
/*                  功能描述：SG90 舵机驱动（WS63 移植版）                                  */
/*                                                                                       */
/*                  WS63 PWM 硬件限制:                                                    */
/*                    freq/duty 寄存器仅 16bit（max=65535）                              */
/*                    默认 DIV_6 下 50Hz 周期=133333 > 65535 → 无法输出                  */
/*                    改为 PWM3 独立 DIV_13: 40MHz/13=3.077MHz, 50Hz周期=61538 < 65535   */
/*                                                                                       */
/*                  开发时间：2026年7月                                                   */
/*                  版本：V1.0                                                           */
/*                  版权所有，盗版必究                                                    */
/*                                                                                       */
/*****************************************************************************************/

#include "servo2.h"
#include "pinctrl.h"
#include "pwm.h"
#include "gpio.h"
#include "soc_osal.h"
#include "app_init.h"
#include "stdio.h"
#include "string.h"

/* ---- 硬件映射 ---- */
#define SERVO_PWM_CHANNEL       3       /* PWM3                              */
#define SERVO_PWM_PIN           GPIO_03 /* GPIO_03                           */
#define SERVO_PWM_MODE          PIN_MODE_1

/* ---- PWM3 时钟分频器 ---- */
/* WS63 PWM 基频 = 40MHz, HAL 只用了 16bit freq 寄存器, max = 65535           */
/* 50Hz 周期需 800000 ticks > 65535 → 必须降频                               */
/* PWM3 分频器在 CLDO_CRG_DIV_CTL4 (0x44001118), DIV1_CFG[17:14] 4bit       */
/* 设 DIV_13: 40MHz/13 = 3.077MHz → 50Hz周期=61538 < 65535 ✓               */
#define CLDO_CRG_DIV_CTL4       0x44001118
#define PWM3_LOAD_DIV_EN        18
#define PWM3_DIV1_CFG           14
#define SERVO_DIV_VAL           13
#define PWM_BASE_CLK_HZ         40000000U
#define PWM_CLK_DIV_HZ          (PWM_BASE_CLK_HZ / SERVO_DIV_VAL)   /* 3,076,923 */
#define SERVO_FREQ_HZ           50U
#define PWM_TOTAL_TICKS         (PWM_CLK_DIV_HZ / SERVO_FREQ_HZ)    /* 61,538    */

/* ---- 寄存器读写辅助 ---- */
static inline void reg_write32(uint32_t addr, uint32_t val) {
    *(volatile uint32_t*)(uintptr_t)addr = val;
}
static inline uint32_t reg_read32(uint32_t addr) {
    return *(volatile uint32_t*)(uintptr_t)addr;
}

/* ---- SG90 角度-脉宽 ---- */
/* 0°→500µs  90°→1500µs  180°→2500µs                                 */
/* WS63 ticks = pulse_us × (CLK_DIV_HZ / 1e6) = pulse_us × 3.077      */

char DangGuangBan[4] = "OFF";
static uint8_t g_servo_inited = 0;

/* ================================================================ */
void Servo_Init(void)
{
    if (g_servo_inited) return;

    /* 1. 配置 GPIO_03 为 PWM 功能 */
    uapi_pin_set_mode(SERVO_PWM_PIN, SERVO_PWM_MODE);

    /* 2. 初始化 PWM 模块（motor_init 可能已初始化过，再次调用无害） */
    uapi_pwm_init();

    /* 3. 给 PWM3 单独配一个大的时钟分频器 DIV_13
     *    因为 50Hz @ 40MHz/DIV_6 = 133k > 65535(16bit) → 必须降频
     *    40MHz / 13 = 3.077MHz → 50Hz周期 = 61538 < 65535 ✓
     *    步骤: 关 load_en → 写 DIV=13 → 开 load_en */
    {
        uint32_t val = reg_read32(CLDO_CRG_DIV_CTL4);
        val &= ~(1u << PWM3_LOAD_DIV_EN);           /* 关 load_en      */
        reg_write32(CLDO_CRG_DIV_CTL4, val);

        val = reg_read32(CLDO_CRG_DIV_CTL4);
        val &= ~(0xFu << PWM3_DIV1_CFG);             /* 清零 DIV_CFG   */
        val |=  (SERVO_DIV_VAL & 0xFu) << PWM3_DIV1_CFG;  /* 写 DIV=13 */
        reg_write32(CLDO_CRG_DIV_CTL4, val);

        val |= (1u << PWM3_LOAD_DIV_EN);             /* 开 load_en     */
        reg_write32(CLDO_CRG_DIV_CTL4, val);
    }

    g_servo_inited = 1;

    /* 4. 启动 PWM，初始角度 0° */
    Door_OFF();

    printf("[SERVO] init: GPIO_%d PWM%d %dHz, clk=40M/%d=%.2fMHz, period=%d ticks\r\n",
           SERVO_PWM_PIN, SERVO_PWM_CHANNEL, SERVO_FREQ_HZ,
           SERVO_DIV_VAL, PWM_CLK_DIV_HZ / 1000000.0f, PWM_TOTAL_TICKS);
}

/* ================================================================ */
void Servo_SetAngle(float Angle)
{
    if (!g_servo_inited) return;
    if (Angle < 0.0f)   Angle = 0.0f;
    if (Angle > 180.0f) Angle = 180.0f;

    /*
     * SG90: pulse_us = 500 + Angle/180 × 2000    (500~2500µs)
     * WS63 ticks = pulse_us × (CLK_DIV_HZ / 1,000,000)
     *            = pulse_us × 3.076923
     */
    uint32_t pulse_us  = (uint32_t)(500.0f + (Angle / 180.0f) * 2000.0f);
    uint32_t high      = (uint32_t)(pulse_us * (PWM_CLK_DIV_HZ / 1000000.0f));
    uint32_t low       = PWM_TOTAL_TICKS - high;

    if (high >= PWM_TOTAL_TICKS) high = PWM_TOTAL_TICKS - 1;
    if (low  == 0)               low  = 1;

    /* close → configure → open → start */
    uapi_pwm_close(SERVO_PWM_CHANNEL);

    pwm_config_t cfg = {
        .low_time    = low,
        .high_time   = high,
        .offset_time = 0,
        .cycles      = 0,
        .repeat      = 1
    };

    errcode_t ret = uapi_pwm_open(SERVO_PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        printf("[SERVO] open failed: %d (low=%d high=%d)\r\n", ret, low, high);
        return;
    }

    ret = uapi_pwm_start(SERVO_PWM_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        printf("[SERVO] start failed: %d\r\n", ret);
        uapi_pwm_close(SERVO_PWM_CHANNEL);
        return;
    }
}

/* ================================================================ */
void Door_ON(void)
{
    Servo_SetAngle(120);
    strcpy(DangGuangBan, "ON");
    printf("[SERVO] DangGuangBan ON (120 deg)\r\n");
}

void Door_OFF(void)
{
    Servo_SetAngle(0);
    strcpy(DangGuangBan, "OFF");
    printf("[SERVO] DangGuangBan OFF (0 deg)\r\n");
}
