
#ifndef DRV_I2C_H
#define DRV_I2C_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"


#define DY_I2C_WR                0x0000    // 写
#define DY_I2C_RD               (1u << 0)  // 读
#define DY_I2C_ADDR_10BIT       (1u << 2)  // 地址为10位模式
#define DY_I2C_NO_START         (1u << 4)  // 不生成起始信号条件
#define DY_I2C_IGNORE_NACK      (1u << 5)  // 忽略从设备的NACK
#define DY_I2C_NO_READ_ACK      (1u << 6)  // 读的时候, 不回ACK
#define DY_I2C_NO_STOP          (1u << 7)  // 不产生STOP信号


#define DY_I2C_ESTOP_TMOUT          (-6)    // 总线发送停止信号超时
#define DY_I2C_EREAD_TMOUT          (-5)    // 总线读取超时
#define DY_I2C_EWRITE_TMOUT         (-4)    // 总线发送超时
#define DY_I2C_EADDSEND_TMOUT       (-3)    // 地址发送超时
#define DY_I2C_ESBSEND_TMOUT        (-2)    // 总线发送起始信号超时
#define DY_I2C_EBSY_TMOUT           (-1)    // 总线忙超时
#define DY_I2C_EOK                  (0)


typedef enum
{
    DY_I2C_CTRL_CMD_RESET = (DY_BUS_CTRL_CMD_MAX + 1),
    DY_I2C_CTRL_CMD_DETECT_DEV,             // 检测设备是否忙
    DY_I2C_CTRL_CMD_SET_SEND_STOP_FLAG
}dy_i2c_bus_ctl_cmd_t;

typedef struct
{
    uint32_t dev_addr;      // 设备地址
    uint32_t timeout_ms;       // 超时时间: ms
}dy_i2c_bus_ctl_cmd_detect_dev_t;

/*IIC消息结构体*/
typedef struct
{
    uint16_t addr;      // 设备地址
    uint16_t flags;     // 读写标志
    uint16_t len;       // 数据长度
    uint8_t  *buf;      // 数据buffer
}dy_i2c_msg;
/*我需要一个i2c设备模型, 还需要一个i2c总线模型*/

/**********************************************
 * 
 * 
 *                  硬件 IIC 设备模型: 根据特定的模型而定, 这里不做设计
 * 
 * 
************************************************/


/**********************************************
 * 
 * 
 *                  硬件 IIC 总线模型
 * 
 * 
************************************************/
typedef struct
{
    I2C_TypeDef *i2c_periph;        //I2C外设地址
    uint32_t periph_clk;            //I2C外设时钟

    uint32_t scl_clk;               // SCL时钟线的时钟配置
    GPIO_TypeDef *scl_port;        // SCL时钟端口
    uint16_t scl_pin;               // SCL时钟引脚
    //uint32_t scl_af;              // SCL引脚复用

    uint32_t sda_clk;               // 数据时钟配置
    GPIO_TypeDef * sda_port;              // 数据端口
    uint16_t sda_pin;               // 数据引脚
    //uint32_t sda_af;              // 数据引脚复用

    IRQn_Type ev_irq_type;          // 事件中断
    IRQn_Type er_irq_type;          // 错误中断
    uint32_t i2c_clock_hz;          // I2C时钟频率: 100kbit/s, 400kbit/s, 3.4Mbit/S

    uint16_t dev_addr;              // IIC地址
}dy_stm32_i2c_bus_config_t;


typedef struct
{
	int (*master_xfer)(dy_bus_t *bus, dy_i2c_msg msgs[], uint32_t num);
    int (*slave_xfer)(dy_bus_t *bus, dy_i2c_msg msgs[], uint32_t num);
    int (*i2c_bus_control)(dy_bus_t *bus, int cmd, void *args);
}dy_i2c_bus_ops_t;


/*在头文件中声明, 破坏了结构的私密性, 应该让bus.ops->control控制所有参数*/
typedef struct 
{
    dy_bus_t bus;
    dy_i2c_bus_ops_t ops;
    bool send_stop;     // 是否发送结束信号: 不发则都在一个事务中
}dy_i2c_bus_t;


/**********************************************
 * 
 *                  服务接口
 * 
 ***********************************************/
int drv_i2c_hw_init(void);

#endif














