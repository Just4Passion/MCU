
#ifndef DRV_RTC_H
#define DRV_RTC_H


#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

typedef enum
{
    RTC_SET_DATE_TIME = (DY_DEVICE_CTRL_CMD_MAX + 1),
    RTC_GET_DATE_TIME
}rtc_ctrl_cmd_t;

typedef struct
{
    uint16_t year;       //可以以1970年为基准
    uint8_t month;      //1-12
    uint8_t date;       //1-31
    uint8_t weekday;    //
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
}rtc_date_time;

int drv_rtc_init();

#endif
