
#ifndef DRV_SPI_FLASH_H
#define DRV_SPI_FLASH_H


#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"
/**********************************************
 * 
 *                  类型定义
 * 
 ***********************************************/
typedef enum
{
    W25Q128_SET_MEM_ADDR = (DY_DEVICE_CTRL_CMD_MAX + 1),
    W25Q128_READ_CHIP_ID,   //读取芯片ID
    W25Q128_CHIP_ERASE,     //擦除整个芯片
    W25Q128_WRITE_ENABLE,   //写使能
    W25Q128_POWER_DOWN,     //进入powerdown模式
    W25Q128_WAKE_UP,        //唤醒芯片
}flash_W25Q128_ctrl_cmd_t;

/**********************************************
 * 
 *                  服务接口
 * 
 ***********************************************/
int drv_spi_flash_init();


#endif
