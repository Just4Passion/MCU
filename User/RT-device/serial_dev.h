
#ifndef __SERIAL_DEV_H
#define __SERIAL_DEV_H

#include "rtdef.h"
#include "device.h"

#define BAUD_RATE_2400                  2400
#define BAUD_RATE_4800                  4800
#define BAUD_RATE_9600                  9600
#define BAUD_RATE_19200                 19200
#define BAUD_RATE_38400                 38400
#define BAUD_RATE_57600                 57600
#define BAUD_RATE_115200                115200
#define BAUD_RATE_230400                230400
#define BAUD_RATE_460800                460800
#define BAUD_RATE_921600                921600
#define BAUD_RATE_2000000               2000000
#define BAUD_RATE_3000000               3000000

#define RT_SERIAL_EVENT_RX_IND          0x01    /* Rx indication */
#define RT_SERIAL_EVENT_TX_DONE         0x02    /* Tx complete   */
#define RT_SERIAL_EVENT_RX_DMADONE      0x03    /* Rx DMA transfer done */
#define RT_SERIAL_EVENT_TX_DMADONE      0x04    /* Tx DMA transfer done */
#define RT_SERIAL_EVENT_RX_TIMEOUT      0x05    /* Rx timeout    */

#define DATA_BITS_5                     5
#define DATA_BITS_6                     6
#define DATA_BITS_7                     7
#define DATA_BITS_8                     8
#define DATA_BITS_9                     9

#define STOP_BITS_1                     0
#define STOP_BITS_2                     1
#define STOP_BITS_3                     2
#define STOP_BITS_4                     3

#define PARITY_NONE                     0
#define PARITY_ODD                      1
#define PARITY_EVEN                     2

#define BIT_ORDER_LSB                   0
#define BIT_ORDER_MSB                   1

#define NRZ_NORMAL                      0       /* Non Return to Zero : normal mode */
#define NRZ_INVERTED                    1       /* Non Return to Zero : inverted mode */

#define RT_SERIAL_RB_BUFSZ              64

#define RT_SERIAL_CONFIG_DEFAULT           \
{                                          \
    BAUD_RATE_115200, /* 115200 bits/s */  \
    DATA_BITS_8,      /* 8 databits */     \
    STOP_BITS_1,      /* 1 stopbit */      \
    PARITY_NONE,      /* No parity  */     \
    BIT_ORDER_LSB,    /* LSB first sent */ \
    NRZ_NORMAL,       /* Normal mode */    \
    RT_SERIAL_RB_BUFSZ, /* Buffer size */  \
    0                                      \
}


struct rt_serial_device;

struct serial_configure
{
    rt_uint32_t baud_rate;              //波特率

    rt_uint32_t data_bits        :4;    //7,8,9
    rt_uint32_t stop_bits        :2;    //0.5, 1, 1.5, 2
    rt_uint32_t parity           :2;    //NONE, EVEN, ODD
    rt_uint32_t bit_order        :1;    //MSB, LSB
    rt_uint32_t invert           :1;    //
    rt_uint32_t bufsz            :16;   //接收缓冲区大小
    rt_uint32_t reserved         :4;    //保留位
};

struct rt_serial_rx_fifo
{
    rt_uint8_t *buffer;
    rt_uint16_t put_index, get_index;
    rt_bool_t is_full;
};

struct rt_uart_ops
{
    /*初始化的时候需要配置*/
    rt_err_t (*configure)(struct rt_serial_device *serial, struct serial_configure *cfg);
    /*控制操作*/
    rt_err_t (*control)(struct rt_serial_device *serial, int cmd, void *args);

    /*
    发送字符和接收字符
    接收有两种方式实现, 一种是轮询, 一种是中断
    */
    int (*putc)(struct rt_serial_device *serial, char ch);
    int (*getc)(struct rt_serial_device *serial);

    /*DMA传输: 发送和接收 - 使用DMA实现*/
    rt_size_t (*dma_transmit)(struct rt_serial_device *serial,rt_uint8_t *buffer, rt_size_t size, int direction);
};

struct rt_serial_device
{
    struct rt_device parent;

    /*对于串口设备, 特化的操作集*/
    struct rt_uart_ops *ops;

    /*串口配置: 初始化的时候需要配置的参数*/
    struct serial_configure config;

    void *serial_rx;        //这个是接收缓冲区
    void *serial_tx;
};
typedef struct rt_serial_device rt_serial_t;


void rt_hw_serial_isr(struct rt_serial_device *serial, int event);

/*作为一个串口设备, 仅仅提供了注册接口*/
rt_err_t rt_hw_serial_register(rt_serial_t *serial, const char *name, rt_uint32_t flag, void *data);


#endif
