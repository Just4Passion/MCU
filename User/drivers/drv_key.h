
#ifndef DRV_KEY_H
#define DRV_KEY_H

/**********************************************
 * 
 *                  类型定义
 * 
***********************************************/
typedef enum
{
    KEY_GET_STATE = (DY_DEVICE_CTRL_CMD_MAX + 1),              // 获取当前状态
    KEY_GET_LONG_PRESS_TIME,    // 获取长按时间
    KEY_GET_PRESS_TIMESTAMP,    // 获取按下的时间戳
    KEY_SET_PRESS_TIMESTAMP,    // 设置按下时间戳
    KEY_SET_RELEASE_TIMESTAMP,  // 设置释放时间戳
}key_ctrl_cmd_t;

/**********************************************
 * 
 *                  服务接口
 * 
 ***********************************************/
int drv_key_init(void);
void key1_timer_callback(void);

#endif

