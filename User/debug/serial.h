
#ifndef SERIAL_H
#define SERIAL_H

#include "gd32f4xx.h"
#include "portmacro.h"

signed portBASE_TYPE serial_put_char(uint8_t cOutChar, TickType_t xBlockTime);
void serial_put_string(const char * const pcString, unsigned short usStringLength );
signed portBASE_TYPE serial_get_char(char *cChar, TickType_t xBlockTime);


void serial_init(void);
void serial_usart_interrupt_handle(void);
void serial_recv_cmd_deal(void);

#endif
