#ifndef DRV_USART
#define DRV_USART

#include "stm32f4xx.h"
#include "drv_dma.h"

typedef struct 
{
    USART_TypeDef *usart_handle;
    GPIO_TypeDef *tx_gpio;
    uint32_t tx_gpio_pin;
    GPIO_TypeDef *rx_gpio;
    uint32_t rx_gpio_pin;

    uint32_t usart_mode;
    uint32_t usart_baud_rate;
    uint32_t usart_word_length;
    uint32_t usart_hardware_flow_control;
    uint32_t usart_parity;
    uint32_t usart_stop_bits;

    uint32_t usart_int_enable;
    uint32_t usart_int_flag[8];
    uint32_t usart_int_num;
}DRV_USART_INIT;

typedef struct 
{
    DRV_USART_INIT stUSARTInit;

    uint8_t dma_snd_en;
    DRV_DMA_MP_INIT stMPInit;

    uint32_t dma_rcv_en;
    DRV_DMA_MP_INIT stPMInit;
}DRV_USART_DMA_INIT;

void drv_usart_init(DRV_USART_INIT *usart_init);
void drv_usart_dma_init(DRV_USART_DMA_INIT *usart_dma_init);
void drv_usart_send_byte(USART_TypeDef *usart_handle, uint8_t data);
void drv_usart_recv_byte(USART_TypeDef *usart_handle, uint8_t *data);
uint32_t drv_usart_get_flag(USART_TypeDef *usart_handle, uint32_t usart_sr_flag);

#endif


