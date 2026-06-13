
#include <stdint.h>

#include "drv_time.h"

static uint32_t g_sys_tick = 0;

void drv_time_sys_tick_increment()
{
    g_sys_tick++;
}

uint32_t drv_time_get_sys_tick()
{
    return g_sys_tick;
}

void drv_time_delay_ms(uint32_t timeout)
{
    uint32_t start_time = drv_time_get_sys_tick();
    while(drv_time_get_sys_tick() - start_time < timeout)
    {

    }
}

