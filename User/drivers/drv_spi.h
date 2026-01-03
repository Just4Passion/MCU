#ifndef DRV_SPI_H
#define DRV_SPI_H


#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"


#include "kservice.h"


#define DY_SPI_RCV_TMOUT            (-2)    // 接收超时
#define DY_SPI_SND_TMOUT            (-1)    // 发送超时
#define DY_SPI_EOK                  (0)


/***********************************************************
 * 
 *                      SPI协议
 * 1、4根信号线: SCLK, MOSI, MISO, CS/SS(拉低选中)
 * 2、时钟信号的极性和相位
 *      (1)极性: CPOL=0, 空闲时, 时钟信号为低电平(第一个边沿为上升沿); CPOL=1, 空闲时, 时钟信号为高电平(第一个边沿为下降沿)
 *      (2)相位: CPHA=0, 数据在时钟第一个边沿被采样; CPHA=1, 数据在时钟第二个边沿被采样
 * 3、优缺点
 *      (1)优点: 快, 简单
 *      (2)缺点: 无硬件应答机制; 信号线多; 只有一个主设备; 没有标准的流控和错误检查; 数据长度、字节序标准未统一
 * 4、应用: 
 *      (1)存储器: Flash, EEPROM, FRAM
 *      (2)传感器: 加速计/陀螺仪, 气压计, 数字温度传感器
 *      (3)显示模块：OLED, TFT屏幕的控制器
 *      (4)通信模块: 以太网控制器, WiFi/蓝牙模块
 * 5、变种与扩展
 *      (1)Dual-SPI, Quad-SPI: 更高带宽
 *      (2)3线SPI: 半双工模式, 共用一根数据线(SDIO)
 * 
 ************************************************************/

typedef enum
{
    DY_SPI_CTRL_CMD_CS_LOW = (DY_BUS_CTRL_CMD_MAX + 1), // 拉低片选
    DY_SPI_CTRL_CMD_CS_HIGH,                            // 拉高片选
}dy_spi_bus_ctl_cmd_t;

typedef struct
{
    GPIO_TypeDef* port;
    uint32_t clk;
    uint16_t pin;
    uint8_t pin_src;
    uint8_t af;
}dy_stm32_gpio_t;

typedef struct 
{
    /*SCLK*/
    dy_stm32_gpio_t sclk;
    /*MOSI*/
    dy_stm32_gpio_t mosi;
    /*MISO*/
    dy_stm32_gpio_t miso;
    /*****************************************************
     *                          片选
     *   这样当存在多个设备时, 总线可以拉低选中的器件
     *   如果设备从总线的设备链表中进行拉高拉低, 则需要访问其他设备的私有数据
     ****************************************************/
    uint8_t sdev_num;      // 从设备数量
    dy_stm32_gpio_t cs;     // 当前先仅支持1个从设备, 可能存在多个.

    SPI_TypeDef* spi_periph;
    uint32_t periph_clk;

    /************************************************************
     *  
     *                      一些中断
     *  1、错误标志
     *      (1)主模式故障
     *      (2)溢出错误: 主器件发送数据, 从器件尚未读取上一个字节
     *      (3)CRC错误
     *      (4)T1模式帧格式错误
     *  2、SPI中断
     *      (1)发送缓冲区为空: TXE/TXEIE
     *      (2)接收缓冲区非空: RXNE/RXNEIE
     *      (3)主模式故障: MODF/ERRIE
     *      (4)溢出错误: OVR/ERRIE
     *      (5)CRC错误: CRCERR/ERRIE
     *      (6)TI帧格式错误: FRE/ERRIE
     *************************************************************/
    /*一些配置*/
    IRQn_Type ev_irq_type;      // 中断事件

    /***********************************************************
     * 
     *                      基础配置
     * 多个从设备
     *      当存在多个从设备时, 此时可以配置NSS引脚为软件管理, 这样NSS不再自动控制片选. 
     *      此时使用多个普通的GPIO作为片选
     ***********************************************************/
    /*模式: 全双工, 半双工(和I2C有点像了, 但是有片选)*/
    uint16_t spi_direction;
    uint16_t spi_mode;
    uint16_t spi_datasize;
    uint16_t spi_cpol;
    uint16_t spi_cpha;
    uint16_t spi_nss;                   //硬件自动管理, 为主时, 自动输出高电平; 为从时, 自动检测; 软件管理, 可通过寄存器位来设置NSS信号电平
    uint16_t spi_baudrate_prescaler;    //通过波特兰预分频器控制传输速率, 最大值Fpclk/2
    uint16_t spi_first_bit;             //LSM, MSB
    uint16_t spi_crc_polynmial;         //CRC多项式: 幂次和系数构成的多项式, 用来标识一个CRC校验的值. 字节流和这个值异或
}dy_stm32_spi_bus_config_t;

typedef struct
{
    dy_bus_t bus;           //
    uint32_t spi_timeout_ms;   //ms
}dy_spi_bus_t;


/**********************************************
 * 
 *                  服务接口
 * 
 ***********************************************/
int drv_spi_hw_init(void);

#endif
