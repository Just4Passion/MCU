
/*
首先是初始化, 由于其本身的用户很多

*/
#include "drv_usart.h"

/*
使用场景
    UART
    USART
    RS485: 
    RS232:
*/

typedef struct 
{
    uint32_t usart_clock;
    uint32_t gpio_tx_clock;
    uint32_t gpio_rx_clock;
    uint32_t usart_handle;
    uint32_t tx_gpio_handle;
    uint32_t tx_gpio_pin;
    uint32_t rx_gpio_handle;
    uint32_t rx_gpio_pin;

    uint32_t af_gpio_chan;
}USART_MAP;

static USART_MAP usart_map[] = 
{
    /*Tx->PA9, Rx->PA10*/
    {RCU_USART0, RCU_GPIOA, RCU_GPIOA, USART0, GPIOA, GPIO_PIN_9, GPIOA, GPIO_PIN_10, GPIO_AF_7},
    /*Tx->P15, Rx->PB3*/
    {RCU_USART0, RCU_GPIOA, RCU_GPIOB, USART0, GPIOA, GPIO_PIN_15, GPIOB, GPIO_PIN_3, GPIO_AF_7},
    /*Tx->PB6, Rx->PB7*/
    {RCU_USART0, RCU_GPIOB, RCU_GPIOB, USART0, GPIOB, GPIO_PIN_6, GPIOB, GPIO_PIN_7, GPIO_AF_7},
    /*Tx->PA2, Rx->PA3*/
    {RCU_USART1, RCU_GPIOA, RCU_GPIOA, USART1, GPIOA, GPIO_PIN_2, GPIOA, GPIO_PIN_3, GPIO_AF_7},
    /*Tx->PB10, Rx->PB11*/
    {RCU_USART2, RCU_GPIOB, RCU_GPIOB, USART2, GPIOB, GPIO_PIN_10, GPIOB, GPIO_PIN_11, GPIO_AF_7},
    /*Rx->PC5*/
    {RCU_USART2, RCU_GPIOC, RCU_GPIOC, USART2, 0, 0, GPIOC, GPIO_PIN_5, GPIO_AF_7},
    /*Tx->PC10, Rx->PC11*/
    {RCU_USART2, RCU_GPIOC, RCU_GPIOC, USART2, GPIOC, GPIO_PIN_10, GPIOC, GPIO_PIN_11, GPIO_AF_7},
    /*Tx->PA0, Rx->PA1*/
    {RCU_UART3, RCU_GPIOA, RCU_GPIOA, UART3, GPIOA, GPIO_PIN_0, GPIOA, GPIO_PIN_1, GPIO_AF_7},
    /*Tx->PC10, Rx->PC11*/
    {RCU_UART3, RCU_GPIOC, RCU_GPIOC, UART3, GPIOC, GPIO_PIN_10, GPIOC, GPIO_PIN_11, GPIO_AF_8},
    /*Tx->PC12, Rx->PD2*/
    {RCU_UART4, RCU_GPIOC, RCU_GPIOD, UART4, GPIOC, GPIO_PIN_12, GPIOD, GPIO_PIN_2, GPIO_AF_8},
    /*Tx->PC6, Rx->PC7*/
    {RCU_USART0, RCU_GPIOC, RCU_GPIOC, USART5, GPIOC, GPIO_PIN_6, GPIOC, GPIO_PIN_7, GPIO_AF_8},
};

static void usart_gpio_config(USART_MAP *usart_map)
{
    rcu_periph_clock_enable(usart_map->usart_clock);
    rcu_periph_clock_enable(usart_map->gpio_tx_clock);
    rcu_periph_clock_enable(usart_map->gpio_rx_clock);
    gpio_mode_set(usart_map->tx_gpio_handle, GPIO_MODE_AF, GPIO_PUPD_NONE, usart_map->tx_gpio_pin);
    gpio_af_set(usart_map->tx_gpio_handle, usart_map->af_gpio_chan, usart_map->tx_gpio_pin);
    gpio_output_options_set(usart_map->tx_gpio_handle, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, usart_map->tx_gpio_pin);

    gpio_mode_set(usart_map->rx_gpio_handle, GPIO_MODE_AF, GPIO_PUPD_NONE, usart_map->rx_gpio_pin);
    gpio_af_set(usart_map->rx_gpio_handle, usart_map->af_gpio_chan, usart_map->rx_gpio_pin);
    //gpio_output_options_set(usart_map->rx_gpio_handle, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, usart_map->rx_gpio_pin);
}

void drv_usart_init(USART_InitTypeDef * usart_init_struct)
{
    /* GD32F407RE初始化参数
    USART 0, 1, 2, 5
    UART 3, 4
    */
    uint32_t i = 0;
    /* GPIO初始化*/
    #if 1
    for (i = 0; i < sizeof(usart_map) / sizeof(usart_map[0]); i++)
    {
        if (usart_init_struct->usart_handle == usart_map[i].usart_handle
            && usart_init_struct->usart_tx_gpio == usart_map[i].tx_gpio_handle
            && usart_init_struct->usart_rx_gpio == usart_map[i].rx_gpio_handle)
        {
            usart_gpio_config(&usart_map[i]);
        }
    }
    #endif

    #if 1
    /* USART初始化 */
    usart_deinit(usart_init_struct->usart_handle);
    //设置波特率
    usart_baudrate_set(usart_init_struct->usart_handle, usart_init_struct->usart_baudrate);
    //设置采样参数
    usart_oversample_config(usart_init_struct->usart_handle, USART_OVSMOD_16);
    usart_sample_bit_config(usart_init_struct->usart_handle, USART_OSB_3bit);
    //设置数据位宽
    usart_word_length_set(usart_init_struct->usart_handle, usart_init_struct->usart_word_length);
    //设置停止位
    usart_stop_bit_set(usart_init_struct->usart_handle, usart_init_struct->usart_stop_bits);
    //设置校验位
    usart_parity_config(usart_init_struct->usart_handle, usart_init_struct->usart_parity);
    //设置收发使能
    usart_transmit_config(usart_init_struct->usart_handle, USART_TRANSMIT_ENABLE);
    usart_receive_config(usart_init_struct->usart_handle, USART_RECEIVE_ENABLE);
    //设置中断 
    usart_interrupt_enable(usart_init_struct->usart_handle, usart_init_struct->usart_interrupt);
    
    //启用中断
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    nvic_irq_enable(usart_init_struct->usart_irq, 15, 0);
    //NVIC_SetPriorityGrouping(0);
    //NVIC_SetPriority(usart_init_struct->usart_irq, 10);
	//NVIC_EnableIRQ(usart_init_struct->usart_irq);
    //启用USART
    usart_enable(usart_init_struct->usart_handle);
    #endif
    return;
}

void drv_usart_it_enable(uint32_t usart_periph, usart_interrupt_enum interrupt)
{
    usart_interrupt_enable(usart_periph, interrupt);
}

void drv_usart_it_disable(uint32_t usart_periph, usart_interrupt_enum interrupt)
{
    usart_interrupt_disable(usart_periph, interrupt);
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



