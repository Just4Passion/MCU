
#ifndef DRV_TIME_H
#define DRV_TIME_H

void drv_time_sys_tick_increment();
uint32_t drv_time_get_sys_tick();
void drv_time_delay_ms(uint32_t timeout);
#endif
