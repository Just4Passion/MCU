
#ifndef DRV_FSMC_H
#define DRV_FSMC_H

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"

/*****************************************
 * 
 * FSMC：灵活静态存储器控制器
 *      1、支持多种存储器类型
 *          SRAM/PSRAM
 *          NOR Flash/OneNAND Flash
 *          NAND Flash
 *          PC Card
 *          LCD
 *      2、支持8位, 16位, 32位数据总线
 *      3、最多4个存储区域(Bank), 每个256MB
 *      4、独立的时序配置: 每个Bank有独立的时序寄存器
 *      5、地址和数总线复用
 *      6、支持异步和同步访问
 * 
 * FSMC架构
 *      1、4个Bank接入存储器类型
 *          (1)Bank1: NOR Flash/PSRAM
 *          (2)Bank2, Bank3: NAND Flash
 *          (3)Bank4: PC Card
 *      2、4个Bank地址范围
 *          (1)Bank1: 0x6000 0000 - 0x6FFF FFFF, 片选NE[4:1](Bank1可以进一步划分为4个区域, 每个64MB)
 *          (2)Bank2: 0x7000 0000 - 0x7FFF FFFF, 片选NCE2
 *          (3)Bank3: 0x8000 0000 - 0x8FFF FFFF, 片选NCE3
 *          (4)Bank4: 0x9000 0000 - 0x9FFF FFFF, 片选NCE4_1/NCE4_2
 *      3、引脚
 *          (1)地址总线: FSMC_A[25:0]
 *          (2)数据总线: FSMC_D[15:0], FSMC_D[31:16]
 *          (3)控制信号
 *              FSMC_NE[4:1]: 片选Bank1
 *              FSMC_NCE[4:2]: 片选Bank2-4
 *              FSMC_NOE: 输出使能
 *              FSMC_NWE: 写使能
 *              FSMC_NWAIT: 等待信号
 *              FSMC_NL: 锁存使能(地址锁存)
 *              FSMC_INT: 中断信号
 *          (4)存储器功能特性引脚
 *              NOR Flash专用
 *                  FSMC_NADV: 地址有效信号
 *                  FSMC_CLK: 时钟信号
 *              NADN Flash专用
 *                  FSMC_NCE2/NCE3: 片选
 *                  FSMC_CLE: 命令锁存信号
 *                  FSMC_ALE: 地址锁存使能
 *              PC Card
 *                  FSMC_CD: 卡检测
 *                  FSMC_IOWR/IORD: IO读写
 *                  FSMC_INPACK: 输入应答
 * 
 ******************************************/

/*****************************************
 * FSMC控制器特点
 *      可以通过片选信号选择多个设备(类似支持地址的总线)
 *      控制器的每个Bank可以被视为一个独立的设备, 但是它们共享一个FSMC控制器硬件资源
 *      
 * FSMC抽象设计
 *      FSMC全局管理结构, 用于记录FSMC控制器的初始状态和Bank的使用情况 - 这意味着要根据不同配置重新初始化FSMC
 *      每个Bank设备在初始化时, 检查全局FSMC, 如果FSMC未初始化则进行初始化, 并标记已初始化
 *      每个Bank设备在打开设备时, 检查所对应的Bank是否已被占用, 如果被占用则返回错误
 *      每个Bank设备在关闭时, 释放对Bank的占用
 * 
 * FSMC: 思考如何做多设备管理 - 先仅做单个设备的管理
 * 
 ******************************************/


/*****************************************
 * 
 *          存储器类型定义
 * 
 ******************************************/
typedef struct
{
    FSMC_DEVICE_SRAM = 0,
    FSMC_DEVICE_NOR_FLASH,
    FSMC_DEVICE_NAND_FLASH,
    FSMC_DEVICE_LCD,    // 8080接口
}fsmc_device_type_t;


/*每个bank的配置*/
typedef struct
{
    uint32_t base_address;      // Bank基地址
    uint32_t size;              // Bank总大小
    uint8_t data_width;         // 数据宽度
    uint8_t mem_type;           // 存储器类型
    bool enabled;               // Bank使能状态
}fsmc_bank_info_t;

typedef struct
{

}fsmc_config_t;                 // 不同的Bank, 有不同的config

/**/
typedef struct 
{
    dy_device_t device;
    /********************************************************
     * 如果有不同的设备注册进来
     * 当某个设备执行了open之后, 其他设备就无法执行open了
     * 当某个设备close之后, 其他设备可以执行初始化了
     * 
     * 
     *********************************************************/
    uint8_t attach_dev_open_flag;   //最多挂8个设备, 每个设备还有编号
    fsmc_bank_info_t *probe_bank_dev;
    int dev_num;

    /*这里要如何设计*/

}fsmc_dev_t;                    // 获取到config, 然后调用init去配置

#endif

