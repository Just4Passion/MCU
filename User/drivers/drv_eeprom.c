#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kservice.h"

#include "drv_i2c.h"
#include "drv_time.h"
#include "drv_eeprom.h"

/**********************************************
 * 
 *                  关于AT24C02
 * 写入
 *      字节写入: 一次写入一个数据字节
 *          发送器件地址+写标志; 发送8位的字地址; 发送8位数据; 发送停止信号.
 *          EEPROM进入写周期, 所有输入无效
 *      页写入: 连续写入一页数据. 超过一页, 地址回卷到页开头. 8字节页
 *          发送器件地址+写标志; 发送8位的字地址; 发送8位数据, 直到一页发送完毕; 发送停止信号
 *      写入需要时间, AT24C02会启动内部自定时写周期. 此时芯片不会响应I2C总线. 因此主控需要通过查询应答确保写入完成
 * 
 * 读取
 *      当前地址读: 发送器件地址+读标志; 接收8位数据(EEPROM中保存了上次访问的地址+1的值); 发送停止条件
 *      随机读: 读取指定地址的数据
 *          发送器件地址+写标志; 发送8位的字地址; 发送器件地址+读标志; 接收8位数据; 发送停止条件
 *      顺序读: 主设备在读取每个字节后回复应答, 内部地址计数器自动递增
 * 
 * 复位
 *      当协议产生中断、掉电或系统复位后. I2C总线可通过以下步骤复位
 *          (1)产生9个时钟周期
 *          (2)当SCL为高时, SDA也为高
 *          (3)产生一个起始条件
 * 
***********************************************/


/**********************************************
 * 
 *                  宏定义
 * 
***********************************************/
#define EEPROM_MEM_ADDR_SIZE    (1)
#define EEPROM_PAGE_SIZE        (8)
#define EEPROM_WRITE_TIMEOUT_MS (10)
#define EEPROM_RETRY_COUNT (3)

#define DY_STORAGE_EEPROM_EBUSY       0x1000  // EEPROM忙
#define DY_STORAGE_EEPROM_ETIMEOUT    0x1001  // EEPROM操作超时
#define DY_STORAGE_EEPROM_EADDR       0x1002  // 地址错误
#define DY_STORAGE_EEPROM_EDATA       0x1003  // 数据错误
/**********************************************
 * 
 *                  类型定义
 * 
***********************************************/
typedef struct
{
    /***********************************
     * 
     * 这是一个容量只有256字节的EEPROM
     * 因此, 它的寻址范围是 0 -255
     * 读取和写入都要指定其内部的地址 —— 因为没有文件系统
     * 
     * AT24C02板接
     *  VCC, GND均未与MCU相连
     *  A0, A1, A2用于指示EEPROM设备地址, 三者均接地
     *  WP接入地
     *  EEPROM芯片的设备地址一共有7位，其中高4位固定为：1010. 最后一位是读写标志
     * 
     ***********************************/
    uint8_t dev_addr;           // 设备地址
    uint8_t mem_addr;           // 存储地址
    uint16_t mem_capacity;      // 存储容量, 单位字节
}eeprom_AT24C02_cfg_t;

typedef struct
{
    dy_device_t device;
    eeprom_AT24C02_cfg_t cfg;   // 这里应该使用priv_data = &cfg, 目前先根据板子定制

    uint32_t write_timeout_ms;  // 写超时时间
    uint8_t max_retry_count;    // 最大重试次数
    uint32_t error_count;       // 错误次数
}eeprom_dev_t;


eeprom_dev_t g_eeprom_dev;

static int eeprom_init(dy_device_t *dev)
{
    //eeprom_dev_t *eeprom = (eeprom_dev_t *)dev;
    dy_i2c_bus_t *i2c_bus = (dy_i2c_bus_t *)dev->bus;
    /*检查总线是否初始化*/
    if (NULL == i2c_bus)
    {
        return DY_EINVAL;
    }
    /*检查总线状态是否已经初始化*/
    if (DY_BUS_STATE_UNINITIALIZED == i2c_bus->bus.state)
    {
        return DY_EBUSNOINIT;   // 总线未初始化
    }
    /*初始化设备: 根据硬件接线, VCC, GND, A0, A1, A2均无需初始化*/

    return DY_EOK;
}

/**
 * @brief 按字节写入: 一次IIC通信写入一个字节. 测试完成
 */
static int eeprom_write_bytes(dy_device_t *dev, void *buf, unsigned int len)
{
    /*一次写入一个字节, 地址递增*/
    eeprom_dev_t *eeprom_dev = (eeprom_dev_t *)dev;
    dy_i2c_bus_t *i2c_bus = (dy_i2c_bus_t*)(dev->bus);
    uint8_t *data = (uint8_t*)buf;
    uint8_t data_buf[2] = {0};
    dy_i2c_msg i2c_msg = {0};
    uint32_t i = 0;
    int ret = DY_EOK;

    dy_i2c_bus_ctl_cmd_detect_dev_t dev_det = {.dev_addr = eeprom_dev->cfg.dev_addr, .timeout_ms = eeprom_dev->write_timeout_ms};

    if (NULL == dev || NULL == buf)
    {
        return DY_EINVAL;
    }

    if (0 == len)
    {
        return len;
    }

    i2c_msg.addr = eeprom_dev->cfg.dev_addr;
    i2c_msg.flags = 0;
    i2c_msg.buf = data_buf;
    i2c_msg.len = 2;
    for (i = 0; i < len; ++i)
    {
        /*检查EEPROM是否准备好了*/
        ret = i2c_bus->ops.i2c_bus_control(dev->bus, DY_I2C_CTRL_CMD_DETECT_DEV, (void*)&dev_det);
        if (DY_EOK != ret)
        {
            return i;
        }
        /*准备好了则写入*/
        data_buf[0] = eeprom_dev->cfg.mem_addr + i;
        data_buf[1] = data[i];
        if (1 != i2c_bus->ops.master_xfer((dy_bus_t *)i2c_bus, &i2c_msg, 1))
        {
            return i;
        }
    }

    return len;
}

/**
 * @brief 按页写入: 一次IIC通信写入一页数据
 * @note 进行数据校验. 避免地址回卷
 */
static int eeprom_write_page(dy_device_t *dev, void *buf, unsigned int len)
{
    /*页写入*/
    eeprom_dev_t *eeprom_dev = (eeprom_dev_t *)dev;
    dy_i2c_bus_t *i2c_bus = (dy_i2c_bus_t*)(dev->bus);
    //uint8_t *data = (uint8_t*)buf;
    uint8_t data_page[EEPROM_MEM_ADDR_SIZE + EEPROM_PAGE_SIZE] = {0};
    dy_i2c_msg i2c_msg = {0};
    int ret = DY_EOK;

    if (NULL == dev || NULL == buf || len > EEPROM_PAGE_SIZE)
    {
        return DY_EINVAL;
    }

    dy_i2c_bus_ctl_cmd_detect_dev_t dev_det = {.dev_addr = eeprom_dev->cfg.dev_addr, .timeout_ms = eeprom_dev->write_timeout_ms};

    if (0 == len)
    {
        return len;
    }

    i2c_msg.addr = eeprom_dev->cfg.dev_addr;
    i2c_msg.flags = 0;
    i2c_msg.buf = data_page;
    i2c_msg.len = EEPROM_MEM_ADDR_SIZE + len;

    /*页地址*/
    memcpy(&data_page[0], &eeprom_dev->cfg.mem_addr, EEPROM_MEM_ADDR_SIZE);
    memcpy(&data_page[EEPROM_MEM_ADDR_SIZE], buf, len);
    if (1 != i2c_bus->ops.master_xfer((dy_bus_t *)i2c_bus, &i2c_msg, 1))
    {
        return DY_ERROR;
    }
    /*检查EEPROM是否写入完成*/
    ret = i2c_bus->ops.i2c_bus_control(dev->bus, DY_I2C_CTRL_CMD_DETECT_DEV, (void*)&dev_det);
    if (DY_EOK != ret)
    {
        return DY_ETMOUT;
    }

    return len;
}

/**
 * @brief 页写入
 * @note 检查地址. 如果是页地址, 全部按页写入; 如果不是页地址, 先按页写入多余的几个, 后续全部按页写入
 */
static int eeprom_write_pages(dy_device_t *dev, void *buf, unsigned int len)
{
    eeprom_dev_t *eeprom_dev = (eeprom_dev_t *)dev;
    dy_i2c_bus_t *i2c_bus = (dy_i2c_bus_t*)(dev->bus);
    uint8_t *data = (uint8_t*)buf;

    if (NULL == dev || NULL == buf)
    {
        return DY_EINVAL;
    }
    
    if (0 == len)

    {
        return len;
    }

    bool send_stop = true;
    uint32_t i = 0;
    int ret = 0;
    uint8_t retry_count = 0;
    /*按页计算*/
    uint32_t w_page_num = 0, w_single_num = 0;
    uint32_t pad_count = 0, left_len = 0, firstw_len = 0;

    //send_stop = false;
    //i2c_bus->ops.i2c_bus_control(dev->bus, DY_I2C_CTRL_CMD_SET_SEND_STOP_FLAG, &send_stop);
    //send_stop = true;
    /*地址*/
    pad_count = (eeprom_dev->cfg.mem_addr % EEPROM_PAGE_SIZE);
    /*非页地址: 写入前面几个字节*/
    if (pad_count != 0)
    {
        firstw_len = EEPROM_PAGE_SIZE - pad_count;
        firstw_len = (firstw_len >= len ? len : firstw_len);
        for (retry_count = 0; retry_count < eeprom_dev->max_retry_count; retry_count++)
        {
            ret = eeprom_write_page(dev, data, firstw_len);
            if (ret == firstw_len)
            {
                break;
            }
            drv_time_delay_ms(1 << retry_count);    //指数退避
        }
        if (ret != firstw_len)
        {
            return DY_ERROR;
        }
        /*如果检查已经写完了, 则直接返回*/
        if (firstw_len == len)
        {
            return len;
        }
        eeprom_dev->cfg.mem_addr += firstw_len;
        data += firstw_len;
    }
    /*剩余一次写入一页*/
    left_len = (len - firstw_len);
    w_page_num = (left_len / EEPROM_PAGE_SIZE);
    w_single_num = (left_len % EEPROM_PAGE_SIZE);
    for (i = 0; i < w_page_num; i++)
    {
        for (retry_count = 0; retry_count < eeprom_dev->max_retry_count; retry_count++) 
        {
            ret = eeprom_write_page(dev, data, EEPROM_PAGE_SIZE);
            if (ret == EEPROM_PAGE_SIZE) 
            {
                break;
            }
            drv_time_delay_ms(1 << retry_count);
        }
        if (ret != EEPROM_PAGE_SIZE) 
        {
            return (i * EEPROM_PAGE_SIZE + firstw_len);
        }

        eeprom_dev->cfg.mem_addr += EEPROM_PAGE_SIZE;
        data += EEPROM_PAGE_SIZE;
    }
    /*最后写入不满足一页的剩余字节*/
    if (w_single_num != 0)
    {
        for (retry_count = 0; retry_count < eeprom_dev->max_retry_count; retry_count++) 
        {
            ret = eeprom_write_page(dev, data, w_single_num);
            if (ret == w_single_num)
            {
                break;
            }
            drv_time_delay_ms(1 << retry_count);
        }
        if (ret != w_single_num) 
        {
            return (w_page_num * EEPROM_PAGE_SIZE + firstw_len);
        }
    }
    
    return len;
}

/**
 * @brief eeprom读取: 测试完成
 */
static int eeprom_read(dy_device_t *dev, void *buf, unsigned int len)
{
    eeprom_dev_t *eeprom_dev = (eeprom_dev_t *)dev;
    dy_i2c_bus_t *i2c_bus = (dy_i2c_bus_t*)(dev->bus);

    dy_i2c_msg i2c_msg = {0};
    int ret = DY_EOK;

    if (NULL == dev || NULL == buf || len <= 0)
    {
        return DY_EINVAL;
    }
    dy_i2c_bus_ctl_cmd_detect_dev_t dev_det = {.dev_addr = eeprom_dev->cfg.dev_addr, .timeout_ms = eeprom_dev->write_timeout_ms};
    /*先等待eeprom可用*/
    ret = i2c_bus->ops.i2c_bus_control(dev->bus, DY_I2C_CTRL_CMD_DETECT_DEV, (void*)&dev_det);
    if (ret != DY_EOK)
    {
        return DY_EBUSY;    // 设备忙
    }
    /*先写入存储地址*/
    i2c_msg.addr = eeprom_dev->cfg.dev_addr;
    i2c_msg.flags = 0;
    i2c_msg.buf = &(eeprom_dev->cfg.mem_addr);
    i2c_msg.len = 1;
    if (1 != i2c_bus->ops.master_xfer((dy_bus_t *)i2c_bus, &i2c_msg, 1))
    {
        return DY_ERROR;
    }
    /*然后开始读取*/
    i2c_msg.addr = eeprom_dev->cfg.dev_addr;
    i2c_msg.flags = 1;
    i2c_msg.buf = (uint8_t*)buf;
    i2c_msg.len = len;
    if (1 != i2c_bus->ops.master_xfer((dy_bus_t *)i2c_bus, &i2c_msg, 1))
    {
        return DY_ERROR;
    }
    return len;
}

/**
 * @brief eeprom控制函数
 */
static int eeprom_control(dy_device_t *dev, int cmd, void *arg)
{
    eeprom_dev_t *eeprom_dev = (eeprom_dev_t *)dev;
    switch (cmd)
    {
        /*设置内存地址*/
        case EEPROM_SET_MEM_ADDR:
            eeprom_dev->cfg.mem_addr = *((uint8_t*)arg);
            break;
        default:
            dy_device_control(dev, cmd, arg);
            break;
    }
    return DY_EOK;
}

static device_ops_t eeprom_ops = {
    .init = eeprom_init,
    .open = NULL,
    .read = eeprom_read,
    .write = eeprom_write_pages,
    //.write = eeprom_write_bytes,
    .control = eeprom_control,
    .callback = NULL
};

int drv_eeprom_init()
{
    int ret = DY_EOK;
    /*设备名称*/
    strcpy(g_eeprom_dev.device.name, "eeprom");
    /*设置驱动*/
    g_eeprom_dev.device.ops = &eeprom_ops;
    /*设备配置*/
    g_eeprom_dev.cfg.dev_addr = 0xA0;       // 高4位固定1010, 低3位A0-A2控制, 最后一位是读写方向
    g_eeprom_dev.cfg.mem_capacity = 256;    // 字节
    g_eeprom_dev.write_timeout_ms = EEPROM_WRITE_TIMEOUT_MS;
    g_eeprom_dev.max_retry_count = EEPROM_RETRY_COUNT;
    g_eeprom_dev.error_count = 0;
    if (DY_EOK != dy_device_register("eeprom", &g_eeprom_dev.device))
    {
        return DY_ERROR;
    }
    /*设备挂载到总线上*/
    dy_bus_t *bus = dy_bus_find("I2C1");
    if (NULL == bus)
    {
        return DY_ERROR;
    }
    dy_bus_attach_device(bus, &(g_eeprom_dev.device));

    /*设备初始化*/
    ret = g_eeprom_dev.device.ops->init(&g_eeprom_dev.device);
    return ret;
}







