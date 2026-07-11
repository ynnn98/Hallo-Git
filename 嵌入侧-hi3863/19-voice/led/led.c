#include "pinctrl.h"
#include "common_def.h"
#include "soc_osal.h"
#include "osal_wait.h"
#include "app_init.h"
#include "gpio.h"
#include "stdio.h"
#include "led.h"


void led_init(void)
{

    // 设置IO复用关系，使用普通IO功能
    uapi_pin_set_mode(GPIO_02, PIN_MODE_0);
    uapi_pin_set_mode(GPIO_03, PIN_MODE_0);

    // 设置IO引脚的方向
    uapi_gpio_set_dir(GPIO_02, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_dir(GPIO_03, GPIO_DIRECTION_OUTPUT);

    // 设置IO引脚的电平  
    uapi_gpio_set_val(GPIO_02, GPIO_LEVEL_LOW);
    uapi_gpio_set_val(GPIO_03, GPIO_LEVEL_LOW);
}

void led_on(void)
{
    // 示例：假设LED接GPIO10，低电平点亮
    uapi_gpio_set_val(GPIO_02, GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(GPIO_03, GPIO_LEVEL_HIGH);
    printf("LED turned on\n");
}

void led_off(void)
{
    uapi_gpio_set_val(GPIO_02, GPIO_LEVEL_LOW);
    uapi_gpio_set_val(GPIO_03, GPIO_LEVEL_LOW);
    printf("LED turned off\n");
}

void led_toggle(void)
{
    uapi_gpio_toggle(GPIO_02);
    uapi_gpio_toggle(GPIO_03);
}

