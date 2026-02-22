
#if 1
#include <stdio.h>
#include <string.h>
#include "gd32f4xx.h"
#include "drv_usart.h"

#include "drv_conf.h"

#define DEBUG_USART USART0
#define DEBUG_IRQ USART0_IRQn

#define DEBUG_USART_GPIO GPIOA
#define DEBUG_GPIO_TX_PIN GPIO_PIN_9
#define DEBUG_GPIO_RX_PIN GPIO_PIN_10

#define DEBUG_BUARATE 115200U
#define DEBUG_RECV_BUFFER_SIZE 64

#define DEBUG_START_NOTE        "\r\nroot>"

typedef enum
{
    DEBUG_STATE_CMD,            //处于命令行状态
    DEBUG_STATE_DOWNLOAD        //处于下载状态
}DEBUG_STATE;

static DEBUG_STATE gDebugState = DEBUG_STATE_CMD;
/*调试指令接收数据接口*/
static uint8_t recv_buffer[DEBUG_RECV_BUFFER_SIZE] = {0};
static uint32_t recv_index = 0;
static uint8_t recv_flag = 0;         //接收到数据标志

/******************************************************
                        局部函数声明
******************************************************/

void debug_init()
{
    USART_InitTypeDef usart_init;
    usart_init.usart_handle = DEBUG_USART;
    usart_init.usart_irq = DEBUG_IRQ;
    usart_init.usart_tx_gpio = DEBUG_USART_GPIO;
    usart_init.usart_rx_gpio = DEBUG_USART_GPIO;
    usart_init.usart_baudrate = DEBUG_BUARATE;
    usart_init.usart_word_length = USART_WL_8BIT;
    usart_init.usart_stop_bits = USART_STB_1BIT;
    usart_init.usart_parity = USART_PM_NONE;
    usart_init.usart_interrupt = USART_INT_RBNE;
    drv_usart_init(&usart_init);
    printf(DEBUG_START_NOTE);
}

static void debug_clear_flag()
{
    recv_index = 0;
    recv_flag = 0;
}

static uint8_t debug_cmd_flag()
{
    return recv_flag;
}

static void debug_set_state(DEBUG_STATE state)
{
    gDebugState = state;
}

static DEBUG_STATE debug_get_state()
{
    return gDebugState;
}

static uint32_t debug_get_cmd_len()
{
    return recv_index;
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

void debug_deal()
{
    /*
    检查是否有命令进来
    */
    uint32_t len = 0;
    uint8_t cmd[DEBUG_RECV_BUFFER_SIZE + 1] = {0};
    if (debug_cmd_flag())
    {
        /*处理命令*/
        len = debug_get_cmd_len();
        memcpy(cmd, recv_buffer, recv_index);
        debug_clear_flag();
        /*
        解析命令
        */
        if (len > 0)
        {
            debug_parse_cmd(cmd, DEBUG_RECV_BUFFER_SIZE);
        }
        printf(DEBUG_START_NOTE);
    }
}

static void debug_cmd_recv(uint8_t data)
{
    if (recv_index < DEBUG_RECV_BUFFER_SIZE)
    {
        switch(data)
        {
            case '\b':  //Backspace, 删掉一个字符
                if (recv_index > 0)
                {
                    recv_index--;
                }
                usart_send_data(DEBUG_USART, data);
                usart_send_data(DEBUG_USART, ' ');
                usart_send_data(DEBUG_USART, data);
                break;
            case '\r':
            case '\n': 
                if (recv_index > 0)
                {
                    data = '\0';
                    recv_flag = 1;
                    recv_buffer[recv_index++] = data;
                }
                else
                {
                    recv_flag = 1;
                }
                break;
            default:
                usart_send_data(DEBUG_USART, data);
                recv_buffer[recv_index++] = data;
                break;
        }
    }
    else
    {
        //如果缓冲区满了, 丢弃所有字节, 重新开始
        recv_index = 0;
        recv_flag = 0;
    }
}

/*
1、接收数据
2、接收到了一行数据, 这行数据可以被处理了, 设置一个可处理标志
3、下一行数据来了，覆盖这一行数据

1、获取可处理标志
2、把数据获取过来，进行处理
*/

/*
中断接收
*/
void debug_entry_USART0_IRQHandler(void)
{
    /*
    接收数据，并把数据放在缓冲区中
    */
    uint8_t data = 0;
    DEBUG_STATE state = DEBUG_STATE_CMD;
    state = debug_get_state();
    if (SET == usart_flag_get(DEBUG_USART, USART_FLAG_RBNE))
    {
        /*
        有数据可以读取了
        */
        usart_recv_data(DEBUG_USART, &data);
        /*
        如果收到是回车或换行, 说明这一行数据接收完了, 可以处理了
        如果是Backspace则删除一个字符
        回显输入的字符
        */
        state = debug_get_state();
        switch(state)
        {
            case DEBUG_STATE_CMD:
                debug_cmd_recv(data);
                break;
            default:
                break;
        }
        
    }
}

/*
重定向
*/
int fputc_debug_entry(int ch, FILE *file)
{
    usart_send_data(DEBUG_USART, (uint8_t)ch);
    return ch;
}


#endif
