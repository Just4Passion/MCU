
#ifndef DRV_EEPROM_H
#define DRV_EEPROM_H

#include "kservice.h"
/**********************************************
 * 
 *                  类型定义
 * 
 ***********************************************/
typedef enum
{
    EEPROM_SET_MEM_ADDR = (DY_DEVICE_CTRL_CMD_MAX + 1),
}eeprom_ctrl_cmd_t;

/**********************************************
 * 
 *                  服务接口
 * 
 ***********************************************/
int drv_eeprom_init(void);


#endif

