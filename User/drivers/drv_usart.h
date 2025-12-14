

#ifndef DRV_USART_H
#define DRV_USART_H

/**********************************************
 * 
 * 
 *                  硬件 USART 总线模型
 * 先编写出USART, 再特化出总线结构
 * 
 * TTL串口: 点对点. 0, 3.3V+
 * RS232: 点对点. 全双工. -15~-5, 5~15V
 * RS485: 总线. 半双工. 存在地址, 可以挂在多个设备
 * 
 *
 * 
************************************************/

/**********************************************
 * 
 *              类型定义
 * 
 ***********************************************/
typedef enum
{
    USART_SET_BAUD_RATE,
}usart_cmd_t;

typedef struct 
{
    /*端口*/
    USART_TypeDef *usart_handle;
    uint32_t usart_clk;

    uint32_t tx_clk;
    GPIO_TypeDef *tx_port;
    uint16_t tx_pin;

    uint32_t rx_clk;
    GPIO_TypeDef *rx_port;
    uint16_t rx_pin;

    /*基本参数*/
    uint32_t mode;                      // 输入输出
    uint32_t baud_rate;                 // 波特率
    uint32_t word_length;               // 字长: 8b, 9b
    uint32_t hardware_flow_control;     // 硬件流控
    uint32_t parity;                    // 奇偶校验
    uint32_t stop_bits;                 // 停止位

    /*中断使能*/
    IRQn_Type uart_irq_type;                // 使能中断, 就监听所有中断标志位

    /**********************************************
    * DMA配置: 流(走那条路), 通道(为谁提供DMA), 中断. 
    * 两个DAM, 16个流; 每个流有多达8个通道
    * 比如DAM1数据流0: SPI3通道, I2C1通道, TIM4_CH1通道, I2S3通道, UART5通道, UART8通道, TIM5通道
    **********************************************/
    uint32_t dma_stream;
    uint32_t dma_channel;
    IRQn_Type dma_irq_type;                 // 使能中断, 就监听所有中断标志: 也可以尝试监听重要的标志
}dy_stm32_usart_config_t;


/*对于RS485, 它是总线型, 可以挂载多个负载*/
typedef struct
{
	int (*master_xfer)(dy_bus_t *bus, void *buf, uint32_t len);
    int (*slave_xfer)(dy_bus_t *bus, void *buf, uint32_t len);
    int (*rs485_bus_control)(dy_bus_t *bus, int cmd, void *args);       // 需要切换发送/接收模式
}dy_usart_rs485_ops_t;

typedef struct
{
    dy_bus_t bus;
    dy_usart_rs485_ops_t ops;
}dy_usart_rs485_bus_t;


/*对于通用的usart, 它是点对点的通信*/
typedef struct
{
    int (*usart_bus_control)(dy_bus_t *bus, int cmd, void *args);
}dy_usart_ops_t;

typedef struct 
{
    dy_bus_t bus;
    dy_usart_ops_t ops;
}dy_usart_bus_t;



/**********************************************
 * 
 *                  服务接口
 * 
 ***********************************************/
int drv_usart_hw_init(void);

#endif

