
#include <string.h>
#include <stdio.h>

#include "drv_spi.h"

#include "drv_spi_flash.h"


/******************************************************************************
 * W25Q128是一种使用SPI通讯协议的NORFLASH存储器, 需要先擦除再写入
 * 它的CS/CLK/DIO/DO引脚分别连接到了STM32对应的SDI引脚NSS/SCK/MOSI/MISO
 * 
 * 1、硬件设计
 * 	(1)CS: 片选. 接入PG6
 * 	(2)CLK: 时钟. 接入PB3
 * 	(3)DIO: 输入. 接入PB5
 * 	(4)DO: 输出. 接入PB4
 * 	(5)WP: 写保护. 低电平时禁止写入. 直接接入电源, 不使用写保护
 *  (6)HOLD: 暂停通信. 低电平时, 通讯暂停. 数据输出引脚输出高阻抗, 时钟和数据输入引脚无效
 ******************************************************************************/


/**********************************************************************      
                            W25Q128指令集
***********************************************************************/
#define W25Q_WRITE_ENABLE       0X06
#define W25Q_WRITE_DISABLE      0x04
#define W25Q_READ_STATUS_REG    0x05    //读取状态寄存器: 返回状态寄存器的值S7-S0
#define W25Q_WRITE_STATUS_REG   0x01    //写状态寄存器, 需要参数
#define W25Q_READ_DATA          0x03
#define W25Q_FAST_READ_DATA     0x0B
#define W25Q_FAST_READ_DUAL     0x3B
#define W25Q_PAGE_PROGRAM       0x02
#define W25Q_BLOCK_ERASE        0xD8
#define W25Q_SECTOR_ERASE       0x20
#define W25Q_CHIP_ERASE         0xC7
#define W25Q_POWER_DOWN         0xB9
#define W25Q_RELEASE_POWER_DOWN 0xAB
#define W25Q_RELEASE_DEVICE_ID  0xAB
#define W25Q_MANU_DEVICE_ID     0x90
#define W25Q_JEDEC_DEVICE_ID    0x9F

/**********************************************************************      
                                状态寄存器标志
***********************************************************************/
#define W25Q_STATUS_REG_BUSY    0x01

/**********************************************************************      
                                Flash参数限制
***********************************************************************/
#define W25Q_PAGE_SIZE          (256)               // 一页256字节
#define W25Q_SECTOR_SIZE        (4 * 1024)          // 一节4KB
#define W25Q_BLOCK_SIZE        (16 * 4 * 1024)      // 一块16 Sector

/*超时时间*/
#define W25Q_FLASH_WAIT_FREE_TIMEOUT    (0xFFFF)

 /**********************************************
  * 这是一个存储器, 需要一个写入获取读取的地址
  * 
  * 
  * 
  * 
  ***********************************************/
typedef struct
{
    uint32_t mem_addr;           // 存储地址
    uint16_t mem_capacity;      // 存储容量, 单位字节: 16MB
}flash_W25Q128_cfg_t;

typedef struct 
{
    dy_device_t device;
    flash_W25Q128_cfg_t cfg;

    uint32_t write_timeout_ms;  // 写超时时间
    uint8_t max_retry_count;    // 最大重试次数
}flash_W25Q128_dev_t;

flash_W25Q128_dev_t g_flash_W25Q128_dev;

/**
 * @brief 初始化
 * @note 
 */
int drv_flash_W25Q128_init(dy_device_t *dev)
{
    /*判断总线的状态*/
    dy_spi_bus_t *spi_bus = (dy_spi_bus_t *)dev->bus;
    /*检查总线是否初始化*/
    if (NULL == spi_bus)
    {
        return DY_EINVAL;
    }
    /*检查总线状态是否已经初始化*/
    if (DY_BUS_STATE_UNINITIALIZED == spi_bus->bus.state)
    {
        return DY_EBUSNOINIT;   // 总线未初始化
    }
    /*初始化设备: 根据硬件接线, VCC, GND, WP, HOLD均无需初始化*/
    printf("spi flash init ok\r\n");
    return DY_EOK;
}

/**
 * @brief 发送命令
 * @note 
 */
static int drv_flash_W25Q128_send_data(dy_device_t *dev, void *buf, unsigned int len)
{
    dy_bus_t *bus = dev->bus;
    uint8_t *data = (uint8_t *)buf;
    int ret = 0;
    /*片选选中*/
    ret = bus->ops->control(bus, DY_SPI_CTRL_CMD_CS_LOW, NULL); //拉低片选
    /*发送写使能*/
    ret = bus->ops->send(bus, (void*)data, len); //发送写使能
    if (ret != len)
    {
        printf("drv_flash_W25Q128_send_data failed, cmd = 0x%x, ret = %d\r\n", data[0], ret);
        return DY_ERROR;
    }
    /*片选拉高*/
    ret = bus->ops->control(bus, DY_SPI_CTRL_CMD_CS_HIGH, NULL);
    return DY_EOK;
}

/**
 * @brief 读取数据
 * @note 
 */
int drv_flash_W25Q128_read_with_addr(dy_device_t *dev, uint32_t read_addr, void *buf, unsigned int len)
{
    int ret = 0;
    dy_bus_t *bus = dev->bus;
    uint8_t *data = (uint8_t *)buf;
    /*从地址中读取数据*/
    uint8_t read_data_cmd = W25Q_READ_DATA;
    uint8_t data_buf[4] = {
        read_data_cmd, 
        (read_addr & 0xFF0000) >> 16,
        (read_addr & 0xFF00) >> 8,
        (read_addr & 0xFF),
    };
    /*发送读取指令*/
    ret = bus->ops->control(bus, DY_SPI_CTRL_CMD_CS_LOW, NULL); //拉低片选
    ret = bus->ops->send(bus, (void*)data_buf, 4); //发送写使能
    if (ret != 4)
    {
        printf("drv_flash_W25Q128_read_with_addr send read cmd failed\r\n");
        return DY_ERROR;
    }
    /*读取*/
    ret = bus->ops->recv(bus, (void*)data, len);
    if (ret != len)
    {
        printf("drv_flash_W25Q128_read_with_addr failed recv failed\r\n");
        return DY_ERROR;
    }
    /*片选拉高*/
    ret = bus->ops->control(bus, DY_SPI_CTRL_CMD_CS_HIGH, NULL);
    return len;
}

/**
 * @brief 读取数据
 * @note 
 */
int drv_flash_W25Q128_read(dy_device_t *dev, void *buf, unsigned int len)
{
    int ret = 0;
    /*获取读取地址*/
    flash_W25Q128_dev_t *flash_dev = (flash_W25Q128_dev_t*)dev;
    uint32_t read_addr = flash_dev->cfg.mem_addr;
    ret = drv_flash_W25Q128_read_with_addr(dev, read_addr, buf, len);
    if (ret != len)
    {
        printf("drv_flash_W25Q128_read_with_addr failed\r\n");
    }
    return len;
}

/**
 * @brief 写使能
 * @note 
 */
static int drv_flash_W25Q128_WriteEnable(dy_device_t *dev)
{
    uint8_t write_enable_cmd = W25Q_WRITE_ENABLE;
    int ret = 0;
    ret = drv_flash_W25Q128_send_data(dev, &write_enable_cmd, 1);
    if (ret != DY_EOK)
    {
        printf("drv_flash_W25Q128_WriteEnable send cmd failed\r\n");
    }
    return DY_EOK;
}

/**
 * @brief 等待flash写入完毕
 * @note 读取寄存器的状态, 判断寄存器值: 第一位标志是否被置位 
 */
static int drv_flash_W25Q128_WaitForWriteEnd(dy_device_t *dev)
{
    dy_bus_t *bus = dev->bus;
    flash_W25Q128_dev_t *flash_dev = (flash_W25Q128_dev_t*)dev;
    uint8_t read_status_reg_cmd = W25Q_READ_STATUS_REG;
    uint8_t reg_value = 0;
    int ret = 0;
    uint32_t start = drv_time_get_sys_tick();
    /*选中*/
    ret = bus->ops->control(bus, DY_SPI_CTRL_CMD_CS_LOW, NULL); //拉低片选
    /*发送读寄存器指令*/
    ret = bus->ops->send(bus, (void*)&read_status_reg_cmd, 1);
    if (ret != 1)
    {
        printf("drv_flash_W25Q128_WaitForWriteEnd ret = %d\r\n", ret);
        return DY_ERROR;
    }
    /*读取寄存器内容*/
    do
    {
        ret = bus->ops->recv(bus, (void*)&reg_value, 1); //读取寄存器内容
        if (ret != 1)
        {
            /*读取失败了, 直接返回错误*/
            printf("drv_flash_W25Q128_WaitForWriteEnd recv ret = %d\r\n", ret);
            return DY_ERROR;
        }
        if ((drv_time_get_sys_tick() - start) > flash_dev->write_timeout_ms)
        {
            printf("drv_flash_W25Q128_WaitForWriteEnd timeout\r\n");
            return DY_ETMOUT;
        }
    }while(reg_value & W25Q_STATUS_REG_BUSY);
    /*片选拉高*/
    ret = bus->ops->control(bus, DY_SPI_CTRL_CMD_CS_HIGH, NULL); //拉高片选
    return DY_EOK;
}

/**
 * @brief 扇区擦除
 * @param sector_addr: 扇区地址
 */
static int drv_flash_W25Q128_SectorErase(dy_device_t *dev, uint32_t sector_addr)
{
    uint8_t sector_erase_cmd = W25Q_SECTOR_ERASE;
    uint8_t data_buf[4] = {
        sector_erase_cmd, 
        (sector_addr & 0xFF0000) >> 16,
        (sector_addr & 0xFF00) >> 8,
        (sector_addr & 0xFF),
    };
    int ret = 0;
    /*写使能*/
    ret = drv_flash_W25Q128_WriteEnable(dev);
    if (ret != DY_EOK)
    {
        printf("write enable failed\r\n");
        return DY_ERROR;
    }

    /*发送擦除命令*/
    ret = drv_flash_W25Q128_send_data(dev, data_buf, sizeof(data_buf));
    if (ret != DY_EOK)
    {
        printf("drv_flash_W25Q128_SectorErase send cmd failed\r\n");
        return DY_ERROR;
    }

    /*等待擦除完毕*/
    ret = drv_flash_W25Q128_WaitForWriteEnd(dev);
    if (ret != DY_EOK)
    {
        printf("drv_flash_W25Q128_SectorErase wait end failed\r\n");
        return DY_ERROR;
    }
    return DY_EOK;
}

/**
 * @brief 整页写入
 * @param
 */
static int drv_flash_W25Q128_PageWrite(dy_device_t *dev, uint32_t write_addr, void *buf, unsigned int len)
{
    int ret = 0;
    dy_bus_t *bus = dev->bus;
    uint8_t *data = (uint8_t *)buf;
    /*向页地址中写入数据*/
    uint8_t page_write_cmd = W25Q_PAGE_PROGRAM;
    uint8_t data_buf[4] = {
        page_write_cmd, 
        (write_addr & 0xFF0000) >> 16,
        (write_addr & 0xFF00) >> 8,
        (write_addr & 0xFF),
    };

    /*判断写入的数据是否大于页大小*/
    if (len > W25Q_PAGE_SIZE)
    {
        printf("drv_flash_W25Q128_PageWrite len beyond W25Q_PAGE_SIZE\r\n");
        return DY_EINVAL;
    }

    /*写使能*/
    ret = drv_flash_W25Q128_WriteEnable(dev);
    if (ret != DY_EOK)
    {
        printf("write enable failed\r\n");
        return DY_ERROR;
    }

    /*发送页写入命令*/
    ret = bus->ops->control(bus, DY_SPI_CTRL_CMD_CS_LOW, NULL); //拉低片选
    ret = bus->ops->send(bus, (void*)data_buf, 4);
    if (ret != 4)
    {
        printf("drv_flash_W25Q128_PageWrite send write cmd failed\r\n");
        return DY_ERROR;
    }
    /*发送写入数据*/
     ret = bus->ops->send(bus, (void*)data, len);
    if (ret != len)
    {
        printf("drv_flash_W25Q128_PageWrite send write data failed\r\n");
        return DY_ERROR;
    }
    /*片选拉高*/
    ret = bus->ops->control(bus, DY_SPI_CTRL_CMD_CS_HIGH, NULL);

    /*等待写入完毕*/
    ret = drv_flash_W25Q128_WaitForWriteEnd(dev);
    if (ret != DY_EOK)
    {
        printf("drv_flash_W25Q128_PageWrite wait end failed\r\n");
        return DY_ERROR;
    }
    return len;
}

/**
 * @brief 单个扇区写入
 * @param
 */
static int drv_flash_W25Q128_single_sector_write(dy_device_t *dev, uint32_t sector_addr, uint32_t offset, 
    void *buf, uint32_t len)
{
    int ret = 0;
    uint8_t *data = (uint8_t*)buf;
    uint8_t sector_buffer[W25Q_SECTOR_SIZE];

    /*读取一页数据*/
    ret = drv_flash_W25Q128_read_with_addr(dev, sector_addr, sector_buffer, sizeof(sector_buffer));
    if (ret != sizeof(sector_buffer))
    {
        printf("drv_flash_W25Q128_single_sector_write read sector failed\r\n");
        return DY_ERROR;
    }

    /*把数据复制过来*/
    memcpy(&sector_buffer[offset], data, len);

    /*执行擦除*/
    ret = drv_flash_W25Q128_SectorErase(dev, sector_addr);
    if (ret != DY_EOK)
    {
        printf("drv_flash_W25Q128_single_sector_write sector erase failed\r\n");
        return DY_ERROR;
    }

    /*开始按页写入*/
    uint32_t bytes_written = 0;
    while (bytes_written < W25Q_SECTOR_SIZE)
    {
        uint32_t page_addr = sector_addr + bytes_written;
        ret = drv_flash_W25Q128_PageWrite(dev, page_addr, &sector_buffer[bytes_written], W25Q_PAGE_SIZE);
        if (ret != W25Q_PAGE_SIZE)
        {
            printf("drv_flash_W25Q128_single_sector_write page write failed\r\n");
            return DY_ERROR;
        }
        bytes_written += W25Q_PAGE_SIZE;
    }

    return DY_EOK;
}

/**
 * @brief 写入数据
 * @param
 */
int drv_flash_W25Q128_write(dy_device_t *dev, void *buf, unsigned int len)
{
    int ret = 0;
	/*获取写入的地址*/
    flash_W25Q128_dev_t *flash_dev = (flash_W25Q128_dev_t*)dev;
    uint32_t write_addr = flash_dev->cfg.mem_addr;
    
    /******************************************************************************
     * 依照 读->修改->写的思想
     * 首先读取一个扇区的数据, 判断是否需要擦除(当新数据试图把bit从0写成1的时候, 才必须擦除)
     * 如果不需要擦除, 则直接修改页中的数据, 然后循环执行页写入 - 暂时不考虑
     * 如果需要擦除, 直接把扇区中的内容修改, 然后执行擦除扇区操作, 最后循环执行页写入
     *******************************************************************************/
    uint8_t *data = (uint8_t*)buf;
    uint32_t bytes_written = 0;     //已经写入的字节

    /*基础参数校验*/
    if (write_addr >= 0x1000000)
    {
        return DY_EINVAL;
    }
    if (write_addr + len >= 0x1000000) 
    {
        return DY_EINVAL; //地址超过16MB的范围
    }
    if (len == 0)
    {
        return 0;
    }

    printf("=====================begin addr = 0x%x=====================\r\n", write_addr);
    /*循环处理*/
    while (bytes_written < len)
    {
        uint32_t cur_sector_addr = (write_addr + bytes_written) & 0xFFFFF000;//16*256: 当前处理的扇区起始地址
        uint32_t offset_in_sector = (write_addr + bytes_written) - cur_sector_addr; //写入地址在当前扇区的偏移量
        uint32_t remaining_in_sector = W25Q_SECTOR_SIZE - offset_in_sector; //扇区剩余可写空间
        uint32_t bytes_to_write_this_sector = len - bytes_written;

        if (bytes_to_write_this_sector > remaining_in_sector)
        {
            bytes_to_write_this_sector = remaining_in_sector;
        }

        printf("cur_sector_addr = 0x%x, offset_in_sector = %d, bytes_to_write_this_sector = %d\r\n",
            cur_sector_addr, offset_in_sector, bytes_to_write_this_sector);
        /*向扇区写入数据*/
        ret = drv_flash_W25Q128_single_sector_write(dev, cur_sector_addr, offset_in_sector, 
            &data[bytes_written], bytes_to_write_this_sector);
        if (ret != DY_EOK)
        {
            printf("drv_flash_W25Q128_write write single sector failed\r\n");
            return ret;  //没写入成功, 退出
        }
        bytes_written += bytes_to_write_this_sector; //更新已经写入的字节
    }

    return len;
}

/********************************************************************
 * 
 *                          一些控制指令
 * 
 ********************************************************************/

/**
 * @brief 芯片擦除: 擦除16MB
 * @param sector_addr: 扇区地址
 */
static int drv_flash_W25Q128_ChipErase(dy_device_t *dev)
{
    uint8_t chip_erase_cmd = W25Q_CHIP_ERASE;
    int ret = 0;
    /*写使能*/
    ret = drv_flash_W25Q128_WriteEnable(dev);
    if (ret != DY_EOK)
    {
        printf("write enable failed\r\n");
        return DY_ERROR;
    }

    /*发送命令*/
    ret = drv_flash_W25Q128_send_data(dev, &chip_erase_cmd, 1);
    if (ret != DY_EOK)
    {
        printf("drv_flash_W25Q128_SectorErase send cmd failed\r\n");
        return DY_ERROR;
    }

    /*等待擦除完毕*/
    ret = drv_flash_W25Q128_WaitForWriteEnd(dev);
    if (ret != DY_EOK)
    {
        printf("drv_flash_W25Q128_SectorErase sector erase failed\r\n");
        return DY_ERROR;
    }
    return DY_EOK;
}

/**
 * @brief 进入掉电模式
 */
static int drv_flash_W25Q128_PowerDown(dy_device_t *dev)
{
    uint8_t power_down_cmd = W25Q_POWER_DOWN;
    int ret = 0;
    /*发送命令*/
    ret = drv_flash_W25Q128_send_data(dev, &power_down_cmd, 1);
    if (ret != DY_EOK)
    {
        printf("drv_flash_W25Q128_SectorErase send cmd failed\r\n");
        return DY_ERROR;
    }
    return DY_EOK;
}

/**
 * @brief 唤醒芯片
 */
static int drv_flash_W25Q128_WakeUp(dy_device_t *dev)
{
    uint8_t wakeup_cmd = W25Q_RELEASE_POWER_DOWN;
    int ret = 0;
    /*发送命令*/
    ret = drv_flash_W25Q128_send_data(dev, &wakeup_cmd, 1);
    if (ret != DY_EOK)
    {
        printf("drv_flash_W25Q128_SectorErase send cmd failed\r\n");
        return DY_ERROR;
    }
    return DY_EOK;
}


/**
 * @brief 控制接口
 */
int drv_flash_W25Q128_control(dy_device_t *dev, int cmd, void *arg)
{
    int ret = DY_EOK;
    flash_W25Q128_dev_t *flash_dev = (flash_W25Q128_dev_t*)dev;
    switch(cmd)
    {
        case W25Q128_SET_MEM_ADDR:
            flash_dev->cfg.mem_addr = *((uint32_t*)arg);
            break;
        case W25Q128_CHIP_ERASE:
            ret = drv_flash_W25Q128_ChipErase(dev);
            break;
        case W25Q128_WRITE_ENABLE:
            ret = drv_flash_W25Q128_WriteEnable(dev);
            break;
        case W25Q128_POWER_DOWN:
            ret = drv_flash_W25Q128_PowerDown(dev);
            break;
        case W25Q128_WAKE_UP:
            ret = drv_flash_W25Q128_WakeUp(dev);
            break;
        default:
            ret = dy_device_control(dev, cmd, arg);
            break;
    }
    return ret;
}


device_ops_t g_flash_W25Q128_ops = {
    .init = drv_flash_W25Q128_init,
    .open = NULL,   //可以把open变成拉高
    .close = NULL,
    .write = drv_flash_W25Q128_write,
    .read = drv_flash_W25Q128_read,
    .control = drv_flash_W25Q128_control
};

int drv_spi_flash_init()
{
    /************************************
     * W25Q128: 128Mb = 16MB(128Mb比特位, 16MB字节)
     *  一页256字节
     *  一个扇区16个页, 4KB: Sector
     *  一个块16个扇区, 64KB: Block
     *  支持Quad SPI模式: 将数据线从1根变成4根(双向数据线, 输入输出复用)
     */
    int ret = DY_EOK;
    /*设置名称*/
    strcpy(g_flash_W25Q128_dev.device.name, "flash_16MB");
    /*设置驱动*/
    g_flash_W25Q128_dev.device.ops = &g_flash_W25Q128_ops;

    //g_flash_W25Q128_dev.cfg.mem_capacity = 16 * 4 * 1024;   // 一个块64KB, 16个扇区; 一个扇区4KB, 16个页; 一页256字节
    g_flash_W25Q128_dev.write_timeout_ms = 5000;
    g_flash_W25Q128_dev.max_retry_count = 1;
    /*注册设备*/
    if (DY_EOK != dy_device_register("flash_16MB", &g_flash_W25Q128_dev.device))
    {
        return DY_ERROR;
    }

    /*设备挂载到总线上*/
    dy_bus_t *bus = dy_bus_find("SPI1");
    if (NULL == bus)
    {
        return DY_ERROR;
    }
    dy_bus_attach_device(bus, &(g_flash_W25Q128_dev.device));

    /*设备初始化*/
    ret = g_flash_W25Q128_dev.device.ops->init(&g_flash_W25Q128_dev.device);
    return ret;
}


