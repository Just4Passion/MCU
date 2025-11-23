#ifndef DRV_MISC_MAP_H
#define DRV_MISC_MAP_H

#include "stm32f4xx.h"

/********************************************
 * 
 *              GPIO功能相关接口
 * 时钟映射
 * 外部中断映射
 * 
 * ******************************************/
void drv_gpio_enable_clk(uint32_t gpio_handle);
void drv_gpio_exti_config(uint32_t gpio_handle, uint16_t gpio_pin);

uint32_t drv_gpio_get_exti_line(uint16_t gpio_pin);
uint32_t drv_gpio_get_exti_iqr_channel(uint16_t gpio_pin);
uint32_t drv_gpio_get_pin(uint32_t pin_no);
uint32_t drv_gpio_get_af_src(uint32_t pin_no);


#endif
