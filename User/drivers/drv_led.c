
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"

#include "drv_misc_func.h"


/**********************************************
 * 
 *                  类型定义
 * 
***********************************************/
typedef enum
{
    LED_OFF,
    LED_RED_ON,
    LED_GREEN_ON,
    LED_BLUE_ON,
    LED_WHITE_ON
}led_ctrl_cmd_t;

typedef struct
{
    /*有哪些灯, 各个灯占用的端口和引脚*/
    /*红色*/
    GPIO_TypeDef *gpio_red_port;
    uint16_t gpio_red_pin;
    /*绿色*/
    GPIO_TypeDef *gpio_green_port;
    uint16_t gpio_green_pin;
    /*蓝色*/
    GPIO_TypeDef *gpio_blue_port;
    uint16_t gpio_blue_pin;
}led_config_t;

typedef struct
{
    dy_device_t device;
    led_config_t led_cfg;
}led_dev_t;


/**********************************************
 * 
 *                  局部变量
 * 
***********************************************/
static led_dev_t g_led_dev = {0};



/**********************************************
 * 
 *                  函数定义
 * 
***********************************************/
/**
 * @brief led初始化
 */
static int led_init(void *dev)
{
    led_dev_t *led_dev = (led_dev_t*)dev;
    if (NULL == dev)
    {
        return DY_ERROR;
    }
    
    GPIO_InitTypeDef stIOInit;

	/*启用时钟: 时钟, 需要一层时钟到端口的映射关系*/
    drv_gpio_enable_clk((uint32_t)(led_dev->led_cfg.gpio_red_port));
    drv_gpio_enable_clk((uint32_t)(led_dev->led_cfg.gpio_green_port));
    drv_gpio_enable_clk((uint32_t)(led_dev->led_cfg.gpio_blue_port));
    //RCC_AHB1PeriphClockCmd(, ENABLE);
    //RCC_AHB1PeriphClockCmd(, ENABLE);
    //RCC_AHB1PeriphClockCmd(, ENABLE);

    /*配置端口*/
	stIOInit.GPIO_Mode = GPIO_Mode_OUT;
	stIOInit.GPIO_OType = GPIO_OType_PP;
	stIOInit.GPIO_Speed = GPIO_Speed_2MHz;
	stIOInit.GPIO_PuPd = GPIO_PuPd_UP;

	/*红色*/
	stIOInit.GPIO_Pin = led_dev->led_cfg.gpio_red_pin;
	GPIO_Init(led_dev->led_cfg.gpio_red_port, &stIOInit);
	GPIO_SetBits(led_dev->led_cfg.gpio_red_port, led_dev->led_cfg.gpio_red_pin);

	/*绿色*/
	stIOInit.GPIO_Pin = led_dev->led_cfg.gpio_green_pin;
	GPIO_Init(led_dev->led_cfg.gpio_green_port, &stIOInit);
	GPIO_SetBits(led_dev->led_cfg.gpio_green_port, led_dev->led_cfg.gpio_green_pin);

	/*蓝色*/
	stIOInit.GPIO_Pin = led_dev->led_cfg.gpio_blue_pin;
	GPIO_Init(led_dev->led_cfg.gpio_blue_port, &stIOInit);
	GPIO_SetBits(led_dev->led_cfg.gpio_blue_port, led_dev->led_cfg.gpio_blue_pin);

    return DY_EOK;
}

/**
 * @brief led控制
 */
static int led_control(void *dev, int cmd, void *arg)
{
    if (NULL == dev)
    {
        return DY_ERROR;
    }
    led_dev_t *led_dev = (led_dev_t *)dev;
    /******************************************
     * 主要控制开关灯: 开红灯, 开绿灯, 开蓝灯, 开白灯
     * 如何控制闪烁: 闪烁是需要配合时间函数实现的
     * 比如定时器回调中启用定时器, 完成闪烁的次数后, 关闭定时器
     * ***************************************/
    switch (cmd)
    {
        case LED_OFF:
            GPIO_SetBits(led_dev->led_cfg.gpio_red_port, led_dev->led_cfg.gpio_red_pin);
            GPIO_SetBits(led_dev->led_cfg.gpio_green_port, led_dev->led_cfg.gpio_green_pin);
            GPIO_SetBits(led_dev->led_cfg.gpio_blue_port, led_dev->led_cfg.gpio_blue_pin);
            break;
        case LED_RED_ON:
            GPIO_ResetBits(led_dev->led_cfg.gpio_red_port, led_dev->led_cfg.gpio_red_pin);
            GPIO_SetBits(led_dev->led_cfg.gpio_green_port, led_dev->led_cfg.gpio_green_pin);
            GPIO_SetBits(led_dev->led_cfg.gpio_blue_port, led_dev->led_cfg.gpio_blue_pin);
            break;
        case LED_GREEN_ON:
            GPIO_SetBits(led_dev->led_cfg.gpio_red_port, led_dev->led_cfg.gpio_red_pin);
            GPIO_ResetBits(led_dev->led_cfg.gpio_green_port, led_dev->led_cfg.gpio_green_pin);
            GPIO_SetBits(led_dev->led_cfg.gpio_blue_port, led_dev->led_cfg.gpio_blue_pin);
            break;
        case LED_BLUE_ON:
            GPIO_SetBits(led_dev->led_cfg.gpio_red_port, led_dev->led_cfg.gpio_red_pin);
            GPIO_SetBits(led_dev->led_cfg.gpio_green_port, led_dev->led_cfg.gpio_green_pin);
            GPIO_ResetBits(led_dev->led_cfg.gpio_blue_port, led_dev->led_cfg.gpio_blue_pin);
            break;
        case LED_WHITE_ON:
            GPIO_ResetBits(led_dev->led_cfg.gpio_red_port, led_dev->led_cfg.gpio_red_pin);
            GPIO_ResetBits(led_dev->led_cfg.gpio_green_port, led_dev->led_cfg.gpio_green_pin);
            GPIO_ResetBits(led_dev->led_cfg.gpio_blue_port, led_dev->led_cfg.gpio_blue_pin);
            break;
    }

    return DY_EOK;
}


/**
 * @brief led操作驱动
 */
static device_ops_t led_dev_ops = 
{
    .init = led_init,
    .open = NULL,           // open 和 close 可以用于一些低功耗的场景
    .close = NULL,
    .read = NULL, 
    .write = NULL,
    .control = led_control,
    .callback = NULL
};

/**
 * @brief led注册到设备管理器中
 */
int drv_led_register(void)
{
    strncpy(g_led_dev.device.name, "led", sizeof(g_led_dev.device.name) - 1);
    g_led_dev.device.ops = &led_dev_ops;

    g_led_dev.led_cfg.gpio_red_port = GPIOF;
    g_led_dev.led_cfg.gpio_red_pin = GPIO_Pin_6;

    g_led_dev.led_cfg.gpio_green_port = GPIOF;
    g_led_dev.led_cfg.gpio_green_pin = GPIO_Pin_7;

    g_led_dev.led_cfg.gpio_blue_port = GPIOF;
    g_led_dev.led_cfg.gpio_blue_pin = GPIO_Pin_8;

    if (DY_EOK != dy_device_register(g_led_dev.device.name, &g_led_dev.device))
    {
        return DY_ERROR;
    }

    return DY_EOK;
}
