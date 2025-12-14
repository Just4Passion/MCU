
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"

#include "drv_usart.h"
#include "drv_serial.h"


/******************************************************
 * 
 *                      类型
 * 
 *******************************************************/


/******************************************************
 * 
 *                     全局变量
 * 
 *******************************************************/
dy_device_t g_serial_dev;

/******************************************************
 * 
 *        对于点对点的串口, 设备和USART是绑定的
 * 
 *******************************************************/
static int serial_init(dy_device_t *dev)
{
    /*获取总线*/
    dy_usart_bus_t *usart_bus = (dy_usart_bus_t *)dev->bus;
    /*检查总线是否初始化*/
    if (NULL == usart_bus)
    {
        return DY_EINVAL;
    }
    /*检查总线状态是否已经初始化*/
    if (DY_BUS_STATE_UNINITIALIZED == usart_bus->bus.state)
    {
        return DY_EBUSNOINIT;   // 总线未初始化
    }

    return DY_EOK;
}

static int serial_write(dy_device_t *dev, void *buf, unsigned int len)
{
    dy_usart_bus_t *usart_bus = (dy_usart_bus_t *)dev->bus;
    return usart_bus->bus.ops->send(usart_bus, buf, len);
}

static int serial_read(dy_device_t *dev, void *buf, unsigned int len)
{
    dy_usart_bus_t *usart_bus = (dy_usart_bus_t *)dev->bus;
    return usart_bus->bus.ops->recv(usart_bus, buf, len);
}

static int serial_control(dy_device_t *dev, int cmd, void *arg)
{
    dy_usart_bus_t *usart_bus = (dy_usart_bus_t *)dev->bus;
    switch (cmd)
    {
        case SERIAL_SET_BAUD_RATE:
            usart_bus->ops.usart_bus_control(usart_bus, USART_SET_BAUD_RATE, arg);
            break;
		default:
            dy_device_control(dev, cmd, arg);
			break;
    }
    return DY_EOK;
}

device_ops_t serial_ops = {
    .init = serial_init,
    .open = NULL,
    .close = NULL,
    .write = serial_write,
    .read = serial_read,
    .control = serial_control,    // 切换波特率
    .callback = NULL,
};

int drv_serial_init()
{
    int ret = DY_EOK;
    int flag = DY_DEVICE_FLAG_DEACTIVATE;
    strcpy(g_serial_dev.name, "serial");
    g_serial_dev.ops = &serial_ops;

    /*注册设备*/
    if (DY_EOK != dy_device_register("serial", &g_serial_dev))
    {
        return DY_ERROR;
    }

    /*设备挂载到总线上*/
    dy_bus_t *bus = dy_bus_find("USART1");
    if (NULL == bus)
    {
        return DY_ERROR;
    }
    dy_bus_attach_device(bus, &g_serial_dev);

    /*进行初始化*/
    ret = g_serial_dev.ops->init(&g_serial_dev);
    if (DY_EOK == ret)
    {
        flag |= DY_DEVICE_FLAG_ACTIVATED;
        g_serial_dev.ops->control(&g_serial_dev, DY_DEVICE_CTRL_CMD_SET_FLAG, &flag);
    }

    return ret;
}

/*有些地方用到了printf, 所以serial的初始化要放在前面*/
int fputc(int ch, FILE *f)
{
    int flag = DY_DEVICE_FLAG_DEACTIVATE;
    /*获取serial, 检查是否初始化成功*/
    dy_device_t *serial = dy_find_device("serial");
    if (NULL == serial)
    {
        return ch;
    }
    /*获取设备标志*/
    g_serial_dev.ops->control(&g_serial_dev, DY_DEVICE_CTRL_CMD_GET_FLAG, &flag);
    if (flag & DY_DEVICE_FLAG_ACTIVATED)
    {
        /*初始化成功, 发送字符*/
        serial->ops->write(serial, &ch, 1); // 发送一个字符
    }
    return ch;
}




