
#ifndef DRV_RTC_H
#define DRV_RTC_H


#include "gd32f4xx.h"


typedef struct 
{
    uint32_t year; 
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}DATE_TIME;

#endif
