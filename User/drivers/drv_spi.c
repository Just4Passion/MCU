
#include <stdio.h>

#include "drv_spi.h"


static void drv_stm32_spi_clk_enable(uint32_t spi_handle)
{
    switch(spi_handle)
    {
        case SPI1:
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
            break;
        case SPI2:
            RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
            break;
        case SPI3:
            RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);
            break;
        default:
            break;
    }
}

/*首先是初始化接口*/
int drv_stm32_spi_bus_init(dy_bus_t *bus)
{
    printf("========================SPI bus [%s] init start===================\n", bus->name);
    dy_bus_state_t state = DY_BUS_STATE_UNINITIALIZED;
    dy_stm32_spi_bus_config_t *cfg = (dy_stm32_spi_bus_config_t *)(bus->priv_data);

    GPIO_InitTypeDef  stGPIOInit;
    SPI_InitTypeDef  stSPIInit;
    /*GPIO时钟*/
    RCC_AHB1PeriphClockCmd(cfg->sclk.clk, ENABLE);
    RCC_AHB1PeriphClockCmd(cfg->miso.clk, ENABLE);
    RCC_AHB1PeriphClockCmd(cfg->mosi.clk, ENABLE);
    RCC_AHB1PeriphClockCmd(cfg->cs.clk, ENABLE);

    /*SPI时钟*/
    drv_stm32_spi_clk_enable((uint32_t)cfg->spi_periph);

    /*GPIO引脚复用*/
    GPIO_PinAFConfig(cfg->sclk.port, cfg->sclk.pin_src, cfg->sclk.af);
	GPIO_PinAFConfig(cfg->miso.port, cfg->miso.pin_src, cfg->miso.af);
	GPIO_PinAFConfig(cfg->mosi.port, cfg->mosi.pin_src, cfg->mosi.af);
  
    /*GPIO初始化*/
    stGPIOInit.GPIO_Speed = GPIO_Speed_50MHz;
    stGPIOInit.GPIO_Mode = GPIO_Mode_AF;
    stGPIOInit.GPIO_OType = GPIO_OType_PP;
    stGPIOInit.GPIO_PuPd = GPIO_PuPd_NOPULL;  
    
    /*sclk*/
    stGPIOInit.GPIO_Pin = cfg->sclk.pin;
    GPIO_Init(cfg->sclk.port, &stGPIOInit);
    /*MISO*/
    stGPIOInit.GPIO_Pin = cfg->miso.pin;
    GPIO_Init(cfg->miso.port, &stGPIOInit);
    /*MOSI*/
    stGPIOInit.GPIO_Pin = cfg->mosi.pin;
    GPIO_Init(cfg->mosi.port, &stGPIOInit);

    /*片选*/
    stGPIOInit.GPIO_Mode = GPIO_Mode_OUT;
    stGPIOInit.GPIO_Pin = cfg->cs.pin;
    GPIO_Init(cfg->cs.port, &stGPIOInit);
    GPIO_SetBits(cfg->cs.port, cfg->cs.pin);

    /*SPI*/
    stSPIInit.SPI_Direction = cfg->spi_direction;
    stSPIInit.SPI_Mode =  cfg->spi_mode;
    stSPIInit.SPI_DataSize =  cfg->spi_datasize;
    stSPIInit.SPI_CPOL =  cfg->spi_cpol;
    stSPIInit.SPI_CPHA =  cfg->spi_cpha;
    stSPIInit.SPI_NSS =  cfg->spi_nss;
    stSPIInit.SPI_BaudRatePrescaler =  cfg->spi_baudrate_prescaler;
    stSPIInit.SPI_FirstBit =  cfg->spi_first_bit;
    stSPIInit.SPI_CRCPolynomial =  cfg->spi_crc_polynmial;
    SPI_Init(cfg->spi_periph, &stSPIInit);
    /*使能SPI1*/
    SPI_Cmd(cfg->spi_periph, ENABLE);

    /*总线初始化完毕*/
    state = DY_BUS_STATE_INITIALIZED;
    bus->ops->control(bus, DY_BUS_CTRL_CMD_SET_STATE, (void *)&state);

    printf("========================SPI bus [%s] init end===================\n", bus->name);

    return DY_EOK;
}


/*全双工模式写入*/
static int dy_stm32_spi_bus_full_duplex_send(dy_bus_t *bus, void *buf, unsigned int len)
{
    dy_spi_bus_t *spi_bus = (dy_spi_bus_t *)bus;
    dy_stm32_spi_bus_config_t *cfg = (dy_stm32_spi_bus_config_t *)(bus->priv_data);

    uint8_t *data = (uint8_t*)buf;
    uint32_t spi_timeout = spi_bus->spi_timeout_ms;
    uint32_t index = 0;

    for (index = 0; index < len; ++index)
    {
        spi_timeout = spi_bus->spi_timeout_ms;
        /*检查发送缓冲区是否为空*/
        while (SPI_I2S_GetFlagStatus(cfg->spi_periph, SPI_I2S_FLAG_TXE) == RESET)
        {
            if ((spi_timeout--) == 0 )
            {
                printf("==========write, wait snd timeout\r\n");
                return DY_ETMOUT;
            }
        }
        SPI_I2S_SendData(cfg->spi_periph, data[index]);

        spi_timeout = spi_bus->spi_timeout_ms;
        /*检查接收缓冲区是否为空*/
        while (SPI_I2S_GetFlagStatus(cfg->spi_periph, SPI_I2S_FLAG_RXNE) == RESET)
        {
            if((spi_timeout--) == 0)
            {
                printf("==========write, wait rcv timeout\r\n");
                return DY_ETMOUT;
            }
        }
        /*把数据从接收缓冲区清空*/
        SPI_I2S_ReceiveData(cfg->spi_periph);
    }
    return len;
}

/*全双工模式读取*/
static int dy_stm32_spi_bus_full_duplex_recv(dy_bus_t *bus, void *buf, unsigned int len)
{
    dy_spi_bus_t *spi_bus = (dy_spi_bus_t *)bus;
    dy_stm32_spi_bus_config_t *cfg = (dy_stm32_spi_bus_config_t *)(bus->priv_data);

    uint8_t *data = (uint8_t*)buf;
    uint32_t spi_timeout = spi_bus->spi_timeout_ms;
    uint32_t index = 0;

    for (index = 0; index < len; ++index)
    {
        spi_timeout = spi_bus->spi_timeout_ms;
        /*检查发送缓冲区是否为空*/
        while (SPI_I2S_GetFlagStatus(cfg->spi_periph, SPI_I2S_FLAG_TXE) == RESET)
        {
            if ((spi_timeout--) == 0 )
            {
                printf("==========read, wait snd timeout\r\n");
                return DY_ETMOUT;
            }
        }
        /*发送一个假数据, 触发接收*/
        SPI_I2S_SendData(cfg->spi_periph, 0xff);

        spi_timeout = spi_bus->spi_timeout_ms;
        /*检查接收缓冲区是否为空*/
        while (SPI_I2S_GetFlagStatus(cfg->spi_periph, SPI_I2S_FLAG_RXNE) == RESET)
        {
            if((spi_timeout--) == 0)
            {
                printf("==========read, wait rcv timeout\r\n");
                return DY_ETMOUT;
            }
        }
        /*把数据从接收缓冲区清空*/
        data[index] = SPI_I2S_ReceiveData(cfg->spi_periph);
    }
    return index;
}

static int dy_stm32_spi_bus_control(dy_bus_t *bus, int cmd, void *arg)
{
    dy_spi_bus_t *spi_bus = (dy_spi_bus_t *)bus;
    dy_stm32_spi_bus_config_t *cfg = (dy_stm32_spi_bus_config_t *)(bus->priv_data);
    /*拉低/拉高某个片选信号*/
    switch (cmd)
    {
        case DY_SPI_CTRL_CMD_CS_LOW:
            /*arg中包含片选引脚信息： 目前仅支持一个片选信号*/
            GPIO_ResetBits(cfg->cs.port, cfg->cs.pin);
            break;
        case DY_SPI_CTRL_CMD_CS_HIGH:
            /*arg中包含片选引脚信息: 比如是片选信号的索引值*/
            GPIO_SetBits(cfg->cs.port, cfg->cs.pin);
            break;
        default:
            dy_bus_control(bus, cmd, arg);
            break;
    }
    return DY_EOK;
}


dy_stm32_spi_bus_config_t stm32_spi1_impl = {
    .sclk = {
        .port = GPIOB,
        .clk = RCC_AHB1Periph_GPIOB,
        .pin = GPIO_Pin_3,
        .pin_src = GPIO_PinSource3,
        .af = GPIO_AF_SPI1
    },
    .miso = {
        .port = GPIOB,
        .clk = RCC_AHB1Periph_GPIOB,
        .pin = GPIO_Pin_4,
        .pin_src = GPIO_PinSource4,
        .af = GPIO_AF_SPI1
    },
    .mosi = {
        .port = GPIOB,
        .clk = RCC_AHB1Periph_GPIOB,
        .pin = GPIO_Pin_5,
        .pin_src = GPIO_PinSource5,
        .af = GPIO_AF_SPI1
    },
    .sdev_num = 1,
    .cs = {
        .port = GPIOG,
        .clk = RCC_AHB1Periph_GPIOG,
        .pin = GPIO_Pin_6,
    },
    .spi_periph = SPI1,
    .periph_clk = RCC_APB2Periph_SPI1,
    .spi_direction = SPI_Direction_2Lines_FullDuplex,
    .spi_mode = SPI_Mode_Master,
    .spi_datasize = SPI_DataSize_8b,
    .spi_cpol = SPI_CPOL_High,
    .spi_cpha = SPI_CPHA_2Edge,
    .spi_nss = SPI_NSS_Soft,
    .spi_baudrate_prescaler = SPI_BaudRatePrescaler_2,
    .spi_first_bit = SPI_FirstBit_MSB,
    .spi_crc_polynmial = 7
};

dy_bus_ops_t spi_bus_ops = {
    .init = drv_stm32_spi_bus_init,
    .send = dy_stm32_spi_bus_full_duplex_send,
    .recv = dy_stm32_spi_bus_full_duplex_recv,
    .control = dy_stm32_spi_bus_control
};

dy_spi_bus_t stm32_spi1_bus = {
    .bus = {
        .name = "SPI1",
        .type = DY_BUS_TYPE_SPI,
        .state = DY_BUS_STATE_UNINITIALIZED,
        .ops = &spi_bus_ops,
        .device_count = 0,
        .device_list = NULL,
        .next = NULL,
        .priv_data = (void*)&stm32_spi1_impl
    },
    .spi_timeout_ms = 5000
};


/*注册总线*/
int drv_spi_hw_init(void)
{
    /*初始化I2C0总线配置: 这里使用的是全局初始化*/
    /*注册I2C总线到管理器*/
    if (DY_EOK == dy_bus_register((dy_bus_t *)&stm32_spi1_bus))
    {
        stm32_spi1_bus.bus.ops->init(&stm32_spi1_bus.bus);
    }
    else
    {
        return DY_ERROR;
    }
    
    return DY_EOK;
}

