
/*
首先是初始化, 由于其本身的用户很多

*/
#include "drv_usart.h"

void drv_usart0_init()
{
    rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_USART0);

    /*GPIO初始化*/
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

    /*USART初始化*/
    usart_deinit(USART0);
	usart_baudrate_set(USART0, 115200U);
	usart_receive_config(USART0, USART_RECEIVE_ENABLE);
	usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
	usart_interrupt_enable(USART0, USART_INT_RBNE);     // 打开接收中断
	usart_interrupt_disable(USART0, USART_INT_TBE);     // 关闭发送中断
	usart_interrupt_disable(USART0, USART_INT_TC);      // 
	usart_enable(USART0);
	nvic_irq_enable(USART0_IRQn, 1, 2);
}

void usart_send_data(uint32_t usart_periph, uint8_t data)
{
    /* 发送数据 */
    usart_data_transmit(usart_periph, data);
    while(RESET == usart_flag_get(usart_periph, USART_FLAG_TBE));
    return;
}

void usart_recv_data(uint32_t usart_periph, uint8_t * data)
{
    /* 接收数据 */
    while(RESET == usart_flag_get(usart_periph, USART_FLAG_RBNE));
    *data = (uint8_t)usart_data_receive(usart_periph);
    return;
}



