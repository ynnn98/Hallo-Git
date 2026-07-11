/*****************************************************************************************/
/*                                                                                       */
/*                  版权所有：沈阳市网联通信规划设计有限公司                                 */
/*                  开发人员：程国辉 刘艳                                                  */
/*                  联系方式：908536420  3512904489                                       */
/*                  文件名称：motor.h                                                    */
/*                  功能描述：TB6612电机驱动头文件（风扇控制）                               */
/*                  开发时间：2025年11月                                                  */
/*                  本程序只供学习使用，未经作者许可，不得用于其它任何用途                    */
/*                  版本：V1.0                                                           */
/*                  版权所有，盗版必究                                                    */
/*                                                                                       */
/*****************************************************************************************/

#ifndef __MOTOR_H__
#define __MOTOR_H__

#include "common_def.h"
#include "soc_osal.h"

// TB6612控制引脚定义（GPIO_02 = PWM2，09-pwm原版验证通过）
#define MOTOR_PWMA_PIN             2       // GPIO_02 - PWM速度控制（风扇转速）
#define MOTOR_AIN1_PIN             10      // GPIO_10 - 方向控制1
#define MOTOR_AIN2_PIN             11      // GPIO_11 - 方向控制2

// 电机方向定义
typedef enum {
    MOTOR_STOP = 0,     // 停止
    MOTOR_FORWARD,      // 正转（风扇开）
    MOTOR_BACKWARD,     // 反转
    MOTOR_BRAKE         // 刹车
} motor_direction_t;

/**
 * @brief 电机初始化函数
 * @return int 成功返回0，失败返回-1
 */
int motor_init(void);

/**
 * @brief 设置电机速度和方向
 * @param speed 速度(0-100)
 * @param direction 方向
 * @return int 成功返回0，失败返回-1
 */
int motor_set_speed(uint8_t speed, motor_direction_t direction);

/**
 * @brief 电机停止
 */
void motor_stop(void);

/**
 * @brief 风扇开启（正转，默认80%速度）
 */
void fan_on(void);

/**
 * @brief 风扇关闭（停止）
 */
void fan_off(void);

/**
 * @brief 电机演示任务
 */
void motor_demo_task(void);

#endif /* __MOTOR_H__ */
