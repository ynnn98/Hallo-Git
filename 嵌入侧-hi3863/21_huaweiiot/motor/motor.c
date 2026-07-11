/*****************************************************************************************/
/*                                                                                       */
/*                  版权所有：沈阳市网联通信规划设计有限公司                                 */
/*                  开发人员：程国辉 刘艳                                                  */
/*                  联系方式：908536420  3512904489                                       */
/*                  文件名称：motor.c                                                    */
/*                  功能描述：TB6612电机驱动实现文件（风扇控制）                              */
/*                  开发时间：2025年11月                                                  */
/*                  本程序只供学习使用，未经作者许可，不得用于其它任何用途                    */
/*                  版本：V1.0                                                           */
/*                  版权所有，盗版必究                                                    */
/*                                                                                       */
/*****************************************************************************************/

#include "motor.h"
#include "pinctrl.h"
#include "gpio.h"
#include "stdio.h"
#include "../pwm/pwm_lamp.h"

static unsigned char g_motor_initialized = 0;

/**
 * @brief 设置电机方向控制引脚
 */
static void set_motor_direction(motor_direction_t direction)
{
    switch (direction) {
        case MOTOR_FORWARD:
            // AIN1=HIGH, AIN2=LOW - 正转（风扇开）
            uapi_gpio_set_val(MOTOR_AIN1_PIN, GPIO_LEVEL_HIGH);
            uapi_gpio_set_val(MOTOR_AIN2_PIN, GPIO_LEVEL_LOW);
            printf("Motor direction: FORWARD (Fan ON) \r\n");
            break;

        case MOTOR_BACKWARD:
            // AIN1=LOW, AIN2=HIGH - 反转
            uapi_gpio_set_val(MOTOR_AIN1_PIN, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(MOTOR_AIN2_PIN, GPIO_LEVEL_HIGH);
            printf("Motor direction: BACKWARD \r\n");
            break;

        case MOTOR_BRAKE:
            // AIN1=HIGH, AIN2=HIGH - 刹车
            uapi_gpio_set_val(MOTOR_AIN1_PIN, GPIO_LEVEL_HIGH);
            uapi_gpio_set_val(MOTOR_AIN2_PIN, GPIO_LEVEL_HIGH);
            printf("Motor direction: BRAKE \r\n");
            break;

        case MOTOR_STOP:
        default:
            // AIN1=LOW, AIN2=LOW - 停止（风扇关）
            uapi_gpio_set_val(MOTOR_AIN1_PIN, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(MOTOR_AIN2_PIN, GPIO_LEVEL_LOW);
            printf("Motor direction: STOP (Fan OFF) \r\n");
            break;
    }
}

/**
 * @brief 电机初始化函数
 */
int motor_init(void)
{
    if (g_motor_initialized) {
        printf("Motor already initialized \r\n");
        return 0;
    }

    printf("Initializing TB6612 motor driver (Fan Control)... \r\n");

    // 初始化PWM（使用PWM_LAMP模块）
    if (pwm_init_module() != ERRCODE_SUCC) {
        printf("Failed to initialize PWM for motor \r\n");
        return -1;
    }

    // 配置方向控制引脚
    uapi_pin_set_mode(MOTOR_AIN1_PIN, PIN_MODE_0);
    uapi_pin_set_mode(MOTOR_AIN2_PIN, PIN_MODE_0);

    uapi_gpio_set_dir(MOTOR_AIN1_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_dir(MOTOR_AIN2_PIN, GPIO_DIRECTION_OUTPUT);

    // 初始状态：停止
    set_motor_direction(MOTOR_STOP);

    g_motor_initialized = 1;
    printf("Motor (Fan) initialization successful \r\n");
    printf("AIN1: GPIO_%d, AIN2: GPIO_%d, PWMA: GPIO_%d (PWM%d) \r\n",
           MOTOR_AIN1_PIN, MOTOR_AIN2_PIN, MOTOR_PWMA_PIN, PWM_CHANNEL);
    return 0;
}

/**
 * @brief 设置电机速度和方向
 */
int motor_set_speed(uint8_t speed, motor_direction_t direction)
{
    if (!g_motor_initialized) {
        printf("Error: Motor not initialized \r\n");
        return -1;
    }

    if (speed > 100) {
        printf("Error: Speed must be 0-100 \r\n");
        return -1;
    }

    // 设置方向
    set_motor_direction(direction);

    if (direction == MOTOR_STOP || speed == 0) {
        // 停止状态，关闭PWM
        printf("Motor stopped \r\n");
    } else {
        // 设置PWM速度
        if (pwm_setup_output(1000, speed) != ERRCODE_SUCC) {
            printf("Failed to set motor speed \r\n");
            return -1;
        }

        printf("Motor speed set: %d%%, direction: %d \r\n", speed, direction);
    }

    return 0;
}

/**
 * @brief 电机停止
 */
void motor_stop(void)
{
    motor_set_speed(0, MOTOR_STOP);
}

/**
 * @brief 风扇开启（正转，默认80%速度）
 */
void fan_on(void)
{
    printf("=== Fan ON === \r\n");
    motor_set_speed(60, MOTOR_FORWARD);
}

/**
 * @brief 风扇关闭（停止）
 */
void fan_off(void)
{
    printf("=== Fan OFF === \r\n");
    motor_stop();
}

/**
 * @brief 电机演示任务
 */
void motor_demo_task(void)
{
    printf("\r\n=== TB6612 Motor (Fan) Control Demo === \r\n");

    if (motor_init() != 0) {
        printf("Motor initialization failed \r\n");
        return;
    }

    // 风扇控制序列
    printf("1. Fan ON (Forward at 80%% speed) \r\n");
    fan_on();
    osal_msleep(3000);

    printf("2. Fan OFF (Stop) \r\n");
    fan_off();
    osal_msleep(1000);

    printf("3. Fan ON again \r\n");
    fan_on();
    osal_msleep(3000);

    printf("4. Fan OFF \r\n");
    fan_off();

    printf("Fan demo completed \r\n");
}
