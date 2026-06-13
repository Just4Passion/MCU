
#ifndef DRV_WATCHDOG_H
#define DRV_WATCHDOG_H

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"

typedef enum
{
    WDG_FEED_IWDG = (DY_DEVICE_CTRL_CMD_MAX + 1),
    WDG_FEED_WWDG
}watchdog_ctrl_cmd_t;


/**********************************************
 * 
 *                  服务接口
 * 
 ***********************************************/
int drv_iwdg_init();

int drv_wwdg_init();

#endif
