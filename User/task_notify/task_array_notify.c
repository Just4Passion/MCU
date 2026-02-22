
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"


/************************************************************************
 * 
 * FreeRTOS中使用configTASK_NOTIFICATION_ARRAY_ENTRIES维护了任务通知数组
 * - xTaskNotifyWaitIndexed: 等待通知
 * - xTaskNotifyAndQueryIndexed: 查询通知
 * - xTaskNotifyAndQueryIndexedFromISR
 * 
 * Index可以指定为 [0, configTASK_NOTIFICATION_ARRAY_ENTRIES)
 * 
 ************************************************************************/











