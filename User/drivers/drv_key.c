
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "simple_event_engine.h"
#include "kservice.h"

#include "drv_misc_func.h"

/**********************************************
 * 
 *                  类型定义
 * 
***********************************************/
typedef enum
{
    KEY_GET_STATE,              // 获取当前状态
    KEY_GET_LONG_PRESS_TIME,    // 获取长按时间
    KEY_GET_PRESS_TIMESTAMP,    // 获取按下的时间戳
    KEY_SET_PRESS_TIMESTAMP,    // 设置按下时间戳
    KEY_SET_RELEASE_TIMESTAMP,  // 设置释放时间戳
}key_ctrl_cmd_t;

typedef struct
{
    char name[32 + 1];  // 按键的名称
}key_event_data_t;

typedef struct
{
    /*两个独立按键*/
    GPIO_TypeDef *gpio_key_port;
    uint16_t gpio_key_pin;
    /*外部中断*/
    // 外部中断的句柄
    // 外部中断的通道
    bool exti_int;              // 外部中断使能
    uint8_t exti_int_priority;  // 中断优先级
    IRQn_Type exti_irq;         // 外部中断类型

}key_config_t;

typedef struct
{
    dy_device_t device;
    key_config_t key_cfg;
    uint8_t key_state;                  // 按键当前状态: 按下, 弹起
    uint32_t key_press_timestamp;       // 按键按下的时间戳
    uint32_t key_release_timestamp;     // 按键弹起的时间戳

    event_t key_event;
    key_event_data_t event_data;
}key_dev_t;

/**********************************************
 * 
 *                  局部变量
 * 
***********************************************/
static key_dev_t g_key_dev1;
static key_dev_t g_key_dev2;

/**********************************************
 * 
 *                  函数定义
 * 
***********************************************/
/**
 * @brief 按键初始化
 */
static int key_init(void *dev)
{
    key_dev_t *key_dev = (key_dev_t*)dev;

    GPIO_InitTypeDef stIOInit;
    /*先初始化时钟*/
    /*启用时钟: 时钟, 需要一层时钟到端口的映射关系*/
    drv_gpio_enable_clk((uint32_t)(key_dev->key_cfg.gpio_key_port));
    /*然后配置端口模式*/
    stIOInit.GPIO_Mode = GPIO_Mode_IN;      // 先配置ModeIn
	//stIOInit.GPIO_OType = GPIO_OType_PP;
	//stIOInit.GPIO_Speed = GPIO_Speed_2MHz;
	stIOInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
    stIOInit.GPIO_Pin = key_dev->key_cfg.gpio_key_pin;
    GPIO_Init(key_dev->key_cfg.gpio_key_port, &stIOInit);

    /*配置中断*/
    if (key_dev->key_cfg.exti_int)
    {
        NVIC_InitTypeDef stNVICInit;
        EXTI_InitTypeDef stEXTIInit;

        stNVICInit.NVIC_IRQChannel = drv_gpio_get_exti_iqr_channel(key_dev->key_cfg.gpio_key_pin);
	    stNVICInit.NVIC_IRQChannelPreemptionPriority = 1;
	    stNVICInit.NVIC_IRQChannelSubPriority = 1;
	    stNVICInit.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&stNVICInit);

        drv_gpio_exti_config((uint32_t)key_dev->key_cfg.gpio_key_port, key_dev->key_cfg.gpio_key_pin);
        stEXTIInit.EXTI_Line = drv_gpio_get_exti_line(key_dev->key_cfg.gpio_key_pin);
        stEXTIInit.EXTI_Mode = EXTI_Mode_Interrupt;			    //中断
        stEXTIInit.EXTI_Trigger = EXTI_Trigger_Rising;	        //上升沿触发, 配置上升沿触发器
        stEXTIInit.EXTI_LineCmd = ENABLE;
        EXTI_Init(&stEXTIInit);
    }
    return DY_EOK;
}

/**
 * @brief key读取
 */
static int key_read(void *dev, void *buf, int len)
{
    key_dev_t *key_dev = (key_dev_t*)dev;
    uint8_t *data = (uint8_t*)buf;
    /*读电平, 检测到按下, 则记录时间戳, 检测到弹起则记录时间戳*/
    *data = GPIO_ReadInputDataBit(key_dev->key_cfg.gpio_key_port, key_dev->key_cfg.gpio_key_pin);
    if (1 == *data)
    {
        key_dev->key_state = 1; // 按下
    }
    else
    {
        key_dev->key_state = 0; // 释放
    }
    return 1;
}


/**
 * @brief key控制接口
 */
static int key_control(void *dev, int cmd, void *arg)
{
    key_dev_t *key_dev = (key_dev_t*)dev;
    switch (cmd)
    {
        case KEY_GET_STATE:
            *(uint8_t*)arg = key_dev->key_state;
            break;
        /*获取长按的时间*/
        case KEY_GET_LONG_PRESS_TIME:
            if (key_dev->key_release_timestamp > key_dev->key_press_timestamp)
            {
                *(uint32_t*)arg = key_dev->key_release_timestamp - key_dev->key_press_timestamp;
            }
            else
            {
                *(uint32_t*)arg = 0;
            }
            break;
        /*获取按下时间戳*/
        case KEY_GET_PRESS_TIMESTAMP:
            *(uint32_t*)arg = key_dev->key_press_timestamp;
            break;
        /*设置按下时间戳*/
        case KEY_SET_PRESS_TIMESTAMP:
            key_dev->key_press_timestamp = (uint32_t)(*(uint32_t*)arg);
            break;
        /*设置松开时间戳*/
        case KEY_SET_RELEASE_TIMESTAMP:
            key_dev->key_release_timestamp = (uint32_t)(*(uint32_t*)arg);
            break;
        default:
            break;
    }
    return DY_EOK;
}

static device_ops_t key_dev_ops = 
{
    .init = key_init,
    .open = NULL,           // open 和 close 可以用于一些低功耗的场景
    .close = NULL,
    .read = key_read, 
    .write = NULL,
    .control = key_control,
    .callback = NULL
};


/**
 * @brief key注册
 */
int drv_key_register()
{
    /*注册两个键值*/
    /*Key1注册*/
    strcpy(g_key_dev1.device.name, "key1");
    g_key_dev1.device.ops = &key_dev_ops;
    g_key_dev1.key_cfg.gpio_key_port = GPIOA;
    g_key_dev1.key_cfg.gpio_key_pin = GPIO_Pin_0;
    g_key_dev1.key_cfg.exti_int = false;
    g_key_dev1.key_state = 0;
    g_key_dev1.key_press_timestamp = 0;
    g_key_dev1.key_release_timestamp = 0;
    g_key_dev1.key_event.data = &g_key_dev1.event_data;
    g_key_dev1.key_event.data_size = sizeof(g_key_dev1.event_data);
    // 注册设备
    if (DY_EOK != dy_device_register("key1", &g_key_dev1.device))
    {
        return DY_ERROR;
    }

    /*Key2注册*/
    strcpy(g_key_dev2.device.name, "key2");
    g_key_dev2.device.ops = &key_dev_ops;
    g_key_dev2.key_cfg.gpio_key_port = GPIOC;
    g_key_dev2.key_cfg.gpio_key_pin = GPIO_Pin_13;
    g_key_dev2.key_cfg.exti_int = false;
    g_key_dev2.key_state = 0;
    g_key_dev2.key_press_timestamp = 0;
    g_key_dev2.key_release_timestamp = 0;
    g_key_dev2.key_event.data = &g_key_dev2.event_data;
    g_key_dev2.key_event.data_size = sizeof(g_key_dev2.event_data);

    if (DY_EOK != dy_device_register("key2", &g_key_dev2.device))
    {
        return DY_ERROR;
    }

    return DY_EOK;
}

void key1_timer_callback()
{
    static uint8_t key_last_state = 0;
    static uint32_t cur_timestamp = 0;
    uint8_t key_cur_state = 0;
    uint32_t press_time = 0;    // 按键按下事件
    dy_device_t *dev = dy_find_device("key1");
    key_dev_t *key_dev = (key_dev_t *)dev;

    dy_device_t *led_dev = dy_find_device("led");
    if (NULL == key_dev)
    {
        return;
    }
    /*读取按键状态, 并记录*/
    dev->ops->read((void*)&g_key_dev1.device, &key_cur_state, 1);
    /*记录时间戳*/
    cur_timestamp++;
    /*说明状态未发生改变*/
    if (key_cur_state == key_last_state)
    {
        if (0 == key_cur_state)
        {
            /*之前是松开的, 现在还是松开的*/
        }
        else
        {
            /*之前是按下的, 现在还是按下的: 检测已经按下了多久*/
            dev->ops->control((void*)&g_key_dev1.device, KEY_GET_PRESS_TIMESTAMP, (void*)&press_time);
            if ((cur_timestamp - press_time) > 200)
            {
                /*产生一个长按事件*/
                key_dev->key_event.type = EVENT_BUTTON_LONG_PRESS;
                //key_dev->key_event.timestamp = 0;   // ?
                strcpy(key_dev->event_data.name, "key1");
                event_publish(&key_dev->key_event);
            }
        }
    }
    /*说明状态发生改变*/
    else
    {
        if (0 == key_cur_state)
        {
            /*之前是按下, 现在松开了*/
            dev->ops->control((void*)&g_key_dev1.device, KEY_SET_RELEASE_TIMESTAMP, (void*)&cur_timestamp);
            /*产生一个release事件*/
            key_dev->key_event.type = EVENT_BUTTON_RELEASE;
            //key_dev->key_event.timestamp = 0;   // ?
            strcpy(key_dev->event_data.name, "key1");
            event_publish(&key_dev->key_event);
        }
        else
        {
            /*之前是松开的, 现在是按下了*/
            dev->ops->control((void*)&g_key_dev1.device, KEY_SET_PRESS_TIMESTAMP, (void*)&cur_timestamp);
            
            /*产生了一个press事件*/
            key_dev->key_event.type = EVENT_BUTTON_PRESS;
            //key_dev->key_event.timestamp = 0;   // ?
            strcpy(key_dev->event_data.name, "key1");
            event_publish(&key_dev->key_event);
        }
    }
    key_last_state = key_cur_state;
}


void key2_timer_callback()
{
    
}