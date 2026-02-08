
#include <string.h>

#include "drv_watchdog.h"


/**********************************************
 * 独立看门狗: 
 *      1、特点
 *          (1)专用低速时钟 (LSI) 驱动
 *          (2)递减计数器值达到0x000时产生复位
 *          (3)寄存器访问保护
 *      2、超时范围ms: 32KHz LSI
 *          (1)预分频4: [0.125, 512]
 *          (2)预分频8: [0.25, 1024]
 *          (3)预分频16: [0.5, 2048]
 *          (4)预分频32: [1, 4096]
 *          (5)预分频64: [2, 8192]
 *          (6)预分频128: [4, 16384]
 *          (7)预分频256: [8, 32768]
 * 
 * 窗口看门狗: 提供更精确的时间控制
 *      1、特点
 *          (1)时钟由 APB1 时钟经预分频后
 *          (2)7位递减计数器
 *          (3)递减小于0x40时复位; 在窗口外重载递减计数器时复位, 比如[0x40, 0x66], 从0x7F开始递减, 没有到0x66就重载, 则重启
 *          (4)提前唤醒中断: 可在复位前执行的一个中断
 *      2、作用
 *          (1)来检测应用程序非正常的过迟或过早的操作
 *      3、超时范围us
 *          (1)预分频1: [136.53, 8.74ms] 
 *          (2)预分频2: [273.07, 17.48ms]
 *          (3)预分频3: [546.13, 34.95ms]
 *          (4)预分频4: [1092.27, 69.91ms]
 * 
 * 关于窗口看门狗
 *      
 * 
 *********************************************/

typedef enum
{
    INDEPENEENT_WATCHDOG,
    WINDOW_WATCHDOG
}watchdog_type_t;

typedef struct
{
    /*************************
     * 对于窗口看门狗来说, 过迟过早都不行, 所以是一个范围也就是[3, 5ms], 小于3ms喂狗, 复位, 大于5ms喂狗, 复位
     * 如果此时只有一个窗口大小, 那么是没有范围的, 还缺乏一个起点值
     * 
     **************************/
    watchdog_type_t wdg_type;      // 独立看门狗或者窗口看门狗: 
    uint32_t feed_timeout_ms;      // 喂狗超时时间
    uint32_t prescaler;
    uint8_t window_value;
    uint8_t counter_value;
}watchdog_cfg_t;

typedef struct
{
    dy_device_t device;
    watchdog_cfg_t cfg;
}watchdog_dev_t;


watchdog_dev_t g_iwdg;
watchdog_dev_t g_wwdg;

/****************************************************
 * 
 *                  独立看门狗
 * 
 *****************************************************/

static int iwdg_init(dy_device_t *dev)
{
    watchdog_dev_t *iwdg = (watchdog_dev_t*)dev;
    uint32_t feed_timeout_ms = iwdg->cfg.feed_timeout_ms;
    printf("===============feed_timeout_ms = %u\r\n", feed_timeout_ms);
    /*写入0x5555, 获取访问权限*/
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    /*设置预分频值*/
    if (feed_timeout_ms < 512)
    {
        IWDG_SetPrescaler(IWDG_Prescaler_4);
        IWDG_SetReload(feed_timeout_ms * 8);
    }
    else if (feed_timeout_ms < 1024)
    {
        IWDG_SetPrescaler(IWDG_Prescaler_8);
        IWDG_SetReload(feed_timeout_ms * 4);
    }
    else if (feed_timeout_ms < 2048)
    {
        IWDG_SetPrescaler(IWDG_Prescaler_16);
        IWDG_SetReload(feed_timeout_ms * 2);
    }
    else if (feed_timeout_ms < 4096)
    {
        IWDG_SetPrescaler(IWDG_Prescaler_32);
        IWDG_SetReload(feed_timeout_ms);
        printf("===============set reload = %u\r\n", feed_timeout_ms);
    }
    else if (feed_timeout_ms < 8192)
    {
        IWDG_SetPrescaler(IWDG_Prescaler_64);
        IWDG_SetReload(feed_timeout_ms / 2);
    }
    else if (feed_timeout_ms < 16384)
    {
        IWDG_SetPrescaler(IWDG_Prescaler_128);
        IWDG_SetReload(feed_timeout_ms / 4);
    }
    else if (feed_timeout_ms < 32768)
    {
        IWDG_SetPrescaler(IWDG_Prescaler_256);
        IWDG_SetReload(feed_timeout_ms / 8);
    }
    else
    {
        printf("iwdg feed timeout invalid\r\n");
        return DY_EINVAL;
    }
    /*把重装在寄存器值放到计数器中*/
    IWDG_ReloadCounter();
    /*使能*/
    IWDG_Enable();
    return DY_EOK;
}


/****************************************************
 * 
 *                  窗口看门狗
 * 
 *****************************************************/

int wwdg_init(dy_device_t *dev)
{
    watchdog_dev_t *iwdg = (watchdog_dev_t*)dev;

    NVIC_InitTypeDef   stNVICInit;
    /* 开启 WWDG 时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, ENABLE);

    /* 设置递减计数器的值 */
    WWDG_SetCounter(iwdg->cfg.counter_value);   //从这个值开始递减, 范围0x7f~0x40, 超出这个范围直接复位
    /* 设置预分频器的值 */
    WWDG_SetPrescaler(iwdg->cfg.prescaler);
    /* 设置上窗口值 */
    WWDG_SetWindowValue(iwdg->cfg.window_value);    //窗口值, 取值范围为：0x7f~0x40
    /* 设置计数器的值，使能WWDG */
    WWDG_Enable(iwdg->cfg.counter_value);


    /* 清除提前唤醒中断标志位 */
    WWDG_ClearFlag();

    #if 0   // 暂时先不开启中断
    /* 配置WWDG中断优先级 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    stNVICInit.NVIC_IRQChannel = WWDG_IRQn;
    stNVICInit.NVIC_IRQChannelPreemptionPriority = 0;
    stNVICInit.NVIC_IRQChannelSubPriority = 0;
    stNVICInit.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&stNVICInit);

    /* 开WWDG 中断 */
    WWDG_EnableIT();
    #endif
    return 0;
}

/****************************************************
 * 
 *                  看门狗初始化
 * 
 *****************************************************/
static int wdg_init(dy_device_t *dev)
{
    int ret = DY_EOK;
    watchdog_dev_t *iwdg = (watchdog_dev_t*)dev;
    if (iwdg->cfg.wdg_type == INDEPENEENT_WATCHDOG)
    {
        ret = iwdg_init(dev);
    }
    else
    {
        ret = wwdg_init(dev);
    }
    return ret;
}

static int wdg_control(dy_device_t *dev, int cmd, void *arg)
{
    int ret = DY_EOK;
    watchdog_dev_t *iwdg = (watchdog_dev_t*)dev;
    switch (cmd)
    {
        case WDG_FEED_IWDG:
            if (iwdg->cfg.wdg_type != INDEPENEENT_WATCHDOG)
            {
                return DY_EINVAL;
            }
            IWDG_ReloadCounter();
            break;
        case WDG_FEED_WWDG:
            if (iwdg->cfg.wdg_type != WINDOW_WATCHDOG)
            {
                return DY_EINVAL;
            }
            WWDG_SetCounter(iwdg->cfg.counter_value);
            break;
        default:
            ret = dy_device_control(dev, cmd, arg);
            break;
    }
    return ret;
}


device_ops_t g_wdg_ops = {
    .init = wdg_init,
    .control = wdg_control
};


/**
 * @brief 独立看门狗初始化
 */
int drv_iwdg_init()
{
    int ret = DY_EOK;
    strcpy(g_iwdg.device.name, "iwdg");
    g_iwdg.device.ops = &g_wdg_ops;
    /*配置*/
    g_iwdg.cfg.wdg_type = INDEPENEENT_WATCHDOG;
    g_iwdg.cfg.feed_timeout_ms = 4095;

    ret = dy_device_register("iwdg", &g_iwdg.device);
    if (ret != DY_EOK)
    {
        return ret;
    }
    ret = g_iwdg.device.ops->init(&g_iwdg.device);

    return ret;
}

/**
 * @brief 窗口看门狗初始化
 */
int drv_wwdg_init()
{
    int ret = DY_EOK;
    strcpy(g_wwdg.device.name, "wwdg");
    g_wwdg.device.ops = &g_wdg_ops;
    /*配置*/
    g_wwdg.cfg.wdg_type = WINDOW_WATCHDOG;
    g_wwdg.cfg.prescaler = WWDG_Prescaler_2;
    g_wwdg.cfg.window_value = 0x5F;
    g_wwdg.cfg.counter_value = 0x7F;    // [8.736ms, 17.48ms]

    ret = dy_device_register("wwdg", &g_wwdg.device);
    if (ret != DY_EOK)
    {
        return ret;
    }
    ret = g_wwdg.device.ops->init(&g_wwdg.device);
    
    return ret;
}

