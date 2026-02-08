
#ifndef DRV_DAC_H
#define DRV_DAC_H

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"

typedef enum
{
    DAC_CH1_START = (DY_DEVICE_CTRL_CMD_MAX + 1),
    DAC_CH1_STOP,
    DAC_CH2_START,
    DAC_CH2_STOP,
    DAC_START,
    DAC_STOP
}dac_ctrl_cmd_t;


#endif
