
#include <stdio.h>
#include <string.h>

#include "drv_usart.h"


/**************************************************
                        宏定义
**************************************************/
/* USART通道 */
#define DEBUG_USART USART1
/* 数据寄存器 */
#define DEBUG_USART_DATA_REG &(USART1->DR);

/* 发送相关GPIO */
#define DEBUG_USART_TX_GPIO GPIOA
#define DEBUG_USART_TX_PIN_NO 9
/* 接收相关GPIO */
#define DEBUG_USART_RX_GPIO GPIOA
#define DEBUG_USART_RX_PIN_NO 10

/* 中断通道与中断标志 */
#define DEBUG_USART_INT_CHAN USART1_IRQn
#define DEBUG_USART_INT_FLAG USART_IT_RXNE

/*
如果调试打印影响到了程序的运行结果
可以使用DMA实现调试程序打印 —— 可以通过一些标志来实现立即打印和延时打印
或者可以50ms, 500ms, 1s打印一次
暂时可以先不处理这个
*/

#define DEBUG_RECV_BUFF_SIZE 16

/**************************************************
                全局变量声明
**************************************************/
uint8_t g_recv_buff[DEBUG_RECV_BUFF_SIZE] = {0};
uint32_t g_recv_index = 0;
uint32_t g_recv_flag = 0;

DRV_DMA_MP_INIT g_stPMRcvDmaInit = {0};

/**************************************************
                局部函数声明
**************************************************/
/*调试指令接收处理函数*/
static uint8_t debug_get_recv_flag(void);
static void debug_clear_recv_flag(void);
static uint8_t *debug_get_recv_buff(void);
static void debug_recv_cmd_parse(const char *cmdstr);
/*调试指令函数*/
static void debug_reboot();
static void debug_led_flowing(uint32_t delay_s);

void debug_modle_init()
{
    DRV_USART_INIT stUSARTInit;

    stUSARTInit.usart_handle = DEBUG_USART;
    stUSARTInit.tx_gpio = DEBUG_USART_TX_GPIO;
    stUSARTInit.tx_gpio_pin = DEBUG_USART_TX_PIN_NO;
    stUSARTInit.rx_gpio = DEBUG_USART_RX_GPIO;
    stUSARTInit.rx_gpio_pin = DEBUG_USART_RX_PIN_NO;

    stUSARTInit.usart_int_enable = 1;
    stUSARTInit.usart_int_flag[0] = DEBUG_USART_INT_FLAG;
    stUSARTInit.usart_int_num = 1;

    stUSARTInit.usart_baud_rate = 115200;
    stUSARTInit.usart_word_length = USART_WordLength_8b;
    stUSARTInit.usart_hardware_flow_control = USART_HardwareFlowControl_None;
    stUSARTInit.usart_mode = USART_Mode_Rx | USART_Mode_Tx;
    stUSARTInit.usart_parity = USART_Parity_No;;
    stUSARTInit.usart_stop_bits = USART_StopBits_1;

    drv_usart_init(&stUSARTInit);
}

void debug_recv_cmd_deal()
{
    uint8_t cmd[DEBUG_RECV_BUFF_SIZE] = {0};
    if (debug_get_recv_flag())
    {
        //说明收到\r\n了可以处理命令了
        memcpy(cmd, debug_get_recv_buff(), DEBUG_RECV_BUFF_SIZE);
        debug_clear_recv_flag();
        if (strlen(cmd))
        {
            debug_recv_cmd_parse((char*)cmd);
        }
        printf("\r\n->");
    }
}

void USART1_IRQHandler()
{
    /*
    接收数据
    */
    uint8_t data = 0;
    if (USART_GetITStatus(DEBUG_USART, USART_IT_RXNE) != RESET)
    {
        //表明有数据可读了, 读取数据即可清空TC
        drv_usart_recv_byte(DEBUG_USART, &data);
        if (g_recv_index < DEBUG_RECV_BUFF_SIZE)
        {
            switch(data)
            {
                case '\b':  //删除
                    if (g_recv_index > 0)
                    {
                        g_recv_index--;
                    }
                    drv_usart_send_byte(DEBUG_USART, '\b');
                    drv_usart_send_byte(DEBUG_USART, ' ');
                    drv_usart_send_byte(DEBUG_USART, '\b');
                    break;
                case '\r':
                case '\n':
                    data = '\0';
                    g_recv_buff[g_recv_index++] = data;
                    g_recv_flag = 1;
                    break;
                default:
                    g_recv_buff[g_recv_index++] = data;
                    drv_usart_send_byte(DEBUG_USART, data);
                    break;
            }
        }
        else
        {
            g_recv_flag = 0;
            g_recv_index = 0;
        }
    }
}


int fputc(int ch, FILE *f)
{
    drv_usart_send_byte(DEBUG_USART, ch);
    return ch;
}

static uint8_t debug_get_recv_flag()
{
    return g_recv_flag;
}

static void debug_clear_recv_flag()
{
    g_recv_flag = 0;
    g_recv_index = 0;
}

static uint8_t *debug_get_recv_buff()
{
    return g_recv_buff;
}

static void debug_recv_cmd_parse(const char *cmdstr)
{
    /*
    指令的形式是 cmdstr\0, cmdstr1 cmdstr2\0, cmdstr1 parameter\0
    */
    if (strcmp("help", cmdstr) == 0)
    {
        printf("\r\nhelp, show all cmd");
        printf("\r\nreboot, reboot system");
        printf("\r\nled flowing, led flowing");
    }
    else if (strcmp("reboot", cmdstr) == 0)
    {
        printf("\r\nrebooting\r\n");
        debug_reboot();
    }
    else
    {
        printf("\r\nrnot support: %s\r\n", cmdstr);
    }
}

/**************************************************
                局部函数声明
**************************************************/
static void debug_reboot()
{
    __set_FAULTMASK(1); // 关闭所有中断
    NVIC_SystemReset(); //重启
}


#if 0       //DMA测试

/*DMA通道*/
#if 0
#define DEBUG_USART_DMA_SND_STREAM DMA2_Stream7
#define DEBUG_USART_DMA_RCV_STREAM DMA2_Stream2
#define DEBUG_USART_DMA_CHANNEL DMA_Channel_4

#define DEBUG_DMA_INT_FLAG (DMA_IT_TC | DMA_IT_HT)
#define DEBUG_DMA_INT_TC_FLAG  (DMA_IT_TCIF2)
#define DEBUG_DMA_INT_HT_FLAG  (DMA_IT_HTIF2)

#define DEBUG_USART_DMA_INT_FLAG USART_IT_IDLE
#endif


void debug_modle_dma_init()
{
    DRV_USART_DMA_INIT stUsartDmaInit = {0};
    DRV_DMA_MP_INIT stPMRcvDmaInit = {0};            //启用接收DMA
    DRV_USART_INIT stUSARTInit = {0};
    stUsartDmaInit.stUSARTInit.usart_handle = DEBUG_USART;
    stUsartDmaInit.stUSARTInit.tx_gpio = DEBUG_USART_TX_GPIO;
    stUsartDmaInit.stUSARTInit.tx_gpio_pin = DEBUG_USART_TX_PIN_NO;
    stUsartDmaInit.stUSARTInit.rx_gpio = DEBUG_USART_RX_GPIO;
    stUsartDmaInit.stUSARTInit.rx_gpio_pin = DEBUG_USART_RX_PIN_NO;

    stUsartDmaInit.stUSARTInit.usart_int_enable = 1;
    stUsartDmaInit.stUSARTInit.usart_int_flag[0] = DEBUG_USART_DMA_INT_FLAG;
    //stUsartDmaInit.stUSARTInit.usart_int_flag[1] = DEBUG_USART_INT_FLAG;
    stUsartDmaInit.stUSARTInit.usart_int_num = 1;

    stUsartDmaInit.stUSARTInit.usart_baud_rate = 115200;
    stUsartDmaInit.stUSARTInit.usart_word_length = USART_WordLength_8b;
    stUsartDmaInit.stUSARTInit.usart_hardware_flow_control = USART_HardwareFlowControl_None;
    stUsartDmaInit.stUSARTInit.usart_mode = USART_Mode_Rx | USART_Mode_Tx;
    stUsartDmaInit.stUSARTInit.usart_parity = USART_Parity_No;;
    stUsartDmaInit.stUSARTInit.usart_stop_bits = USART_StopBits_1;

    #if 1
    stUsartDmaInit.stPMInit.dma_stream_handle = DEBUG_USART_DMA_RCV_STREAM;        //接收数据流
    stUsartDmaInit.stPMInit.dma_channel_handle = DEBUG_USART_DMA_CHANNEL;
    stUsartDmaInit.stPMInit.dma_m2p_p2m = DRV_DMA_DIR_P2M;
    stUsartDmaInit.stPMInit.dma_periph_reg = DEBUG_USART_DATA_REG;
    stUsartDmaInit.stPMInit.dma_mem_addr = g_recv_buff;
    stUsartDmaInit.stPMInit.dma_mem_size = DEBUG_RECV_BUFF_SIZE;
    stUsartDmaInit.stPMInit.dma_datasize = 1;

    stUsartDmaInit.stPMInit.dma_int_enable = ENABLE;
    stUsartDmaInit.stPMInit.dma_int_flag[0] = DEBUG_DMA_INT_FLAG;
    stUsartDmaInit.stPMInit.dma_int_num = 1;
    stUsartDmaInit.dma_rcv_en = ENABLE;
    #endif
    drv_usart_dma_init(&stUsartDmaInit);
    
}

void DMA2_Stream2_IRQHandler()
{
    uint32_t i = 0;
    if (DMA_GetITStatus(DEBUG_USART_DMA_RCV_STREAM, DEBUG_DMA_INT_TC_FLAG) != RESET)
    {
        DMA_ClearITPendingBit(DEBUG_USART_DMA_RCV_STREAM, DEBUG_DMA_INT_TC_FLAG);
        //先把打印出来看看
    }
}
#endif

