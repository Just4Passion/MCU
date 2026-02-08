
#ifndef DRV_SPI_FLASH_H
#define DRV_SPI_FLASH_H


#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"

/**********************************************
 * Flash: NOR and NAND
 *  
 * NOR Flash特性: 随机读取快(可以块内执行)
 *      1、特点
 *          读取速度块
 *          接口简单: SPI
 *      2、硬件设计
 *          存储单元并联设计
 * 
 * NAND Flash特性: 连续写入或连续读取快
 *      1、特点
 *          写入/擦除速度快; 以块为单位擦除
 *          接口复杂: 需要专门的控制器
 *          需要坏块管理和ECC校验
 *      2、硬件设计
 *          存储单元串联设计
 * 
 * 其他基于NAND和NOR Flash的衍生品
 *      eMMC: 将NAND和控制器集成在一个BGA封装内. 是智能手机、平板电脑等移动设备的主流存储方案
 *      UFS: eMMC的高性能替代者
 *      SSD: 原理上与eMMC/UFS类似, 但是接口(SATA, NVMe)和形态面向高性能计算领域
 *      3D NAND: NAND Flash在存储单元结构上革新
 *      SPI NAND: 使用NAND, 但是配备了简单的片上ECC, 并提供了与NOR兼容的SPI接口
 * 
 ***********************************************/



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
