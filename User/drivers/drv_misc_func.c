
#include <stdint.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"


/*************************************************************
 * 
 *                  GPIO功能相关接口
 *      
 * 
 **************************************************************/
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

static uint32_t g_gpio_exti_line_set[] = 
{
    EXTI_Line0, EXTI_Line1, EXTI_Line2, EXTI_Line3, EXTI_Line4, EXTI_Line5, EXTI_Line6,
    EXTI_Line7, EXTI_Line8, EXTI_Line9, EXTI_Line10, EXTI_Line11, EXTI_Line12, EXTI_Line13,
    EXTI_Line14, EXTI_PinSource15
};

static uint8_t g_gpio_exti_pin_src_set[] = 
{
    EXTI_PinSource0, EXTI_PinSource1, EXTI_PinSource2, EXTI_PinSource3, EXTI_PinSource4, EXTI_PinSource5, EXTI_PinSource6,
    EXTI_PinSource7, EXTI_PinSource8, EXTI_PinSource9, EXTI_PinSource10, EXTI_PinSource11, EXTI_PinSource12, EXTI_PinSource13,
    EXTI_PinSource14, EXTI_PinSource15
};

static uint32_t g_gpio_exti_port_src_set[] = 
{
    EXTI_PortSourceGPIOA, EXTI_PortSourceGPIOB, EXTI_PortSourceGPIOC, EXTI_PortSourceGPIOD, 
    EXTI_PortSourceGPIOE, EXTI_PortSourceGPIOF, EXTI_PortSourceGPIOG,
    EXTI_PortSourceGPIOH, EXTI_PortSourceGPIOI, EXTI_PortSourceGPIOJ, EXTI_PortSourceGPIOK
};

uint8_t drv_gpio_get_exti_src(uint16_t gpio_pin)
{
    uint8_t i = 0;
    for (i = 0; i < sizeof(g_gpio_pin_set); ++i)
    {
        if (gpio_pin == g_gpio_pin_set[i])
        {
            return g_gpio_exti_pin_src_set[i];
        }
    }
    return g_gpio_exti_pin_src_set[0];
}

uint32_t drv_gpio_get_exti_line(uint16_t gpio_pin)
{
    uint8_t i = 0;
    for (i = 0; i < sizeof(g_gpio_pin_set); ++i)
    {
        if (gpio_pin == g_gpio_pin_set[i])
        {
            return g_gpio_exti_line_set[i];
        }
    }
    return g_gpio_exti_line_set[0];
}

uint32_t drv_gpio_get_exti_iqr_channel(uint16_t gpio_pin)
{
    switch(gpio_pin)
    {
        case GPIO_Pin_0:
            return EXTI0_IRQn;
        case GPIO_Pin_1:
            return EXTI1_IRQn;
        case GPIO_Pin_2:
            return EXTI2_IRQn;
        case GPIO_Pin_3:
            return EXTI3_IRQn;
        case GPIO_Pin_4:
            return EXTI4_IRQn;
        case GPIO_Pin_5:
        case GPIO_Pin_6:
        case GPIO_Pin_7:
        case GPIO_Pin_8:
        case GPIO_Pin_9:
            return EXTI9_5_IRQn;
        case GPIO_Pin_10:
        case GPIO_Pin_11:
        case GPIO_Pin_12:
        case GPIO_Pin_13:
        case GPIO_Pin_14:
        case GPIO_Pin_15:
            return EXTI15_10_IRQn;
        default:
            return 0;
    }
}

void drv_gpio_enable_clk(uint32_t gpio_handle)
{
    switch(gpio_handle)
    {
        case GPIOA:
            RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
            break;
        case GPIOB:
            RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
            break;
        case GPIOC:
            RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
            break;
        case GPIOG:
            RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);
            break;
        case GPIOF:
            RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
            break;
        default:
            break;
    }
}

void drv_gpio_exti_config(uint32_t gpio_handle, uint16_t gpio_pin)
{
    uint8_t exti_pin_src = drv_gpio_get_exti_src(gpio_pin);
    switch(gpio_handle)
    {
        case GPIOA:
            SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, exti_pin_src);
            break;
        case GPIOC:
            SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOC, exti_pin_src);
            break;
        default:
            break;
    }
}

uint32_t drv_gpio_get_pin(uint32_t pin_no)
{
    if (pin_no > 15)
    {
        return g_gpio_pin_set[0];
    }
    return g_gpio_pin_set[pin_no];
}

uint8_t drv_gpio_get_af_src(uint32_t gpio_pin)
{
    uint8_t i = 0;
    for (i = 0; i < sizeof(g_gpio_pin_set); ++i)
    {
        if (gpio_pin == g_gpio_pin_set[i])
        {
            return g_gpio_af_src_set[i];
        }
    }
    return g_gpio_af_src_set[0];
}

uint8_t drv_gpio_get_i2c_af(uint32_t i2c_handle)
{
    switch(i2c_handle)
    {
        case I2C1:
            return GPIO_AF_I2C1;
        case I2C2:
            return GPIO_AF_I2C2;
        case I2C3:
            return GPIO_AF_I2C3; 
        default:
            return GPIO_AF_I2C1; 
    }
}


uint8_t drv_gpio_get_usart_af(uint32_t usart_handle)
{
    switch(usart_handle)
    {
        case USART1:
            return GPIO_AF_USART1;
        case USART2:
            return GPIO_AF_USART2;
        case USART3:
            return GPIO_AF_USART3; 
        default:
            return GPIO_AF_USART1; 
    }
}






