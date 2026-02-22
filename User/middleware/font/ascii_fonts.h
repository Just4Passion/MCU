
#ifndef ASCII_FONTS_H
#define ASCII_FONTS_H

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"


typedef struct
{
    const uint8_t *font_table;
    uint16_t width;
    uint16_t height;
}ascii_font_t;


extern ascii_font_t ascii_font_8x16;
extern ascii_font_t ascii_font_16x32;
extern ascii_font_t ascii_font_24x48;


#endif
