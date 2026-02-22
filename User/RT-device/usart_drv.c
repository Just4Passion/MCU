
#include "gd32f4xx.h"
#include "drv_usart.h"

#include "rtdef.h"
#include "serial_dev.h"
#include "usart_drv.h"


#define DEBUG_USART USART0
#define DEBUG_IRQ USART0_IRQn

#define DEBUG_USART_GPIO GPIOA
#define DEBUG_GPIO_TX_PIN GPIO_PIN_9
#define DEBUG_GPIO_RX_PIN GPIO_PIN_10

#define DEBUG_BUARATE 115200U

struct gd32_uart
{
    uint32_t usart_handle;
    IRQn_Type irq;
};

static struct gd32_uart uart0 =
{
    .usart_handle = USART0,
    .irq = USART0_IRQn,
};

struct rt_serial_device serial0;

static rt_err_t gd32_configure(struct rt_serial_device *serial, struct serial_configure *cfg)
{
    USART_InitTypeDef usart_init;
    struct gd32_uart *usart;

    usart = (struct gd32_uart *)serial->parent.user_data;
    usart_init.usart_handle = usart->usart_handle;
    usart_init.usart_irq = usart->irq;
    usart_init.usart_tx_gpio = DEBUG_USART_GPIO;
    usart_init.usart_rx_gpio = DEBUG_USART_GPIO;
    usart_init.usart_baudrate = DEBUG_BUARATE;
    usart_init.usart_word_length = USART_WL_8BIT;
    usart_init.usart_stop_bits = USART_STB_1BIT;
    usart_init.usart_parity = USART_PM_NONE;
    usart_init.usart_interrupt = USART_INT_RBNE;
    drv_usart_init(&usart_init);
}

static rt_err_t gd32_control(struct rt_serial_device *serial, int cmd, void *args)
{
    struct gd32_uart *uart;

    uart = (struct gd32_uart *)serial->parent.user_data;

    switch (cmd)
    {
        case RT_DEVICE_CTRL_CLR_INT:
            /*禁用中断*/
            /*禁用中断标志*/
            break;
        case RT_DEVICE_CTRL_SET_INT:
            /*启用中断*/
            /*启用中断标志*/
            break;
    }
    return RT_EOK;
}

static int gd32_putc(struct rt_serial_device *serial, char ch)
{
    struct gd32_uart *uart;
    uart = (struct gd32_uart *)serial->parent.user_data;
    while (usart_flag_get(uart->usart_handle, USART_FLAG_TBE) == RESET);
    usart_data_transmit(uart->usart_handle, (uint8_t)ch);
    return 1;
}

static int gd32_getc(struct rt_serial_device *serial)
{
    int ch;
    struct gd32_uart *uart;
    uart = (struct gd32_uart *)serial->parent.user_data;
    if (usart_flag_get(uart->usart_handle, USART_FLAG_TBE) != RESET)
    {
        ch = usart_data_receive(uart->usart_handle);
    }
    return ch;
}

static void uart_isr(struct rt_serial_device *serial)
{
    struct gd32_uart *uart = (struct gd32_uart *) serial->parent.user_data;

    if (usart_flag_get(uart->usart_handle, USART_FLAG_RBNE) != RESET
        && usart_interrupt_flag_get(uart->usart_handle, USART_INT_FLAG_RBNE) != RESET)
    {
        /*启用了中断, 接收标志置位了, 中断标志也置位了*/
        rt_hw_serial_isr(serial, RT_SERIAL_EVENT_RX_IND);
    }
}

static const struct rt_uart_ops gd32_uart_ops =
{
    gd32_configure,
    gd32_control,
    gd32_putc,
    gd32_getc,
};

int rt_hw_usart_init()
{
    struct gd32_uart *uart;
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    uart = &uart0;
    config.baud_rate = BAUD_RATE_115200;
    serial0.ops = (struct rt_uart_ops*)&gd32_uart_ops;
    serial0.config = config;

    rt_hw_serial_register(&serial0, "uart0", RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX, uart);
    return 0;
}


