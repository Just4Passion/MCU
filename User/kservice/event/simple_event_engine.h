
#ifndef SIMPLE_EVENT_ENGINE_H
#define SIMPLE_EVENT_ENGINE_H

/*********************************************************************************
 * 简单事件驱动框架
 *      订阅事件: 所有订阅者放在订阅列表中
 *      产生事件
 *      事件放入事件队列
 *      从事件队列中取出事件, 检查所有订阅者, 订阅了该事件, 则处理 —— 可以使用节点-链表的方式[event]-[sub1]-[sub2]-[sub3]
 * 
 * 异步事件驱动
 *      事件发生: 事件发生, 调用派发器去派发事件
 *      事件派发: 事件派发器查阅订阅名单, 发现有三个服务订阅了事件
 *      通知订阅者: 事件派发器异步地把消息分别传递给三个服务
 *      事件处理: 三个服务独立地, 并行地执行自己的任务
 * 
 * 如何避免事件循环依赖
 *      1、事件循环依赖: 事件A触发事件B, 事件B又触发事件A; 多个事件处理函数相互调用, 形成循环
 *      2、如何避免
 *          (1)确保事件流向是单向的
 *          (2)使用事件版本控制??
 *          (3)分离读写事件: 将读事件和写事件分开
 *          (4)超时和断路机制: 为事件处理设置超时, 如果事件处理长事件未完成, 则超时并释放资源, 避免无限等待
 *          (5)监控和告警: 监控事件流, 及时发现循环依赖. 记录事件链, 发现同一个事件被重复触发时发出告警
 * 
 * 如何处理异步事件带来的复杂度
 *      1、事件顺序问题: 异步事件可能以与发布顺序不同的顺序被处理
 *          (1)问题: 异步事件可能以与发布顺序不同的顺序被处理
 *          (2)解决方法: 使用事件序列号. 为每个事件分配一个递增的序列号，消费者按照序列号顺序处理
 *      2、事件重复问题
 *          (1)问题: 超时重试等原因, 事件可能被重复传递
 *          (2)解决方案: 实现幂等性. 确保同一事件被处理多次的结果与处理一次相同。例如，通过事件ID去重
 *      3、事件丢失问题
 *          (1)问题: 事件在传递过程中可能丢失
 *          (2)解决方案: 使用持久化事件总线. 确保事件在传递过程中被持久化，直到被成功消费; 确认机制. 消费者成功处理事件后发送确认，否则重试
 *      4、事务一致性
 *          (1)问题: 在分布式系统中，事件发布和业务操作可能不在同一个事务中，导致数据不一致
 *          (2)解决方案: 使用事务性发件箱模式; 使用两阶段提交(2PC)或最终一致性模式
 *      5、错误处理
 *          (1)问题: 事件处理过程中可能发生错误，如何重试和补偿
 *          (2)解决方案: 重试机制; 死信队列. 将多次重试失败的事件转移到死信队列; 补偿事务. 如果事件处理失败，需要执行补偿操作来回滚之前的操作
 *      6、可观测性
 *          (1)问题: 异步事件流使得调试和追踪变得困难
 *          (1)解决方案: 使用分布式追踪. 为每个事件分配一个追踪ID; 记录详细日志; 监控指标. 如事件处理延迟、吞吐量、错误率等
 *      7、测试复杂性
 *          (1)问题: 异步事件处理难以测试，因为测试用例需要等待事件被处理
 *          (2)解决方案: 使用测试专用的事件总线，以便在测试中模拟事件和等待事件处理; 使用模拟（Mock）和存根（Stub）来隔离组件
 *
 * 不同的模块会产生不同的事件
 * 
 * 
*******************************************************************************/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"


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
/*事件类型*/
typedef enum 
{
    EVENT_NONE = 0,
    EVENT_SW_TIMER_EXPIRED,             // 软件定时器到期了
    EVENT_HW_TIMER_EXPIRED,             // 硬件定时器到期了
    EVENT_BUTTON_PRESS,                 // 按键按下
    EVENT_BUTTON_RELEASE,               // 按键释放
    EVENT_BUTTON_LONG_PRESS,            // 按键长按
}event_type_t;

/*事件结构体*/
typedef struct
{
    event_type_t type;                  // 事件类型
    uint32_t timestamp;                 // 事件时间戳, 当使用RTC时有作用
    void *data;                         // 事件数据指针: 不同的事件携带不同的数据
    uint16_t data_size;                 // 事件数据大小
}event_t;

/*事件处理回调函数*/
typedef void (*event_handler_t)(event_t *event);

/*事件订阅项*/
typedef struct
{
    uint16_t subscribe_id;                  // 订阅ID, 标识订阅者的身份
    event_type_t event_type;                // 订阅的事件类型
    event_handler_t handler;                // 事件的处理回调
    bool enabled;                            // 处理使能
}event_subscription_t;

/**********************************************
 * 
 *                  函数声明
 * 
***********************************************/
/*事件引擎初始化*/
void event_engine_init();

/*事件的分发与处理*/
bool event_publish(event_t *event);     // 把事件放到事件队列中, 等待处理
bool event_process();                   // 从事件队列中取出事件, 进行处理

/*事件订阅管理*/
bool event_subscribe(event_type_t event_type, event_handler_t handler);
bool event_unsubscribe(event_type_t event_type, event_handler_t handler);
//bool event_unsubscribe(uint32_t subscribe_id);                              
void event_enable_subscription(event_type_t event_type, event_handler_t handler);


/*工具函数*/
event_t event_create(event_type_t type, void* data, uint16_t data_size);
uint32_t event_get_timestamp(void);     // 获取事件时间戳
#endif

