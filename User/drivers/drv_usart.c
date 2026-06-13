
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"

#include "drv_misc_func.h"
#include "drv_usart.h"


#define USART_TIMEROUT (0x1000)


/**********************************************
 * 
 *              通用UART外设
 * 基础实现, 先使用轮询的方式
 * 后续加入中断, DMA方式
 * 
 ***********************************************/

/**
 * @brief 总线初始化
 */
static int drv_stm32_usart_bus_init(dy_bus_t *bus)
{
    dy_bus_state_t state = DY_BUS_STATE_UNINITIALIZED;
    dy_stm32_usart_config_t *usart_config = (dy_stm32_usart_config_t *)bus->priv_data;

    GPIO_InitTypeDef stGPIOInit;
    USART_InitTypeDef stUSARTInit;
    /*初始化USART*/
    /*时钟启用*/
    RCC_APB2PeriphClockCmd(usart_config->usart_clk, ENABLE);
    RCC_AHB1PeriphClockCmd(usart_config->tx_clk, ENABLE);
    RCC_AHB1PeriphClockCmd(usart_config->rx_clk, ENABLE);

    /*GPIO端口复用映射: GPIO->Pin->I2C*/
    GPIO_PinAFConfig(usart_config->tx_port, drv_gpio_get_af_src(usart_config->tx_pin), drv_gpio_get_usart_af((uint32_t)usart_config->usart_handle));
    GPIO_PinAFConfig(usart_config->rx_port, drv_gpio_get_af_src(usart_config->rx_pin), drv_gpio_get_usart_af((uint32_t)usart_config->usart_handle));

    /*初始化GPIO: 发送和接收*/
    stGPIOInit.GPIO_Mode = GPIO_Mode_AF;      // 复用
	stGPIOInit.GPIO_OType = GPIO_OType_PP;  // 推挽
	stGPIOInit.GPIO_PuPd = GPIO_PuPd_UP;    // 上拉
	stGPIOInit.GPIO_Speed = GPIO_Speed_50MHz;
    stGPIOInit.GPIO_Pin = usart_config->tx_pin;
    GPIO_Init(usart_config->tx_port, &stGPIOInit);
    stGPIOInit.GPIO_Pin = usart_config->rx_pin;
    GPIO_Init(usart_config->rx_port, &stGPIOInit);
    /*初始化USART*/
    stUSARTInit.USART_Mode = usart_config->mode;	                    //发送和接收
    stUSARTInit.USART_BaudRate = usart_config->baud_rate;				//波特率115200
	stUSARTInit.USART_WordLength = usart_config->word_length;		    //使用8位字长
	stUSARTInit.USART_HardwareFlowControl = usart_config->hardware_flow_control;	//硬件流控: 发送请求, 接收请求
	stUSARTInit.USART_Parity = usart_config->parity;				    //校验位
	stUSARTInit.USART_StopBits = usart_config->stop_bits;			    //停止位
	USART_Init(usart_config->usart_handle, &stUSARTInit);
    /*使能中断*/

    /*使能DMA*/

    /*使能USART*/
    USART_Cmd(usart_config->usart_handle, ENABLE);

    state = DY_BUS_STATE_INITIALIZED;
    bus->ops->control(bus, DY_BUS_CTRL_CMD_SET_STATE, (void *)&state);
    return DY_EOK;
}

/**
 * @brief 发送1字节数据
 */
static int drv_stm32_usart_bus_send_byte(dy_bus_t *bus, uint8_t data)
{
    /*
    发送, 直到发送完成
    */
    uint32_t timeout = USART_TIMEROUT;
    dy_stm32_usart_config_t *usart_config = (dy_stm32_usart_config_t *)bus->priv_data;
    USART_SendData(usart_config->usart_handle, data);
    while(USART_GetFlagStatus(usart_config->usart_handle, USART_FLAG_TXE) == RESET)
    {
        /*超时*/
        if (0 == timeout--)
        {
            return DY_ETMOUT;
        }
    }
    return 1;
}

/**
 * @brief 接收1字节数据
 */
static int drv_stm32_usart_bus_recv_byte(dy_bus_t *bus, uint8_t *data)
{
    uint32_t timeout = USART_TIMEROUT;
    dy_stm32_usart_config_t *usart_config = (dy_stm32_usart_config_t *)bus->priv_data;
    while(USART_GetFlagStatus(usart_config->usart_handle, USART_FLAG_RXNE) == RESET)
    {
        /*超时*/
        if (0 == timeout--)
        {
            return DY_ETMOUT;
        }
    }
    *data = USART_ReceiveData(usart_config->usart_handle);
    return 1;
}

/**
 * @brief 发送指定数量字节
 */
static int drv_stm32_usart_send(dy_bus_t *bus, void *buf, uint32_t len)
{
		uint8_t *data = (uint8_t *)buf;
    uint32_t i = 0;
    for (i = 0; i < len; ++i)
    {
        if (1 != drv_stm32_usart_bus_send_byte(bus, data[i]))
        {
            /*发送超时*/
            return i;
        }
    }
    return len;
}

/**
 * @brief 接收指定数量字节
 */
static int drv_stm32_usart_recv(dy_bus_t *bus, void *buf, uint32_t len)
{
	uint8_t *data = (uint8_t *)buf;
    uint32_t i = 0;
    for (i = 0; i < len; ++i)
    {
        if (1 != drv_stm32_usart_bus_recv_byte(bus, &data[i]))
        {
            return i;
        }
    }
    return len;
}

static int drv_stm32_usart_bus_control(dy_bus_t *bus, int cmd, void *args)
{
    dy_stm32_usart_config_t *usart_config = (dy_stm32_usart_config_t *)bus->priv_data;
    switch (cmd)
    {
        case USART_SET_BAUD_RATE:
            usart_config->baud_rate = *((uint32_t*)args);
            /*首先去去使能*/
            USART_Cmd(usart_config->usart_handle, DISABLE);
            /*重新初始化*/
            bus->ops->init(bus);
            break;
        default:
            break;
    }
    return DY_EOK;
}


dy_bus_ops_t usart_bus_ops = {
    .init = drv_stm32_usart_bus_init,
    .send = drv_stm32_usart_send,
    .recv = drv_stm32_usart_recv,
    .control = dy_bus_control
};


dy_stm32_usart_config_t usart1_impl = {
    .usart_handle = USART1,
    .usart_clk = RCC_APB2Periph_USART1,
    /*发送*/
    .tx_clk = RCC_AHB1Periph_GPIOA,
    .tx_port = GPIOA,
    .tx_pin = GPIO_Pin_9,
    /*接收*/
    .rx_clk = RCC_AHB1Periph_GPIOA,
    .rx_port = GPIOA,
    .rx_pin = GPIO_Pin_10,
    /*基本参数*/
    .mode = (USART_Mode_Rx | USART_Mode_Tx),                    // 输入输出
    .baud_rate = 115200,                                        // 波特率
    .word_length = USART_WordLength_8b,                         // 字长: 8b, 9b
    .hardware_flow_control = USART_HardwareFlowControl_None,   // 硬件流控
    .parity = USART_Parity_No,                                  // 奇偶校验
    .stop_bits = USART_StopBits_1,                              // 停止位
};

dy_usart_bus_t usart1_stm32_bus = {
    .bus = {
        .name = "USART1",
        .type = DY_BUS_TYPE_USART,
        .state = DY_BUS_STATE_UNINITIALIZED,
        .ops = &usart_bus_ops,
        .device_count = 0,
        .device_list = NULL,
        .next = NULL,
        .priv_data = (void *)&usart1_impl,
    },
    .ops = {
        .usart_bus_control = drv_stm32_usart_bus_control,
    }
};

int drv_usart_hw_init(void)
{
    /*注册I2C总线到管理器*/
    if (DY_EOK != dy_bus_register((dy_bus_t *)&usart1_stm32_bus))
    {
        return DY_ERROR;
    }
    usart1_stm32_bus.bus.ops->init(&usart1_stm32_bus.bus);
    
    return DY_EOK;
}


