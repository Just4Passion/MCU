
#ifndef DRV_TIME_H
#define DRV_TIME_H

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"

void drv_time_sys_tick_increment();
uint32_t drv_time_get_sys_tick();
void drv_time_delay_ms(uint32_t timeout);
#endif
