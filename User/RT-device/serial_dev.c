

#include "serial_dev.h"

rt_inline int _serial_poll_rx(struct rt_serial_device *serial, rt_uint8_t *data, int length)
{
    int ch;
    int size;
    /*一次一次调用getc(), 直到获取到length个字节或者没有数据可读取*/
    size = length;
    while(length)
    {
        ch = serial->ops->getc(serial);
        if (ch == -1) {break;}
        *data++ = ch;
        length--;
        if (ch == '\n') {break;}
    }
    return (size - length);
}

rt_inline int _serial_poll_tx(struct rt_serial_device *serial, const rt_uint8_t *data, int length)
{
    int ch;
    int size;

    size = length;
    while(length)
    {
        if (*data == '\n' && (serial->parent.open_flag & RT_DEVICE_FLAG_STREAM))
        {
            serial->ops->putc(serial, '\r');
        }
        serial->ops->putc(serial, *data++);
        --length;
    }
    return (size - length);
}

/*
DMA方式, 这样直接向buffer里面写入数据
接收也只是需要从buffer里面读取数据即可
*/
rt_inline int _serial_int_rx(struct rt_serial_device *serial, rt_uint8_t *data, int length)
{
    return 0;
}

static rt_err_t rt_serial_init(struct rt_device *dev)
{
    rt_err_t result = RT_EOK;
    struct rt_serial_device *serial;

    serial = (struct rt_serial_device *)dev;

    serial->serial_rx = RT_NULL;
    serial->serial_tx = RT_NULL;
    
    if (serial->ops->configure)
    {
        result = serial->ops->configure(serial, &serial->config);
    }
    return result;
}

static rt_err_t rt_serial_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_uint16_t stream_flag = 0;
    struct rt_serial_device *serial;

    serial = (struct rt_serial_device *)dev;

    if ((oflag & RT_DEVICE_FLAG_INT_RX) && !(dev->flag & RT_DEVICE_FLAG_INT_RX))
    {
        return -RT_EIO;
    }
    if ((oflag & RT_DEVICE_FLAG_INT_TX) && !(dev->flag & RT_DEVICE_FLAG_INT_TX))
    {
        return -RT_EIO;
    }
        
    if ((oflag & RT_DEVICE_FLAG_STREAM) || (dev->open_flag & RT_DEVICE_FLAG_STREAM))
    {
        stream_flag = RT_DEVICE_FLAG_STREAM;
    }

    dev->open_flag = oflag & 0xff;

    if (serial->serial_rx == RT_NULL)
    {
        /*如果使用中断的方式接收, 则放在接收缓冲区中*/
        if (oflag & RT_DEVICE_FLAG_INT_RX)
        {
            struct rt_serial_rx_fifo* rx_fifo;
            rx_fifo = (struct rt_serial_rx_fifo*) rt_malloc (sizeof(struct rt_serial_rx_fifo) +
                serial->config.bufsz);
            rx_fifo->buffer = (rt_uint8_t*) (rx_fifo + 1);
            rt_memset(rx_fifo->buffer, 0, serial->config.bufsz);

            rx_fifo->put_index = 0;
            rx_fifo->get_index = 0;
            rx_fifo->is_full = RT_FALSE;

            serial->serial_rx = rx_fifo;
            dev->open_flag |= RT_DEVICE_FLAG_INT_RX;

            /*配置成低级设备*/
            serial->ops->control(serial, RT_DEVICE_CTRL_SET_INT, (void *)RT_DEVICE_FLAG_INT_RX);
        }
        else
        {
            serial->serial_rx = RT_NULL;
        }
    }
    else
    {
        if (oflag & RT_DEVICE_FLAG_INT_RX)
        {
            dev->open_flag |= RT_DEVICE_FLAG_INT_RX;
        }
    }

    if (serial->serial_tx == RT_NULL)
    {
        serial->serial_tx = RT_NULL;
    }

    dev->open_flag |= stream_flag;
    return RT_EOK;
}

static rt_err_t rt_serial_close(struct rt_device *dev)
{
    /*资源要回收*/
    struct rt_serial_device *serial;
    serial = (struct rt_serial_device *)dev;

    if (dev->ref_count > 1) {return RT_EOK;}

    if (dev->open_flag & RT_DEVICE_FLAG_INT_RX)
    {
        struct rt_serial_rx_fifo* rx_fifo;

        rx_fifo = (struct rt_serial_rx_fifo*)serial->serial_rx;

        /*如果外部是全局变量, 这里的释放会不会有问题*/
        rt_free(rx_fifo);
        serial->serial_rx = RT_NULL;
        dev->open_flag &= ~RT_DEVICE_FLAG_INT_RX;
        /*清空中断标志*/
        serial->ops->control(serial, RT_DEVICE_CTRL_CLR_INT, (void*)RT_DEVICE_FLAG_INT_RX);
    }
    return RT_EOK;
}

static rt_size_t rt_serial_read(struct rt_device *dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    struct rt_serial_device *serial;

    if (size == 0) return 0;

    serial = (struct rt_serial_device *)dev;

    if (dev->open_flag & RT_DEVICE_FLAG_INT_RX)
    {
        return _serial_int_rx(serial, buffer, size);
    }

    return _serial_poll_rx(serial, buffer, size);
}

static rt_size_t rt_serial_write(struct rt_device *dev, rt_off_t pos, const void *buffer,rt_size_t size)
{
    struct rt_serial_device *serial;

    if (size == 0) return 0;
    serial = (struct rt_serial_device *)dev;

    return _serial_poll_tx(serial, buffer, size);
}

static rt_err_t rt_serial_control(struct rt_device *dev, int cmd, void *args)
{
    rt_err_t ret = RT_EOK;
    struct rt_serial_device *serial;

    serial = (struct rt_serial_device *)dev;

    switch (cmd)
    {
        case RT_DEVICE_CTRL_SUSPEND:
            dev->flag |= RT_DEVICE_FLAG_SUSPENDED;
            break;
        case RT_DEVICE_CTRL_RESUME:
            dev->flag &= ~RT_DEVICE_FLAG_SUSPENDED;
            break;
        /*控制修改串口配置*/
        case RT_DEVICE_CTRL_CONFIG:
            if (args)
            {
                struct serial_configure *pconfig = (struct serial_configure *) args;
                if (pconfig->bufsz != serial->config.bufsz && serial->parent.ref_count)
                {
                    /*can not change buffer size*/
                    return RT_EBUSY;
                }
                serial->config = *pconfig;
                /*有人引用, 说明设备启用了*/
                if (serial->parent.ref_count)
                {
                    serial->ops->configure(serial, (struct serial_configure *) args);
                }
            }
        default:
            ret = serial->ops->control(serial, cmd, args);
            break;
    }
    return ret;
}


rt_err_t rt_hw_serial_register(struct rt_serial_device *serial, const char *name, rt_uint32_t flag, void *data)
{
    rt_err_t ret;
    struct rt_device *device;

    device = &(serial->parent);
    device->type        = RT_Device_Class_Char;
    device->rx_indicate = RT_NULL;
    device->tx_complete = RT_NULL;

    device->init        = rt_serial_init;
    device->open        = rt_serial_open;
    device->close       = rt_serial_close;
    device->read        = rt_serial_read;
    device->write       = rt_serial_write;
    device->control     = rt_serial_control;
    device->user_data   = data;

    /*设备注册, 注册到哪里, device和name会起到什么作用*/
    //ret = rt_device_register(device, name, flag);
    return ret;
}

/*在中断中调用, 用来处理事件*/
void rt_hw_serial_isr(struct rt_serial_device *serial, int event)
{
    switch(event & 0xff)
    {
        case RT_SERIAL_EVENT_RX_IND:
        {
            int ch = 1;
            rt_base_t level;
            struct rt_serial_rx_fifo *rx_fifo;

            rx_fifo = (struct rt_serial_rx_fifo *)serial->serial_rx;

            while(1)
            {
                ch = serial->ops->getc(serial);
                if (ch == -1) break;

                level = rt_hw_interrupt_disable();

                rx_fifo->buffer[rx_fifo->put_index] = ch;
                rx_fifo->put_index += 1;
                if (rx_fifo->put_index >= serial->config.bufsz) rx_fifo->put_index = 0;

                if (rx_fifo->put_index == rx_fifo->get_index)
                {
                    rx_fifo->get_index += 1;
                    rx_fifo->is_full = RT_TRUE;
                    if (rx_fifo->get_index >= serial->config.bufsz) rx_fifo->get_index = 0;
                }

                rt_hw_interrupt_enable(level);
            }

            if (serial->parent.rx_indicate != RT_NULL)
            {
                rt_size_t rx_length;
                level = rt_hw_interrupt_disable();
                rx_length = (rx_fifo->put_index >= rx_fifo->get_index)? (rx_fifo->put_index - rx_fifo->get_index):
                    (serial->config.bufsz - (rx_fifo->get_index - rx_fifo->put_index));
                rt_hw_interrupt_enable(level);

                serial->parent.rx_indicate(&serial->parent, rx_length);
            }
            break;
        }
    }
}



