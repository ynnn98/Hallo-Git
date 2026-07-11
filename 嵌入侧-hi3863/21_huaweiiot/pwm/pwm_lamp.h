/*****************************************************************************************/
/*                                                                                       */
/*                  版权所有：沈阳市网联通信规划设计有限公司                                 */
/*                  开发人员：程国辉 刘艳                                                  */
/*                  联系方式：908536420  3512904489                                       */
/*                  文件名称：pwm_lamp.h                                                 */
/*                  功能描述：PWM驱动头文件（风扇电机PWM控制）                               */
/*                  开发时间：2025年11月                                                  */
/*                  本程序只供学习使用，未经作者许可，不得用于其它任何用途                    */
/*                  版本：V1.0                                                           */
/*                  版权所有，盗版必究                                                    */
/*                                                                                       */
/*****************************************************************************************/

#ifndef __PWM_LAMP_H__
#define __PWM_LAMP_H__

#if defined(CONFIG_PWM_SUPPORT_LPM)
#include "pm_veto.h"
#endif
#include "common_def.h"
#include "pinctrl.h"
#include "pwm.h"
#include "gpio.h"
#include "tcxo.h"
#include "soc_osal.h"
#include "app_init.h"

//#define CONFIG_PWM_USING_V151 1

#define TEST_TCXO_DELAY_20MS       20
#define TEST_TCXO_DELAY_50MS       50
#define TEST_TCXO_DELAY_100MS      100
#define TEST_TCXO_DELAY_1000MS     1000

#define PWM_TASK_PRIO              24
#define PWM_TASK_STACK_SIZE        0x1000

// PWM配置 — GPIO_02 = PWM2（09-pwm原版验证通过，不可随意改通道）
#define PWM_CHANNEL                2       // PWM2通道（对应GPIO_02）
#define PWM_GROUP_ID               0       // 组ID
#define PWM_PIN                    2       // GPIO_02
#define PWM_PIN_MODE               1       // PWM功能模式

// PWM时钟参数
#define PWM_BASE_CLOCK_HZ          24000000 // PWM时钟通常为24MHz
#define PWM_DESIRED_FREQ_HZ        1000    // PWM频率1kHz

// 呼吸灯参数
#define BREATH_MIN_DUTY            5       // 最暗时占空比 5%
#define BREATH_MAX_DUTY            95      // 最亮时占空比 95%
#define BREATH_STEP                1       // 呼吸步长
#define BREATH_CYCLE_COUNT         2       // 呼吸循环次数

/**
 * @brief PWM初始化
 * @return errcode_t 错误码
 */
errcode_t pwm_init_module(void);

/**
 * @brief 配置并启动PWM输出
 * @param freq 频率
 * @param duty 占空比
 * @return errcode_t 错误码
 */
errcode_t pwm_setup_output(uint32_t freq, uint8_t duty);

/**
 * @brief 测试PWM固定占空比
 */
void pwm_test_fixed_duty(void);

/**
 * @brief 启动呼吸灯效果
 */
void pwm_start_breathing_effect(void);

/**
 * @brief 停止呼吸灯效果
 */
void pwm_stop_breathing_effect(void);

/**
 * @brief 清理PWM资源
 */
void pwm_cleanup(void);

/**
 * @brief 打印PWM配置信息
 */
void pwm_print_info(void);

#endif /* __PWM_LAMP_H__ */
