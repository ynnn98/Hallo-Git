/*****************************************************************************************/
/*                                                                                       */
/*                  版权所有：沈阳市网联通信规划设计有限公司                                 */
/*                  开发人员：程国辉 刘艳                                                  */
/*                  联系方式：908536420  3512904489                                       */
/*                  文件名称：pwm_lamp.c                                                 */
/*                  功能描述：PWM驱动实现文件（风扇电机PWM控制）                               */
/*                  开发时间：2025年11月                                                  */
/*                  本程序只供学习使用，未经作者许可，不得用于其它任何用途                    */
/*                  版本：V1.0                                                           */
/*                  版权所有，盗版必究                                                    */
/*                                                                                       */
/*****************************************************************************************/

#include "stdio.h"
#include "pwm_lamp.h"

static unsigned char g_pwm_initialized = 0;
static unsigned char g_breathing = 1;

/**
 * @brief PWM中断回调函数
 */
static errcode_t pwm_sample_callback(uint8_t channel)
{
    osal_printk("PWM %d cycle completed \r\n", channel);
    return ERRCODE_SUCC;
}

/**
 * @brief 打印PWM配置信息
 */
void pwm_print_info(void)
{
    osal_printk("=== WS63 PWM Configuration === \r\n");
    osal_printk("PWM Channel: %d (PWM%d) \r\n", PWM_CHANNEL, PWM_CHANNEL);
    osal_printk("PWM Group: %d \r\n", PWM_GROUP_ID);
    osal_printk("GPIO Pin: GPIO_%d \r\n", PWM_PIN);
    osal_printk("Pin Mode: %d \r\n", PWM_PIN_MODE);
    osal_printk("Base Clock: %d Hz \r\n", PWM_BASE_CLOCK_HZ);
    osal_printk("Target Frequency: %d Hz \r\n", PWM_DESIRED_FREQ_HZ);
    osal_printk("================================ \r\n");
}

/**
 * @brief 计算PWM高低电平时间
 */
static errcode_t calculate_pwm_timing(uint32_t freq, uint8_t duty,
                                     uint32_t *low_time, uint32_t *high_time)
{
    if (freq == 0) {
        osal_printk("Error: PWM frequency cannot be 0 \r\n");
        return ERRCODE_FAIL;
    }

    uint32_t total_cycles = PWM_BASE_CLOCK_HZ / freq;

    // 检查周期数是否在有效范围内
    if (total_cycles < 2) {
        osal_printk("Warning: Total cycles too small, adjusting frequency \r\n");
        total_cycles = 2;
    }
    if (total_cycles > 0xFFFF) {
        osal_printk("Warning: Total cycles too large, adjusting frequency \r\n");
        total_cycles = 0xFFFF;
    }

    uint32_t high_cycles = (total_cycles * duty) / 100;
    uint32_t low_cycles = total_cycles - high_cycles;

    // 确保至少有一个时钟周期
    if (high_cycles == 0) high_cycles = 1;
    if (low_cycles == 0) low_cycles = 1;

    *high_time = high_cycles;
    *low_time = low_cycles;

    return ERRCODE_SUCC;
}

/**
 * @brief 初始化PWM分组
 */
errcode_t init_pwm_group(void)
{
#if defined(CONFIG_PWM_USING_V151)
    uint8_t channel_set[] = {PWM_CHANNEL};
    uint32_t channel_set_len = sizeof(channel_set) / sizeof(channel_set[0]);

    errcode_t ret = uapi_pwm_set_group(PWM_GROUP_ID, channel_set, channel_set_len);
    if (ret != ERRCODE_SUCC) {
        osal_printk("Error: uapi_pwm_set_group failed: %d \r\n", ret);
        return ret;
    }
    osal_printk("PWM group %d configured with channel %d \r\n", PWM_GROUP_ID, PWM_CHANNEL);
#endif
    return ERRCODE_SUCC;
}

/**
 * @brief 配置并启动PWM
 */
errcode_t setup_pwm(uint32_t freq, uint8_t duty)
{
    uint32_t low_time, high_time;
    errcode_t ret;

    // 计算PWM时序
    ret = calculate_pwm_timing(freq, duty, &low_time, &high_time);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    // 配置PWM参数
    pwm_config_t cfg = {
        .low_time = low_time,
        .high_time = high_time,
        .offset_time = 0,
        .cycles = 0,        // 0表示连续输出
        .repeat = 1         // 重复模式
    };

    // 关闭之前的PWM（如果已打开）
    if (g_pwm_initialized) {
        uapi_pwm_close(PWM_CHANNEL);
    }

    // 打开PWM
    ret = uapi_pwm_open(PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        osal_printk("Error: uapi_pwm_open failed: %d \r\n", ret);
        return ret;
    }

    // 启动PWM
    ret = uapi_pwm_start(PWM_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        osal_printk("Error: uapi_pwm_start failed: %d \r\n", ret);
        uapi_pwm_close(PWM_CHANNEL);
        return ret;
    }

    g_pwm_initialized = 1;
    return ERRCODE_SUCC;
}

/**
 * @brief 使用分组启动PWM（V151版本备用方案）
 */
errcode_t setup_pwm_with_group(uint32_t freq, uint8_t duty)
{
    uint32_t low_time, high_time;
    errcode_t ret;

    // 计算PWM时序
    ret = calculate_pwm_timing(freq, duty, &low_time, &high_time);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    // 配置PWM参数
    pwm_config_t cfg = {
        .low_time = low_time,
        .high_time = high_time,
        .offset_time = 0,
        .cycles = 0,        // 0表示连续输出
        .repeat = 1         // 重复模式
    };

    // 关闭之前的PWM
    if (g_pwm_initialized) {
        uapi_pwm_close(PWM_CHANNEL);
    }

    // 打开PWM
    ret = uapi_pwm_open(PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        osal_printk("Error: uapi_pwm_open failed: %d \r\n", ret);
        return ret;
    }

#if defined(CONFIG_PWM_USING_V151)
    // 使用分组启动
    ret = uapi_pwm_start_group(PWM_GROUP_ID);
    if (ret != ERRCODE_SUCC) {
        osal_printk("Error: uapi_pwm_start_group failed: %d \r\n", ret);
        uapi_pwm_close(PWM_CHANNEL);
        return ret;
    }
    osal_printk("PWM started using group %d \r\n", PWM_GROUP_ID);
#else
    // 使用普通启动
    ret = uapi_pwm_start(PWM_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        osal_printk("Error: uapi_pwm_start failed: %d \r\n", ret);
        uapi_pwm_close(PWM_CHANNEL);
        return ret;
    }
#endif

    g_pwm_initialized = 1;
    return ERRCODE_SUCC;
}

/**
 * @brief 测试PWM固定占空比
 */
void pwm_test_fixed_duty(void)
{
    osal_printk("=== Testing PWM Fixed Duty Cycles === \r\n");

    uint8_t test_duties[] = {10, 25, 50, 75, 90};
    uint8_t test_count = sizeof(test_duties) / sizeof(test_duties[0]);

    for (int i = 0; i < test_count; i++) {
        osal_printk("Testing %d%% duty cycle \r\n", test_duties[i]);

        // 先尝试普通方法
        errcode_t ret = setup_pwm(PWM_DESIRED_FREQ_HZ, test_duties[i]);
        if (ret != ERRCODE_SUCC) {
            osal_printk("Normal method failed, trying group method... \r\n");
            // 如果普通方法失败，尝试分组方法
            ret = setup_pwm_with_group(PWM_DESIRED_FREQ_HZ, test_duties[i]);
        }

        if (ret == ERRCODE_SUCC) {
            osal_printk("PWM started successfully with %d%% duty \r\n", test_duties[i]);
        } else {
            osal_printk("Failed to start PWM with %d%% duty \r\n", test_duties[i]);
        }

        uapi_tcxo_delay_ms(1500);
    }

    osal_printk("PWM Fixed Duty Test Completed \r\n");
}

/**
 * @brief 呼吸灯效果实现
 */
static void breathing_led_effect(void)
{
    osal_printk("=== Starting Breathing LED Effect === \r\n");

    uint8_t current_duty = BREATH_MIN_DUTY;
    unsigned char increasing = 1;
    uint32_t cycle_count = 0;

    // 初始设置
    errcode_t ret = setup_pwm(PWM_DESIRED_FREQ_HZ, current_duty);
    if (ret != ERRCODE_SUCC) {
        osal_printk("Normal method failed, trying group method for breathing... \r\n");
        ret = setup_pwm_with_group(PWM_DESIRED_FREQ_HZ, current_duty);
    }

    if (ret != ERRCODE_SUCC) {
        osal_printk("Error: Failed to setup initial PWM \r\n");
        return;
    }

    osal_printk("Breathing effect started - PWM on GPIO_%d \r\n", PWM_PIN);

    while (g_breathing && cycle_count < BREATH_CYCLE_COUNT) {
        // 更新占空比
        if (increasing) {
            current_duty += BREATH_STEP;
            if (current_duty >= BREATH_MAX_DUTY) {
                current_duty = BREATH_MAX_DUTY;
                increasing = 0;
                osal_printk("Reached maximum brightness \r\n");
            }
        } else {
            current_duty -= BREATH_STEP;
            if (current_duty <= BREATH_MIN_DUTY) {
                current_duty = BREATH_MIN_DUTY;
                increasing = 1;
                cycle_count++;
                osal_printk("Breathing cycle %d completed \r\n", cycle_count);
            }
        }

        // 设置新的占空比
        setup_pwm(PWM_DESIRED_FREQ_HZ, current_duty);
        osal_msleep(50);
    }

    osal_printk("Breathing effect completed \r\n");
}

/**
 * @brief PWM初始化
 */
errcode_t pwm_init_module(void)
{
    osal_printk("\r\n--- Step 1: Configuring PWM Pin --- \r\n");
    osal_printk("Setting GPIO_%d to PWM mode %d \r\n", PWM_PIN, PWM_PIN_MODE);
    uapi_pin_set_mode(PWM_PIN, PWM_PIN_MODE);

    osal_printk("\r\n--- Step 2: Initializing PWM Module --- \r\n");
    uapi_pwm_deinit();  // 确保先清理
    errcode_t init_ret = uapi_pwm_init();
    if (init_ret != ERRCODE_SUCC) {
        osal_printk("Error: PWM initialization failed: %d \r\n", init_ret);
        return init_ret;
    }
    osal_printk("PWM initialization successful \r\n");

    osal_printk("\r\n--- Step 3: Initializing PWM Group --- \r\n");
    init_pwm_group();

    osal_printk("\r\n--- Step 4: Registering PWM Interrupt --- \r\n");
    uapi_pwm_unregister_interrupt(PWM_CHANNEL);
    uapi_pwm_register_interrupt(PWM_CHANNEL, pwm_sample_callback);

    g_pwm_initialized = 1;
    return ERRCODE_SUCC;
}

/**
 * @brief 启动呼吸灯效果
 */
void pwm_start_breathing_effect(void)
{
    breathing_led_effect();
}

/**
 * @brief 停止呼吸灯效果
 */
void pwm_stop_breathing_effect(void)
{
    g_breathing = 0;
}

/**
 * @brief 清理PWM资源
 */
void pwm_cleanup(void)
{
    osal_printk("\r\n--- Cleaning Up --- \r\n");
    uapi_pwm_close(PWM_CHANNEL);
#if defined(CONFIG_PWM_USING_V151)
    uapi_pwm_clear_group(PWM_GROUP_ID);
#endif
    uapi_tcxo_delay_ms(100);
    uapi_pwm_deinit();

    g_pwm_initialized = 0;
    g_breathing = 0;
}

/**
 * @brief 配置并启动PWM输出（供motor模块调用）
 */
errcode_t pwm_setup_output(uint32_t freq, uint8_t duty)
{
    uint32_t low_time, high_time;
    errcode_t ret;

    // 计算PWM时序
    ret = calculate_pwm_timing(freq, duty, &low_time, &high_time);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    // 配置PWM参数
    pwm_config_t cfg = {
        .low_time = low_time,
        .high_time = high_time,
        .offset_time = 0,
        .cycles = 0,        // 0表示连续输出
        .repeat = 1         // 重复模式
    };

    // 关闭之前的PWM（如果已打开）
    if (g_pwm_initialized) {
        uapi_pwm_close(PWM_CHANNEL);
    }

    // 打开PWM
    ret = uapi_pwm_open(PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        osal_printk("Error: uapi_pwm_open failed: %d \r\n", ret);
        return ret;
    }

    // 启动PWM
    ret = uapi_pwm_start(PWM_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        osal_printk("Error: uapi_pwm_start failed: %d \r\n", ret);
        uapi_pwm_close(PWM_CHANNEL);
        return ret;
    }

    g_pwm_initialized = 1;
    return ERRCODE_SUCC;
}
