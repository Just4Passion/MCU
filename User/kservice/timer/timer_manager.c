
#if 1 //USING_SOFTWARE_TIMER
#include "timer_manager.h"

typedef struct
{
    software_timer_t timers[MAX_TIMERS];
    uint32_t timer_count;               // 当前定时器总数
    uint32_t active_timer_count;        // 当前活跃的定时器数量
}timer_manager_t;

static timer_manager_t g_tim_mnger = {
    .timer_count = 0, 
    .active_timer_count = 0
};

/**********************************************
 * 
 *                  静态函数
 * 
***********************************************/
/**
 * @brief 查找空闲的定时器
 */
static int8_t find_free_timer(void)
{
    uint8_t i = 0;
    for (i = 0; i < MAX_TIMERS; i++) 
    {
        if (g_tim_mnger.timers[i].state == TIMER_STOPPED) 
        {
            return i;
        }
    }
    return -1; // 没有空闲定时器
}

/**
 * @brief 定时器管理初始化
 */
void timer_manager_init(void)
{
    uint8_t i = 0;
    for (i = 0; i < MAX_TIMERS; ++i)
    {
        g_tim_mnger.timers[i].state = TIMER_STOPPED;
        g_tim_mnger.timers[i].callback = NULL;
        g_tim_mnger.timers[i].period_ms = 0;
        g_tim_mnger.timers[i].remaining_ticks = 0;
        g_tim_mnger.timers[i].auto_reload = false;
    }
}


/**
 * @brief 定时器管理tick处理函数: 回调函数在中断中执行
 */
void timer_manager_tick_handler(void)
{
    /*编译所有定时器, 处于运行态, 且到期的, 执行回调函数; 回调函数执行后, 可重载, 可停止*/
    uint8_t i = 0;
    for (i = 0 ; i < MAX_TIMERS; ++i)
    {
        /**/
        if (g_tim_mnger.timers[i].state == TIMER_RUNNING)
        {
            if (--g_tim_mnger.timers[i].remaining_ticks == 0)
            {
                /*定时器到期了, 执行回调*/
                g_tim_mnger.timers[i].state = TIMER_EXPIRED;
                if (g_tim_mnger.timers[i].callback != NULL)
                {
                    g_tim_mnger.timers[i].callback();
                }

                if (g_tim_mnger.timers[i].auto_reload)
                {
                    g_tim_mnger.timers[i].remaining_ticks = g_tim_mnger.timers[i].period_ms;
                    g_tim_mnger.timers[i].state = TIMER_RUNNING;
                }
                else
                {
                    g_tim_mnger.timers[i].state = TIMER_STOPPED;
                }
            }
        }
    }
}

/**
 * @brief 定时器管理tick处理函数: 在中断中修改定时器状态
 */
void timer_manager_tick_decrement_handler(void)
{
    /*编译所有定时器, 处于运行态, 且到期的, 执行回调函数; 回调函数执行后, 可重载, 可停止*/
    uint8_t i = 0;
    for (i = 0 ; i < MAX_TIMERS; ++i)
    {
        /**/
        if (g_tim_mnger.timers[i].state == TIMER_RUNNING)
        {
            if (--g_tim_mnger.timers[i].remaining_ticks == 0)
            {
                /*定时器到期了, 设置到期*/
                g_tim_mnger.timers[i].state = TIMER_EXPIRED;
            }
        }
    }
}

/**
 * @brief 定时器管理超时定时器处理函数: 在主循环中处理超时的定时器, 执行回调
 */
void timer_manager_expired_timer_handler(void)
{
    /*编译所有定时器, 处于运行态, 且到期的, 执行回调函数; 回调函数执行后, 可重载, 可停止*/
    uint8_t i = 0;
    for (i = 0 ; i < MAX_TIMERS; ++i)
    {
        /**/
        if (g_tim_mnger.timers[i].state == TIMER_EXPIRED)
        {
            if (g_tim_mnger.timers[i].callback != NULL)
            {
                g_tim_mnger.timers[i].callback();
            }
            if (g_tim_mnger.timers[i].auto_reload)
            {
                g_tim_mnger.timers[i].remaining_ticks = g_tim_mnger.timers[i].period_ms;
                g_tim_mnger.timers[i].state = TIMER_RUNNING;
            }
            else
            {
                g_tim_mnger.timers[i].state = TIMER_STOPPED;
            }
        }
    }
}

/**
 * @brief 创建一个定时器
 * @note 查找是否有不运行的定时器, 有则覆盖
 */
int8_t timer_create(uint32_t period_ms, timer_callback callback, bool auto_reload)
{
    /*查找空闲的定时器*/
    int8_t timer_id = find_free_timer();
    if (timer_id < 0)
    {
        return -1;
    }
    g_tim_mnger.timers[timer_id].state = TIMER_STOPPED;
    g_tim_mnger.timers[timer_id].callback = callback;
    g_tim_mnger.timers[timer_id].period_ms = period_ms;
    g_tim_mnger.timers[timer_id].remaining_ticks = 0;
    g_tim_mnger.timers[timer_id].auto_reload = auto_reload;
    return timer_id; // 没有空闲定时器
}

/**
 * @brief 启动定时器
 */
bool timer_start(uint8_t timer_id)
{
    if (timer_id >= MAX_TIMERS || g_tim_mnger.timers[timer_id].state != TIMER_STOPPED) 
    {
        return false;
    }
    
    g_tim_mnger.timers[timer_id].remaining_ticks = g_tim_mnger.timers[timer_id].period_ms;
    g_tim_mnger.timers[timer_id].state = TIMER_RUNNING;
    return true;
}

/**
 * @brief 停止定时器
 */
bool timer_stop(uint8_t timer_id)
{
    if (timer_id >= MAX_TIMERS) 
    {
        return false;
    }
    
    g_tim_mnger.timers[timer_id].state = TIMER_STOPPED;
    return true;
}

/**
 * @brief 重启定时器
 */
bool timer_restart(uint8_t timer_id)
{
    if (timer_id >= MAX_TIMERS) 
    {
        return false;
    }
    
    /*重新赋值定时器当前倒计时*/
    g_tim_mnger.timers[timer_id].remaining_ticks = g_tim_mnger.timers[timer_id].period_ms;
    /*设置定时器处于运行状态*/
    g_tim_mnger.timers[timer_id].state = TIMER_RUNNING;
    return true;
}

/**
 * @brief 删除定时器
 */
bool timer_delete(uint8_t timer_id)
{
    if (timer_id >= MAX_TIMERS) 
    {
        return false;
    }
    
    g_tim_mnger.timers[timer_id].state = TIMER_STOPPED;
    g_tim_mnger.timers[timer_id].callback = NULL;
    g_tim_mnger.timers[timer_id].period_ms = 0;
    g_tim_mnger.timers[timer_id].remaining_ticks = 0;
    g_tim_mnger.timers[timer_id].auto_reload = false;
    
    return true;
}


/**
 * @brief 设置定时器周期
 */
bool timer_set_period(uint8_t timer_id, uint32_t period_ms)
{
    if (timer_id >= MAX_TIMERS || period_ms == 0) 
    {
        return false;
    }
    
    g_tim_mnger.timers[timer_id].period_ms = period_ms;
    /*处于运行状态的, 才可以设置运行周期*/
    if (g_tim_mnger.timers[timer_id].state == TIMER_RUNNING) 
    {
        g_tim_mnger.timers[timer_id].remaining_ticks = period_ms;
    }
    return true;
}




#endif
