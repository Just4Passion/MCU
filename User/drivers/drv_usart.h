#ifndef DRV_USART
#define DRV_USART

#include "gd32f30x.h"
#include "gd32f30x_libopt.h"

void drv_usart0_init();
void usart_send_data(uint32_t usart_periph, uint8_t data);
void usart_recv_data(uint32_t usart_periph, uint8_t *data);

#endif
