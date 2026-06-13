#ifndef DRV_FSMC_LCD_H
#define DRV_FSMC_LCD_H

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"

typedef enum
{
    LCD_RST = (DY_DEVICE_CTRL_CMD_MAX + 1),
    LCD_REG_CONFIG,
    LCD_GRAM_SCAN_CONFIG,
    LCD_SET_PIXEL_COLOR,
    LCD_SET_BACKGROUND_COLOR,
    LCD_CLEAR_SCREEN,
    LCD_OPEN_BK_LIGHT,
    LCD_CLOSE_BK_LIGHT,
    LCD_DRAW_LINE,
    LCD_DRAW_RECTANGLE,
    LCD_DRAW_CIRCLE,
    LCD_DISPLAY_EN_CHAR,
    LCD_DISPLAY_EN_STRING
}fsmc_lcd_ctrl_cmd_t;

typedef enum
{
    LCD_PIXEL_WHITE = 0xFFFF,
    LCD_PIXEL_BLACK = 0x0000,
    LCD_PIXEL_GREEY = 0xF7DE,
    LCD_PIXEL_BLUE = 0x001F,	    //蓝色 
    LCD_PIXEL_BLUE2 = 0x051F,	    //浅蓝色 
    LCD_PIXEL_RED = 0xF800,	        //红色 
    LCD_PIXEL_MAGENTA = 0xF81F,	    //红紫色，洋红色 
    LCD_PIXEL_GREEN = 0x07E0,	    //绿色 
    LCD_PIXEL_CYAN = 0x7FFF,	    //蓝绿色，青色 
    LCD_PIXEL_YELLOW = 0xFFE0,	    //黄色 
    LCD_PIXEL_BRED = 0xF81F,
    LCD_PIXEL_GRED = 0xFFE0,
    LCD_PIXEL_GBLUE = 0x07FF
}fsmc_lcd_pixel_color_t;

typedef enum
{
    LCD_ASCII_FONT_TABLE_8X16,
    LCD_ASCII_FONT_TABLE_16X32,
    LCD_ASCII_FONT_TABLE_24X48
}fsmc_lcd_font_table_t;

typedef struct 
{
    uint16_t x;
    uint16_t y;
}fsmc_lcd_point_t;

typedef struct
{
    fsmc_lcd_point_t start_point;
    uint16_t width;
    uint16_t height;
}fsmc_lcd_window_config_t;

typedef struct
{
    fsmc_lcd_point_t point1;
    fsmc_lcd_point_t point2;
    uint16_t color;
}fsmc_lcd_line_t;

typedef struct
{
    fsmc_lcd_point_t start_point;
    uint16_t width;
    uint16_t height;
    uint16_t color;
    bool filled;
}fsmc_lcd_rectangle_t;

typedef struct
{
    fsmc_lcd_point_t center;
    uint16_t radius;
    uint16_t color;
    bool filled;
}fsmc_lcd_circle_t;

typedef struct
{
    fsmc_lcd_point_t position;
    uint16_t color;
    char *str;
}fsmc_lcd_string_t;


int drv_fsmc_lcd_init();


#endif