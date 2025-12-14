
#ifndef DRV_SERIAL_H
#define DRV_SERIAL_H

/******************************************************
 * 
 *                      类型
 * 
 *******************************************************/
typedef enum
{
    SERIAL_SET_BAUD_RATE = (DY_DEVICE_CTRL_CMD_MAX + 1),       // 设置波特率
}serial_ctrl_cmd_t;

/**********************************************
 * 
 *                  服务接口
 * 
 ***********************************************/
int drv_serial_init(void);


#endif
