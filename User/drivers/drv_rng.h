#ifndef DRV_RNG_H
#define DRV_RNG_H

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"
/*随机数发生器: 提供由模拟量发生器产生的 32 位随机数*/

typedef enum
{
    RNG_START = (DY_DEVICE_CTRL_CMD_MAX + 1),       //启用随机数发生器
    RNG_STOP,                                       //停止随机数发生器
    RNG_GET_RANDOM,                                 //获取随机数
    RNG_EXE_IRQ,                                    //执行中断
}rng_ctrl_cmd_t;



#endif

