
#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

/**********************************************
 * 简单软件定时器
 *      超时时间
 *      回调函数
 *      是否重载
 * 
 * 业务逻辑
 *      创建定时器之处, 定时器处于停止状态: 创建成功之后, 启动加入到管理器中, 此时依然可以被覆盖
 *      启动后, 定时器处于运行状态
 *
***********************************************/

#include <stdlib.h>
#include <stdbool.h>

#include "stm32f4xx.h"


/**********************************************
 * 
 *                  宏定义
 * 
***********************************************/
#define MAX_TIMERS 8


/**********************************************
 * 
 *                  类型定义
 * 
***********************************************/
typedef void (*timer_callback)(void);

typedef enum
{
    TIMER_STOPPED = 0,  // 停止
    TIMER_RUNNING,      // 运行中
    TIMER_EXPIRED       // 过期
}timer_state_t;

typedef struct
{
    uint32_t period_ms;         // 定时周期, 毫秒
    uint32_t remaining_ticks;   // 剩余ticks数
    timer_callback callback;
    timer_state_t state;
    bool auto_reload;
}software_timer_t;

/**********************************************
 * 
 *              定时器模块管理函数
 * 
***********************************************/
void timer_manager_init(void);      // 创建一个管理器用来管理定时器
void timer_manager_tick_handler(void);          // 状态更新, 回调执行都在SysTick中断中执行
void timer_manager_tick_decrement_handler();    // 在SysTick中断中更新定时器状态为"过期"
void timer_manager_expired_timer_handler();     // 在main中执行过期定时器的回调, 执行完毕调整定时器状态

int8_t timer_create(uint32_t period_ms, timer_callback callback, bool auto_reload); // 返回定时器ID, 通过ID可以查找定时器
bool timer_start(uint8_t timer_id);
bool timer_stop(uint8_t timer_id);
bool timer_restart(uint8_t timer_id);
bool timer_delete(uint8_t timer_id);
bool timer_set_period(uint8_t timer_id, uint32_t period_ms);

#endif

