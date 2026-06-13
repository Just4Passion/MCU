
#ifndef DRV_LED_H
#define DRV_LED_H

/**********************************************
 * 
 *                  类型定义
 * 
 ***********************************************/
typedef enum
{
    LED_OFF = (DY_DEVICE_CTRL_CMD_MAX + 1),
    LED_RED_ON,
    LED_GREEN_ON,
    LED_BLUE_ON,
    LED_WHITE_ON
}led_ctrl_cmd_t;

/**
 * @brief led注册到设备管理器中并初始化
 */
int drv_led_init(void);

#endif
