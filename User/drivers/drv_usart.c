
#include "stm32f4xx.h"
#include "drv_usart.h"

/*
# USART概述

## 挂载总线
    AHB/APB2
        - USART1
        - USART6
    AHB/APB1
        - USART2
        - USART3
        - UART4
        - UART5


*/

/*
USART与端口的映射关系
*/

/* 时钟, 配置寄存器 */
#define USART_CLK RCC_APB2Periph_USART1
#define GPIO_TX_CLK RCC_AHB1Periph_GPIOA
#define GPIO_RX_CLK RCC_AHB1Periph_GPIOA
/* USART通道 */
#define USART_X USART1
/* 发送相关GPIO */
#define USART_TX_GPIO GPIOA
#define USART_TX_GPIO_PIN GPIO_Pin_9
/* 接收相关GPIO */
#define USART_RX_GPIO GPIOA
#define USART_RX_GPIO_PIN GPIO_Pin_10
/* 发送GPIO复用关系 */
#define USART_AF_TX_GPIO GPIO_AF_USART1
#define USART_AF_TX_PIN GPIO_PinSource9
/* 接收GPIO复用关系 */
#define USART_AF_RX_GPIO GPIO_AF_USART1
#define USART_AF_RX_PIN GPIO_PinSource10

#define GPIO_AF_TX GPIO_PinAFConfig(USART_TX_GPIO, USART_AF_TX_PIN, USART_AF_TX_GPIO)
#define GPIO_AF_RX GPIO_PinAFConfig(USART_RX_GPIO, USART_AF_RX_PIN, USART_AF_RX_GPIO)

/* 中断通道与中断标志 */
#define USART_INT_CHAN USART1_IRQn
#define USART_INT_FLAG USART_IT_RXNE

static uint32_t g_gpio_pin_set[] = 
{
    GPIO_Pin_0, GPIO_Pin_1, GPIO_Pin_2, GPIO_Pin_3, GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_6, GPIO_Pin_7, GPIO_Pin_8,
    GPIO_Pin_9, GPIO_Pin_10, GPIO_Pin_11, GPIO_Pin_12, GPIO_Pin_13, GPIO_Pin_14, GPIO_Pin_15
};

static uint32_t g_gpio_af_src_set[] = 
{
    GPIO_PinSource0, GPIO_PinSource1, GPIO_PinSource2, GPIO_PinSource3, GPIO_PinSource4, GPIO_PinSource5, GPIO_PinSource6,
    GPIO_PinSource7, GPIO_PinSource8, GPIO_PinSource9, GPIO_PinSource10, GPIO_PinSource11, GPIO_PinSource12, GPIO_PinSource13,
    GPIO_PinSource14, GPIO_PinSource15
};

/*
先增加USART1, 后续有需要再增加
*/
static void drv_usart_enable_clk(uint32_t usart_handle)
{
    switch(usart_handle)
    {
        case USART1:
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
            break;
        default:
            break;
    }
}

static void drv_usart_gpio_enable_clk(uint32_t gpio_handle)
{
    switch(gpio_handle)
    {
        case GPIOA:
            RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
            break;
        default:
            break;
    }
}

static uint32_t drv_usart_gpio_get_af(uint32_t usart_handle)
{
    switch(usart_handle)
    {
        case USART1:
            return GPIO_AF_USART1;
        default:
            return 0;
    }
}

static uint32_t drv_usart_get_irq(uint32_t usart_handle)
{
    switch(usart_handle)
    {
        case USART1:
            return USART1_IRQn;
        default:
            return 0;
    }
}

static void drv_usart_dma_enable_clk(uint32_t usart_handle)
{
    switch(usart_handle)
    {
        case USART1:
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
            RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
            break;
    }
}


void drv_usart_init(DRV_USART_INIT *usart_init)
{
    uint32_t i = 0;
    GPIO_InitTypeDef stGPIOInit;
    USART_InitTypeDef stUSARTInit;
    NVIC_InitTypeDef stNVICInit;
    /*
    时钟
    */
    drv_usart_enable_clk(usart_init->usart_handle);
    drv_usart_gpio_enable_clk(usart_init->tx_gpio);
    drv_usart_gpio_enable_clk(usart_init->rx_gpio);
    /*复用*/
    GPIO_PinAFConfig(usart_init->tx_gpio, g_gpio_af_src_set[usart_init->tx_gpio_pin], 
        drv_usart_gpio_get_af(usart_init->usart_handle));
	GPIO_PinAFConfig(usart_init->rx_gpio, g_gpio_af_src_set[usart_init->rx_gpio_pin], 
        drv_usart_gpio_get_af(usart_init->usart_handle));
    /*
    /*
    GPIO配置
    */
    stGPIOInit.GPIO_Mode = GPIO_Mode_AF;
	stGPIOInit.GPIO_OType = GPIO_OType_PP;
	stGPIOInit.GPIO_PuPd = GPIO_PuPd_UP;
	stGPIOInit.GPIO_Speed = GPIO_Speed_50MHz;
    /*发送*/
    stGPIOInit.GPIO_Pin = g_gpio_pin_set[usart_init->tx_gpio_pin];
	GPIO_Init(usart_init->tx_gpio, &stGPIOInit);
    /*接收*/
    stGPIOInit.GPIO_Pin = g_gpio_pin_set[usart_init->rx_gpio_pin];
	GPIO_Init(usart_init->rx_gpio, &stGPIOInit);
    /*
    USART配置
    */
	stUSARTInit.USART_BaudRate = 115200;					    //波特率115200
	stUSARTInit.USART_WordLength = USART_WordLength_8b;		    //使用8位字长
	stUSARTInit.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	//不使用硬件流
	stUSARTInit.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	    //使能发送和接收
	stUSARTInit.USART_Parity = USART_Parity_No;				    //不使用校验位
	stUSARTInit.USART_StopBits = USART_StopBits_1;			    //1位停止位
	USART_Init(usart_init->usart_handle, &stUSARTInit);

    /*
    中断
    */
    if (usart_init->usart_int_enable)
    {
        NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
        stNVICInit.NVIC_IRQChannel = drv_usart_get_irq(usart_init->usart_handle);   //黄色灯亮
        stNVICInit.NVIC_IRQChannelPreemptionPriority = 2;
        stNVICInit.NVIC_IRQChannelSubPriority = 2;
        stNVICInit.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&stNVICInit);
        //USART_IT_RXNE
        for(i = 0; i < usart_init->usart_int_num; ++i)
        {
            USART_ITConfig(usart_init->usart_handle, usart_init->usart_int_flag[i], ENABLE);
        }
    }
    /*
    启用USART
    */
    USART_Cmd(usart_init->usart_handle, ENABLE);
}

void drv_usart_dma_init(DRV_USART_DMA_INIT *usart_dma_init)
{
    /*
    启用USART的时钟
    */
    drv_usart_enable_clk(usart_dma_init->stUSARTInit.usart_handle);

    /*
    启用DMA
    */
    if (usart_dma_init->dma_snd_en)
    {
        drv_dma_mp_init(&usart_dma_init->stMPInit);
        USART_DMACmd(usart_dma_init->stUSARTInit.usart_handle, USART_DMAReq_Tx, ENABLE);
    }

    if (usart_dma_init->dma_rcv_en)
    {
        drv_dma_mp_init(&usart_dma_init->stPMInit);
        USART_DMACmd(usart_dma_init->stUSARTInit.usart_handle, USART_DMAReq_Rx, ENABLE);
    }

    /*
    启用USART
    */
    drv_usart_init(&usart_dma_init->stUSARTInit);
}

void drv_usart_send_byte(USART_TypeDef *usart_handle, uint8_t data)
{
    /*
    发送, 直到发送完成
    */
    USART_SendData(usart_handle, data);
    while(USART_GetFlagStatus(usart_handle, USART_FLAG_TXE) == RESET);
}

void drv_usart_recv_byte(USART_TypeDef *usart_handle, uint8_t *data)
{
    /*
    可以接收, 接收
    */
    while(USART_GetFlagStatus(usart_handle, USART_FLAG_RXNE) == RESET);
    *data = USART_ReceiveData(usart_handle);
}

uint32_t drv_usart_get_flag(USART_TypeDef *usart_handle, uint32_t usart_sr_flag)
{
    return USART_GetFlagStatus(usart_handle, usart_sr_flag);
}

uint32_t drv_usart_get_it_status(USART_TypeDef *usart_handle, uint32_t usart_sr_flag)
{
    return USART_GetITStatus(usart_handle, usart_sr_flag);
}


