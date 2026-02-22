
#include <string.h>
#include <stdio.h>

#include "gd32f4xx.h"
#include "drv_usart.h"


/*rt-thread相关*/
#include "rtdef.h"				//rt_mq_t声明
#include "rtthread.h"			//rq_mq_create声明

#include "serial.h"



/*串口硬件信息*/
#define DEBUG_USART                     USART0
#define DEBUG_IRQ                       USART0_IRQn

#define DEBUG_USART_GPIO                GPIOA
#define DEBUG_GPIO_TX_PIN               GPIO_PIN_9
#define DEBUG_GPIO_RX_PIN               GPIO_PIN_10

#define DEBUG_BUARATE                   115200U
#define DEBUG_RECV_BUFFER_SIZE          64


/*
首先我需要一个发送队列和一个接收队列
*/
#define SERIAL_TX_MQ_SIZE 1024
#define SERIAL_RX_MQ_SIZE 128


static rt_mq_t serial_mq_tx;
static rt_mq_t serial_mq_rx;



void serial_init()
{
    USART_InitTypeDef usart;
    /*创建收发消息队列*/
    serial_mq_tx = rt_mq_create("serial_tx", sizeof(uint8_t), SERIAL_TX_MQ_SIZE, RT_IPC_FLAG_FIFO);
    serial_mq_rx = rt_mq_create("serial_rx", sizeof(uint8_t), SERIAL_RX_MQ_SIZE, RT_IPC_FLAG_FIFO);

    if (serial_mq_tx != RT_NULL || serial_mq_rx != RT_NULL)
    {
        usart.usart_handle = DEBUG_USART;
        usart.usart_irq = DEBUG_IRQ;
        usart.usart_tx_gpio = DEBUG_USART_GPIO;
        usart.usart_rx_gpio = DEBUG_USART_GPIO;
        usart.usart_baudrate = DEBUG_BUARATE;
        usart.usart_word_length = USART_WL_8BIT;
        usart.usart_stop_bits = USART_STB_1BIT;
        usart.usart_parity = USART_PM_NONE;
        usart.usart_interrupt = USART_INT_RBNE;
        drv_usart_init(&usart);
    }
    else
    {
        if (serial_mq_tx != RT_NULL) {rt_mq_delete(serial_mq_tx);}
        if (serial_mq_rx != RT_NULL) {rt_mq_delete(serial_mq_rx);}
    }
}

 
void serial_usart_interrupt_handle()
{
    signed char cchar;
    /*首先检查发送中断*/
    if (RESET != usart_interrupt_flag_get(DEBUG_USART, USART_INT_FLAG_TBE))
    {
        /******************************
        rt_mq_recv();               //阻塞后, 不响应中断
        rt_mq_recv_interruptible(); //阻塞后, 可响应中断, 可被中断唤醒
        *******************************/
        /*发送中断触发了, 发送寄存器为空, 从队列中取出一个字符*/
        if (sizeof(uint8_t) == rt_mq_recv(serial_mq_tx, &cchar, sizeof(uint8_t), 0))
        {
            usart_data_transmit(DEBUG_USART, cchar);
        }
        else
        {
            /*队列为空了, 禁用发送中断, 等待队列非空的时候, 启用发送中断*/
            usart_interrupt_disable(DEBUG_USART, USART_INT_TBE);
        }
    }
    /*检查接收中断*/
    if (RESET != usart_interrupt_flag_get(DEBUG_USART, USART_INT_FLAG_RBNE))
    {
        /*接收一个字符: 读取接收寄存器会清空中断标志*/
        cchar = usart_data_receive(DEBUG_USART);

        if (RT_EOK == rt_mq_send(serial_mq_rx, &cchar, sizeof(uint8_t)))
        {

        }
        else
        {
            /*队列满了, 丢弃接收到的字符*/
            cchar = 0;
        }   
    }
}



static void debug_parse_cmd(uint8_t *cmd, uint16_t len)
{
    if(strcmp(cmd, "help") == 0)
    {
        printf("\r\nAvailable commands:");
        printf("\r\n  help - Show this help");
        printf("\r\n  reboot - Reboot");
        printf("\r\n");
    }
    else if(strcmp(cmd, "reboot") == 0)
    {
        printf("\r\nrebooting...\r\n" );
        NVIC_SystemReset();
    }
    else 
    {
        printf("\r\nUnknown command: %s\r\n", cmd);
    }
}

int32_t serial_put_char(uint8_t cchar, uint32_t timeout)
{
    int32_t ret = 0;
    if (RT_EOK == rt_mq_send_wait(serial_mq_tx, &cchar, sizeof(uint8_t), timeout))
    {
        usart_interrupt_enable(DEBUG_USART, USART_INT_TBE);
        ret = 0;
    }
    else
    {
        ret = -1;
    }

	return ret;
}

void serial_recv_cmd_deal()
{
    static unsigned int cindex = 0;
    static char cmd_buffer[32] = {0};
    signed char cchar;
    unsigned int deal_flag = 0;
    /*从队列中获取字符, 进行处理, 先进先出*/
    if (sizeof(char) == rt_mq_recv(serial_mq_rx, &cchar, sizeof(char), 0))
    {
        switch(cchar)
        {
            case '\b':  //Backspace, 删掉一个字符
                serial_put_char(DEBUG_USART, cchar);
                serial_put_char(DEBUG_USART, ' ');
                serial_put_char(DEBUG_USART, cchar);
                break;
            case '\r':
            case '\n': 
                if (cindex > 0)
                {
                    cchar = '\0';
                    deal_flag = 1;
                    cmd_buffer[cindex++] = cchar;
                }
                else
                {
                    deal_flag = 1;
                }
                break;
            default:
                usart_send_data(DEBUG_USART, cchar);
                cmd_buffer[cindex++] = cchar;
                break;
        }
        if (deal_flag)
        {
            /*处理命令*/
            if (cindex >= sizeof(cmd_buffer))
            {
                printf("\r\ninvalid command: too long");
            }
            else if (cindex > 0)
            {
                debug_parse_cmd(cmd_buffer, cindex);
            }
            
            cindex = 0;
            deal_flag = 0;
            printf("\r\nroot>");
        }
    }
}

#if 0
void serial_put_string(const char * const pcString, unsigned short usStringLength )
{
    signed char *pxNext;
	pxNext = ( signed char * ) pcString;
	while( *pxNext )
	{
		serial_put_char(*pxNext, serNO_BLOCK);
		pxNext++;
	}
}

signed portBASE_TYPE serial_get_char(char *cChar, TickType_t xBlockTime)
{
    signed portBASE_TYPE xReturn;
    if( xQueueReceive(xQueueRx, cChar, xBlockTime) == pdPASS )
    {
        xReturn = pdPASS;
    }
    else
    {
        xReturn = pdFAIL;
    }
    return xReturn;
}
#endif




