
#include <string.h>

#include "gd32f4xx.h"
#include "drv_usart.h"

#include "FreeRTOS.h"
#include "queue.h"

#include "serial.h"

/*
首先我需要一个发送队列和一个接收队列
*/
/*

*/
#define serINVALID_QUEUE                ((QueueHandle_t)0)          //无效队列
#define serNO_BLOCK						( ( TickType_t ) 0 )        //非阻塞发送
#define serTX_BLOCK_TIME				( 40 / portTICK_PERIOD_MS ) //阻塞超时时间

/*串口硬件信息*/
#define DEBUG_USART                     USART0
#define DEBUG_IRQ                       USART0_IRQn

#define DEBUG_USART_GPIO                GPIOA
#define DEBUG_GPIO_TX_PIN               GPIO_PIN_9
#define DEBUG_GPIO_RX_PIN               GPIO_PIN_10

#define DEBUG_BUARATE                   115200U
#define DEBUG_RECV_BUFFER_SIZE          64


static void debug_parse_cmd(uint8_t *cmd, uint16_t len);

static QueueHandle_t xQueueTx = NULL;
static QueueHandle_t xQueueRx = NULL;

int fputc(int ch, FILE *file)
{
    serial_put_char((uint8_t)ch, serNO_BLOCK);
    return ch;
}

void serial_init()
{
    USART_InitTypeDef xUsartInit;
    xQueueTx = xQueueCreate(1024, sizeof(uint8_t));
    xQueueRx = xQueueCreate(64, sizeof(uint8_t));

    if (xQueueTx != NULL && xQueueRx != NULL)
    {
        xUsartInit.usart_handle = DEBUG_USART;
        xUsartInit.usart_irq = DEBUG_IRQ;
        xUsartInit.usart_tx_gpio = DEBUG_USART_GPIO;
        xUsartInit.usart_rx_gpio = DEBUG_USART_GPIO;
        xUsartInit.usart_baudrate = DEBUG_BUARATE;
        xUsartInit.usart_word_length = USART_WL_8BIT;
        xUsartInit.usart_stop_bits = USART_STB_1BIT;
        xUsartInit.usart_parity = USART_PM_NONE;
        xUsartInit.usart_interrupt = USART_INT_RBNE;
        drv_usart_init(&xUsartInit);
    }
    else
    {
        if (xQueueTx != NULL) {vQueueDelete(xQueueTx);}
        if (xQueueRx != NULL) {vQueueDelete(xQueueRx);}
    }
}

void serial_usart_interrupt_handle()
{
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
    signed char cChar;
    /*首先检查发送中断*/
    if (RESET != usart_interrupt_flag_get(DEBUG_USART, USART_INT_FLAG_TBE))
    {
        /*发送中断触发了, 发送寄存器为空, 从队列中取出一个字符*/
        if (pdPASS == xQueueReceiveFromISR(xQueueTx, &cChar, &xHigherPriorityTaskWoken))
        {
            /*发送出去, 向发送寄存器赋值, 会清空中断标志*/
            usart_data_transmit(DEBUG_USART, cChar);
            /*发送完成后, 中断标志会再次置位, 然后会再一次触发中断*/
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
        cChar = usart_data_receive(DEBUG_USART);
        
        if (pdPASS == xQueueSendFromISR(xQueueRx, &cChar, &xHigherPriorityTaskWoken))
        {

        }
        else
        {
            /*队列满了, 丢弃接收到的字符*/
            cChar = 0;
        }
    }
    /*如果更高优先级的任务被唤醒了, 则进行任务切换: 中断服务是不在内核的任务调用中, 它会打断正在执行的任务*/
    portEND_SWITCHING_ISR( xHigherPriorityTaskWoken );
}

void serial_recv_cmd_deal()
{
    static unsigned portBASE_TYPE cIndex = 0;
    static char sCmdBuffer[32] = {0};
    signed char cChar;
    portBASE_TYPE cDealFlag = pdFALSE;
    /*从队列中获取字符, 进行处理, 先进先出*/
    if (pdPASS == xQueueReceive(xQueueRx, &cChar, serNO_BLOCK))
    {
        switch(cChar)
        {
            case '\b':  //Backspace, 删掉一个字符
                serial_put_char(DEBUG_USART, cChar);
                serial_put_char(DEBUG_USART, ' ');
                serial_put_char(DEBUG_USART, cChar);
                break;
            case '\r':
            case '\n': 
                if (cIndex > 0)
                {
                    cChar = '\0';
                    cDealFlag = 1;
                    sCmdBuffer[cIndex++] = cChar;
                }
                else
                {
                    cDealFlag = 1;
                }
                break;
            default:
                usart_send_data(DEBUG_USART, cChar);
                sCmdBuffer[cIndex++] = cChar;
                break;
        }
        if (cDealFlag)
        {
            /*处理命令*/
            if (cIndex >= sizeof(sCmdBuffer))
            {
                printf("\r\ninvalid command: too long");
            }
            else if (cIndex > 0)
            {
                debug_parse_cmd(sCmdBuffer, cIndex);
            }
            
            cIndex = 0;
            cDealFlag = 0;
            printf("\r\nroot>");
        }
    }
}

signed portBASE_TYPE serial_put_char(uint8_t cOutChar, TickType_t xBlockTime)
{
    signed portBASE_TYPE xReturn;
	if( xQueueSend(xQueueTx, &cOutChar, xBlockTime ) == pdPASS )
	{
		xReturn = pdPASS;
        /*使能发送中断, 如果发送寄存器是空的, 则会自动触发中断*/
        usart_interrupt_enable(DEBUG_USART, USART_INT_TBE);
	}
	else
	{
		xReturn = pdFAIL;
	}

	return xReturn;
}

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





