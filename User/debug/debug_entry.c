
#if 1
#include <stdio.h>
#include <string.h>
#include "gd32f30x.h"
#include "drv_usart.h"
#include "system_mng.h"

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
    drv_usart0_init();
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
        printf("\r\n  pendsv - PendSV_Handler Trigger");
        printf("\r\n  svc - SVC_Handler Trigger");
        printf("\r\n");
    }
    else if(strcmp(cmd, "reboot") == 0)
    {
        printf("\r\nrebooting...\r\n" );
        NVIC_SystemReset();
    }
    else if (strcmp(cmd, "pendsv") == 0)
    {
        pend_sv_trigger();
    }
    else if (strcmp(cmd, "svc") == 0)
    {
        svc_trigger();
    }
    else if (memcmp(cmd, "svc:", 4) == 0)
    {
        typedef struct
        {
            int value1;
            int value2;
        }ret_type;

        typedef struct
        {
            int value1;
            int value2;
            int value3;
            int value4;
            int value5;
            int value6;
        }big_ret_type;
        __svc(0x01) int svc_service_add(int x, int y);
        __svc(0x02) int svc_service_sub(int x, int y);
        __svc(0x03) int svc_service_incr(int x);
        __svc(0x04)  __value_in_regs ret_type svc_service_pair(int x, int y);
        __svc(0x05) big_ret_type* svc_service_big();
        int svc_number = atoi(cmd + 4);
        int result = 0;
        switch(svc_number)
        {
            case 1:
                result = svc_service_add(4, 3);
                break;
            case 2:
                result = svc_service_sub(4, 3);
                break;
            case 3:
                result = svc_service_incr(4);
                break;
            default:
                break;
        }
        printf("SVC %d result = %d\r\n", svc_number, result);
    }
    else if (memcmp(cmd, "timer7:", 7) == 0)
    {
        /*在0到10的范围已经很亮了: 999*/
        int duty = atoi(cmd + 7);
        printf("duty = %d\r\n", duty);
        timer_channel_output_pulse_value_config(TIMER7, TIMER_CH_1, duty);
    }
    else if (memcmp(cmd, "led:", 4) == 0)
    {
        int led_num = atoi(cmd + 4);
        printf("led_num = %d\r\n", led_num);
        switch(led_num)
        {
            case 10: gpio_bit_reset(GPIOA, GPIO_PIN_6); break;  //绿灯灭
            case 11: gpio_bit_set(GPIOA, GPIO_PIN_6); break;    //绿灯亮
            case 20: gpio_bit_reset(GPIOA, GPIO_PIN_7); break;  //蓝灯灭
            case 21: gpio_bit_set(GPIOA, GPIO_PIN_7); break;    //蓝灯亮
            case 30: timer_pwm_black_led_off(); break;          //背光灭
            case 31: timer_pwm_black_led_on(); break;           //背光亮
            case 40: timer_pwm_breath_led_off(); break;         //呼吸灯灭
            case 41: timer_pwm_breath_led_on(); break;          //呼吸灯亮
            default:
                break;
        }
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
int fputc(int ch, FILE *file)
{
    usart_send_data(DEBUG_USART, (uint8_t)ch);
    return ch;
}


#endif
