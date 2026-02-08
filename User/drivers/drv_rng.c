
#include <stdbool.h>

#include "drv_rng.h"

/*****************************************
 * 
 * RNG处理器
 *      以连续模拟噪声为基础的随机数发生器
 *      两个连续随机数的间隔为40个PLL48CLK 时钟信号周期: 
 *      通过监视 RNG 熵来标识异常行为（产生稳定值，或产生稳定的值序列）
 * 错误管理
 *      时钟错误
 *      种子错误
 * 中断管理
 * 
 ******************************************/
typedef void(*rng_irq_handler)();
typedef struct
{
    /*时钟*/
    uint32_t periph_clk;

    bool irq_enable;
    /*通道, 优先级*/
    uint16_t it_flags;   //使能哪些中断标志位
    uint8_t irq_channel;
    uint8_t group_priority;
    uint8_t sub_priority;
    rng_irq_handler rng_handler;
}rng_cfg_t;

typedef struct
{
    dy_device_t device;
    rng_cfg_t cfg;
    uint32_t rng_errno;
}rng_dev_t;

rng_dev_t g_rng_dev;

static void rng_init(dy_device_t *dev)
{
    rng_dev_t *rng = (rng_dev_t*)dev;
    RCC_AHB2PeriphClockCmd(rng->cfg.periph_clk, ENABLE);
    RNG_Cmd(ENABLE);

    if (rng->cfg.irq_enable)
    {
        /*使能中断标志位*/
        RNG_ITConfig(ENABLE);

        /*使能NVIC中断号*/
        NVIC_InitTypeDef stNVICInit = {0};
        stNVICInit.NVIC_IRQChannel = rng->cfg.irq_channel;
        stNVICInit.NVIC_IRQChannelPreemptionPriority = rng->cfg.group_priority;
        stNVICInit.NVIC_IRQChannelSubPriority = rng->cfg.sub_priority;
        stNVICInit.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&stNVICInit);
    }
    return DY_EOK;
}

static void rng_control(dy_device_t *dev, int cmd, void *arg)
{
    rng_dev_t *rng = (rng_dev_t*)dev;
    switch(cmd)
    {
        case RNG_START:
            RNG_Cmd(ENABLE);
            break;
        case RNG_STOP:
            RNG_Cmd(DISABLE);
            break;
        case RNG_GET_RANDOM:
            while(RNG_GetFlagStatus(RNG_FLAG_DRDY)== RESET);
            *(uint32_t*)arg = RNG_GetRandomNumber();
            break;
        case RNG_EXE_IRQ:
            /*执行中断*/
            if(rng->cfg.irq_enable && rng->cfg.rng_handler != NULL)
            {
                rng->cfg.rng_handler();
            }
            break;
        default:
            break;
    }
    return DY_EOK;
}

device_ops_t g_rng_dev_ops = {
    .init = rng_init,
    .control = rng_control
};

int drv_rng_init()
{
    int ret = DY_EOK;
    strncpy(g_rng_dev.device.name, "rng", sizeof(g_rng_dev.device.name) - 1);
    g_rng_dev.device.ops = &g_rng_dev_ops;

    g_rng_dev.cfg.periph_clk = RCC_AHB2Periph_RNG;
    g_rng_dev.cfg.irq_enable = false;

    if (DY_EOK != dy_device_register(g_rng_dev.device.name, &g_rng_dev.device))
    {
        return DY_ERROR;
    }
    ret = g_rng_dev.device.ops->init(&g_rng_dev.device);

    return DY_EOK;
}

