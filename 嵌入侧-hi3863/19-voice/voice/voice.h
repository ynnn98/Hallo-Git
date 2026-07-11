#ifndef VOICE_H
#define VOICE_H

// 函数声明
void uart_gpio_init(void);
void uart_init_config(void);
void voice_analysis(uint8_t *info);
void *uart_voice_task(const char *arg);

#endif