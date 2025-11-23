
#include "simple_event_engine.h"

/**********************************************
 * 
 *                  宏定义
 * 
***********************************************/

#define EVENT_QUEUE_SIZE    32      // 事件队列大小
#define MAX_SUBSCRIPTIONS   16      // 最大订阅数

/**********************************************
 * 
 *                  类型定义
 * 
***********************************************/
/*事件队列*/
typedef struct
{
    event_t event_queue[EVENT_QUEUE_SIZE];
    uint8_t queue_head;
    uint8_t queue_tail;
    uint8_t queue_count;
}event_queue_t;


/*事件订阅表*/
typedef struct
{
    event_subscription_t subscriptions[MAX_SUBSCRIPTIONS];
    uint8_t subscription_count;         // 订阅的数量
}event_subscription_table_t;
/**********************************************
 * 
 *                  局部变量
 * 
***********************************************/
static event_queue_t g_event_queue;                     // 全局变量未初始化, 不会放在flash中
static event_subscription_table_t g_subscription_table; // 全局变量未初始化, 不会放在flash中

/*****************************************************
 * 
 *      事件生产者
 *      事件消费者
 * 
 *      订阅事件, 检测到订阅了事件, 则使用回调函数处理该事件
 *      需要注意生产者有消费者的速度问题: 如何做到限速
 * 
 * 1、可以订阅重复的事件
 * 2、一个事件回调只能处理一种事件
 * 3、不同的模块之间无标识: 比如A模块订阅了事件E1, E2, E3; B模块订阅事件E2, E4. 只能通过订阅事件处理函数来区分
 * 4、如果要在中断中使用, 则需要在生产和消费过程中, 把中断禁掉; 同时一些不会使用这些函数的中断无需禁用
 * 
 ******************************************************/

/**
 * @brief 事件引擎初始化
 * @note 
 */
void event_engine_init()
{
    g_event_queue.queue_head = 0;
    g_event_queue.queue_tail = 0;
    g_event_queue.queue_count = 0;

    g_subscription_table.subscription_count = 0;
}


/**
 * @brief 事件分发: 往事件队列存放事件
 * @note 
 */
bool event_publish(event_t *event)
{
    if (g_event_queue.queue_count >= EVENT_QUEUE_SIZE)
    {
        return false;       // 队列满了
    }

    event->timestamp = 0;   // 获取系统时间戳

    g_event_queue.event_queue[g_event_queue.queue_tail] = *event;
    g_event_queue.queue_tail = (g_event_queue.queue_tail + 1) % EVENT_QUEUE_SIZE;
    g_event_queue.queue_count++;
    return true;
}

/**
 * @brief 事件分发: 往事件队列存放事件
 * @note 
 */
bool event_process()
{
    if (g_event_queue.queue_count == 0) 
    {
        return false; // 队列为空
    }
    uint8_t i = 0;
    /*从头中取出一个事件*/
    event_t event = g_event_queue.event_queue[g_event_queue.queue_head];
    g_event_queue.queue_head = (g_event_queue.queue_head + 1) % EVENT_QUEUE_SIZE;
    g_event_queue.queue_count--;
    
    /*检查订阅表, 订阅了该事件则进行处理*/
    for (i = 0; i < g_subscription_table.subscription_count; ++i)
    {
        if (g_subscription_table.subscriptions[i].event_type == event.type && 
            g_subscription_table.subscriptions[i].enabled &&
            g_subscription_table.subscriptions[i].handler != NULL)
        {
            g_subscription_table.subscriptions[i].handler(&event);
        }
    }
    return true;
}

/**
 * @brief 事件订阅
 * @note 
 */
bool event_subscribe(event_type_t event_type, event_handler_t handler)
{
    if (g_subscription_table.subscription_count >= MAX_SUBSCRIPTIONS) 
    {
        return false;
    }

    /*检查事件是否已经订阅*/
    for (uint8_t i = 0; i < g_subscription_table.subscription_count; i++) 
    {
        if (g_subscription_table.subscriptions[i].event_type == event_type && 
            g_subscription_table.subscriptions[i].handler == handler)
        {
            g_subscription_table.subscriptions[i].enabled = true;
            return true;
        }
    }

    /*添加新的订阅*/
    g_subscription_table.subscriptions[g_subscription_table.subscription_count].event_type = event_type;
    g_subscription_table.subscriptions[g_subscription_table.subscription_count].handler = handler;
    g_subscription_table.subscriptions[g_subscription_table.subscription_count].enabled = true;
    g_subscription_table.subscription_count++;
    
    return true;
}

/**
 * @brief 取消事件订阅: 并没有删除
 * @note 
 */
bool event_unsubscribe(event_type_t event_type, event_handler_t handler)
{
    for (uint8_t i = 0; i < g_subscription_table.subscription_count; i++) 
    {
        if (g_subscription_table.subscriptions[i].event_type == event_type && 
            g_subscription_table.subscriptions[i].handler == handler)
        {
            g_subscription_table.subscriptions[i].enabled = false;
            return true;
        }
    }
    return false;
}
//bool event_unsubscribe(uint32_t subscribe_id);                              // 取消订阅

/**
 * @brief 使能事件订阅
 * @note
 */
void event_enable_subscription(event_type_t event_type, event_handler_t handler)
{
    for (uint8_t i = 0; i < g_subscription_table.subscription_count; i++) 
    {
        if (g_subscription_table.subscriptions[i].event_type == event_type && 
            g_subscription_table.subscriptions[i].handler == handler)
        {
            g_subscription_table.subscriptions[i].enabled = true;
        }
    }
}

/**
 * @brief 获取待处理事件的数量
 * @note 
 */
uint8_t event_get_pending_count(void)
{
    return g_event_queue.queue_count;
}


