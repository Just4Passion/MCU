
#ifndef DRV_TIMER_H
#define DRV_TIMER_H

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"

/*************************************************************
 * 时钟源，控制器，递增计数器
 * 
 * 基本定时器特性
 *      (1)组成: 时钟源, 控制器, 递增计数器
 * 
 * 通用定时器特性
 * 
 * 高级定时器特性
 *      (1)时钟源可选外部时钟或其他定时器
 * 
 * 定时器的应用
 *      1、定时发送数据
 *      2、定时采集数据
 *      3、定时器+GPIO测量输入信号的脉冲宽度: 波特率自动检测
 *      4、定时器+GPIO输出波形
 *      5、定时器+GPIO产生PWM控制电机
 * 
 * 
 **************************************************************/
/*计时定时器类型: 仅作为定时器功能*/
typedef enum
{
    BTIMER1,
    BTIMER2,
    BTIMER3,
    BTIMER4,
    BTIMER5,
    BTIMER6,
    BTIMER7,
    BTIMER8,
    BTIMER9,
    BTIMER10,
    BTIMER11,
    BTIMER12,
    BTIMER13,
    BTIMER14
}base_timer_type_t;

/*计时定时器类型: 仅作为定时器功能*/
typedef enum
{
    /*作为IC定时器*/
    ICTIMER1,
    ICTIMER2,
    ICTIMER3,
    ICTIMER4,
    ICTIMER5,
    ICTIMER8,
    ICTIMER9,
    ICTIMER10,
    ICTIMER11,
    ICTIMER12,
    ICTIMER13,
    ICTIMER14
}ic_timer_type_t;

/*计时定时器类型: 仅作为定时器功能*/
typedef enum
{
    /*作为IC定时器*/
    OCTIMER1,
    OCTIMER2,
    OCTIMER3,
    OCTIMER4,
    OCTIMER5,
    OCTIMER8,
    OCTIMER9,
    OCTIMER10,
    OCTIMER11,
    OCTIMER12,
    OCTIMER13,
    OCTIMER14
}oc_timer_type_t;

typedef enum
{
    TIMER_START = (DY_DEVICE_CTRL_CMD_MAX + 1),
    TIMER_STOP,
    TIMER_EXE_IRQ,      //执行中断
    TIMER_GET_CNT,      //获取计数值
    TIMER_OC_START,
    TIMER_OC_STOP,
    TIMER_IC_START,
    TIMER_IC_STOP,
    TIMER_BT_START,
    TIMER_BT_STOP
}timer_ctrl_cmd_t;

typedef enum
{
    BTIMER_START = (DY_DEVICE_CTRL_CMD_MAX + 1),          //启动
    BTIMER_STOP,            //停止
    BTIMER_EN_IRQ,          //中断使能
    BTIMER_DN_IRQ,          //关闭中断
    BTIMER_EN_DMA,          //使能DMA
    BTIMER_DN_DMA,           //关闭DMA
    BTIMER_OUTPUT_TRIGGER,  //设置输出触发
    BTIMER_IRQ_EXE,         //执行中断
    BTIMER_GET_CNT,         //获取计数值
}base_timer_ctrl_cmd_t;

typedef enum
{
    OCTIMER_START = (DY_DEVICE_CTRL_CMD_MAX + 1),          //启动
    OCTIMER_STOP,            //停止
    OCTIMER_EN_IRQ,          //中断使能
    OCTIMER_DN_IRQ,          //关闭中断
    OCTIMER_EN_DMA,          //使能DMA
    OCTIMER_DN_DMA,          //关闭DMA
    OCTIMER_IRQ_EXE,         //执行中断
    OCTIMER_GET_CNT,         //获取计数值
    OCTIMER_SET_PULSE
}oc_timer_ctrl_cmd_t;

typedef enum
{
    ICTIMER_START = (DY_DEVICE_CTRL_CMD_MAX + 1),          //启动
    ICTIMER_STOP,            //停止
    ICTIMER_EN_IRQ,          //中断使能
    ICTIMER_DN_IRQ,          //关闭中断
    ICTIMER_EN_DMA,          //使能DMA
    ICTIMER_DN_DMA,           //关闭DMA
}ic_timer_ctrl_cmd_t;

/**********************************************
 * 
 *                  服务接口
 * 
 ***********************************************/
int drv_base_timer_init();


#endif
