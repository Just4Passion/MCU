#ifndef DRV_USART
#define DRV_USART

#include "gd32f4xx.h"

typedef struct 
{
    uint32_t usart_handle;
    uint32_t usart_irq;
    uint32_t usart_tx_gpio;
    uint32_t usart_rx_gpio;
    uint32_t usart_baudrate;        //波特率
    uint16_t usart_word_length;     //位宽
    uint16_t usart_stop_bits;       //停止位
    uint16_t usart_parity;          //就校验
    uint16_t usart_interrupt;       //使能哪些中断：中断可以相或
}USART_InitTypeDef;

void drv_usart_init(USART_InitTypeDef * usart_init_struct);
void usart_send_data(uint32_t usart_periph, uint8_t data);
void usart_recv_data(uint32_t usart_periph, uint8_t *data);

#endif
