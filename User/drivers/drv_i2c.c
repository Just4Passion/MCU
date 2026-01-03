

#include <stdlib.h>
#include <stdio.h>

#include "drv_misc_func.h"
#include "drv_i2c.h"


/**********************************************
 * 
 *                  宏定义
 * 
***********************************************/
#define I2C_TIMEOUT 5000U


/**********************************************
 * 
 *                  关于I2C
 * I2C事务: 起始信号+地址+读/写标志+数据+停止信号
 * I2C批量读写: 起始信号+地址+写标志+数据+起始信号+地址+读标志+数据+起始信号+写标志+数据...
 * 
***********************************************/
/**
 * @brief I2C发送起始信号+地址: 经过测试
 */
static int drv_stm32_i2c_bus_start_addr(dy_bus_t *bus, uint32_t addr, uint8_t flag)
{
    //dy_i2c_bus_t *i2c_bus = (dy_i2c_bus_t *)bus;
    dy_stm32_i2c_bus_config_t *cfg = (dy_stm32_i2c_bus_config_t *)(bus->priv_data);
    uint32_t i2c_timeout = I2C_TIMEOUT;
    /*写数据, 也就是先发送地址+写标志位, 然后再发送数据*/
    /*首先检查总线是否空闲*/
    while(RESET != I2C_GetFlagStatus(cfg->i2c_periph, I2C_FLAG_BUSY))
    {
        if ((i2c_timeout--) ==0)
        {
            return DY_I2C_EBSY_TMOUT;
        }
    }
    /*发送起始信号, 并等待信号发送完毕*/
    i2c_timeout = I2C_TIMEOUT;
    I2C_GenerateSTART(cfg->i2c_periph, ENABLE);
    while(SUCCESS != I2C_CheckEvent(cfg->i2c_periph, I2C_EVENT_MASTER_MODE_SELECT))  //BUSY, MSL and SB flag
    {
        if ((i2c_timeout--) == 0)
        {
            return DY_I2C_ESBSEND_TMOUT;
        }
    }
    
    /*发送地址 + 读写标志位*/
    i2c_timeout = I2C_TIMEOUT;
    I2C_Send7bitAddress(cfg->i2c_periph, addr, (0 == flag ? I2C_Direction_Transmitter : I2C_Direction_Receiver));
    while (ERROR == I2C_CheckEvent(cfg->i2c_periph, (0 == flag ? I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED : I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)))
    {
        if ((i2c_timeout--) == 0)
        {
            return DY_I2C_EADDSEND_TMOUT;
        }
    }
    
    //
    return DY_I2C_EOK;
}

/**********************************************
 * 
 * 
 *  这里要显示总线的特化操作, 然后注册这个总线
 *  硬件I2C外设, 和软件I2C, 都在这里实现并注册
 * 
 * 
************************************************/
/**
 * @brief I2C总线初始化: 经过测试
 */
int drv_stm32_i2c_bus_init(dy_bus_t *bus)
{
    printf("========================I2C bus [%s] init start===================\n", bus->name);
    //I2C_EE_Init();
    #if 1
    dy_bus_state_t state = DY_BUS_STATE_UNINITIALIZED;
    dy_stm32_i2c_bus_config_t *cfg = (dy_stm32_i2c_bus_config_t *)(bus->priv_data);

    GPIO_InitTypeDef  stGPIOInit;
    I2C_InitTypeDef  stI2CInit;

    /*时钟启用*/
    RCC_APB1PeriphClockCmd(cfg->periph_clk, ENABLE);
    RCC_AHB1PeriphClockCmd(cfg->scl_clk, ENABLE);
    RCC_AHB1PeriphClockCmd(cfg->sda_clk, ENABLE);

    /*GPIO端口复用映射: GPIO->Pin->I2C*/
    GPIO_PinAFConfig(cfg->scl_port, drv_gpio_get_af_src(cfg->scl_pin), drv_gpio_get_i2c_af((uint32_t)cfg->i2c_periph));
    GPIO_PinAFConfig(cfg->sda_port, drv_gpio_get_af_src(cfg->sda_pin), drv_gpio_get_i2c_af((uint32_t)cfg->i2c_periph));


    stGPIOInit.GPIO_Mode = GPIO_Mode_AF;
    stGPIOInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
    stGPIOInit.GPIO_OType = GPIO_OType_OD;  //开漏, 线与, 可主动输出低电平, 默认高阻态
    stGPIOInit.GPIO_Speed = GPIO_Speed_50MHz;

    /*时钟*/
    stGPIOInit.GPIO_Pin = cfg->scl_pin;
    GPIO_Init(cfg->scl_port, &stGPIOInit);

    /*数据*/
    stGPIOInit.GPIO_Pin = cfg->sda_pin;
    GPIO_Init(cfg->sda_port, &stGPIOInit);

    /*I2C配置*/
    stI2CInit.I2C_Mode = I2C_Mode_I2C;
    stI2CInit.I2C_DutyCycle = I2C_DutyCycle_2;  //占空比1:2
    stI2CInit.I2C_Ack = I2C_Ack_Enable;
    stI2CInit.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    stI2CInit.I2C_OwnAddress1 = cfg->dev_addr;
    stI2CInit.I2C_ClockSpeed = cfg->i2c_clock_hz;  //400KHz
    I2C_Init(cfg->i2c_periph, &stI2CInit);
    I2C_Cmd(cfg->i2c_periph, ENABLE);

    I2C_AcknowledgeConfig(cfg->i2c_periph, ENABLE);

    /*设置I2C总线状态为初始化完成*/
    state = DY_BUS_STATE_INITIALIZED;
    bus->ops->control(bus, DY_BUS_CTRL_CMD_SET_STATE, (void *)&state);
    #endif
    printf("========================I2C bus [%s] init end===================\n", bus->name);
    
    return DY_EOK;
}


/**
 * @brief I2C写入数据: 经过测试
 */
static int drv_stm32_i2c_bus_master_write(dy_bus_t *bus, uint32_t addr, uint8_t *buf, uint32_t len)
{
    dy_i2c_bus_t *i2c_bus = (dy_i2c_bus_t *)bus;
    dy_stm32_i2c_bus_config_t *cfg = (dy_stm32_i2c_bus_config_t *)(bus->priv_data);
    uint32_t i2c_timeout = I2C_TIMEOUT;
    uint32_t index = 0;
    int ret = 0;
    /*写数据, 也就是先发送地址+写标志位, 然后再发送数据*/
    /*首先检查总线是否空闲*/
    ret = drv_stm32_i2c_bus_start_addr(bus, addr, 0);
    if (DY_I2C_EOK != ret)
    {
        return ret;
    }
    /*开始发送数据*/
    while(index < len)
    {
        i2c_timeout = I2C_TIMEOUT;
        I2C_SendData(cfg->i2c_periph, buf[index]);
        /* 检测 EV8 事件并清除标志*/
        while (ERROR == I2C_CheckEvent(cfg->i2c_periph, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
        {
            if ((i2c_timeout--) == 0)
            {
                return DY_I2C_EWRITE_TMOUT;
            }
        }
        index++;
    }
    /*发送停止信号*/
    I2C_GenerateSTOP(cfg->i2c_periph, ENABLE);
    return index;
}

/**
 * @brief I2C读取数据
 */
static int drv_stm32_i2c_bus_master_read(dy_bus_t *bus, uint32_t addr, uint8_t *buf, uint32_t len)
{
    dy_i2c_bus_t *i2c_bus = (dy_i2c_bus_t *)bus;
    dy_stm32_i2c_bus_config_t *cfg = (dy_stm32_i2c_bus_config_t *)(bus->priv_data);
    uint32_t i2c_timeout = I2C_TIMEOUT;
    uint32_t index = 0;
    int ret = 0;
    /*读数据, 也就是先发送地址+读标志位, 然后再发送数据*/
    /*首先检查总线是否空闲*/
    ret = drv_stm32_i2c_bus_start_addr(bus, addr, 1);
    if (DY_I2C_EOK != ret)
    {
        return ret;
    }

    /*开始接收数据*/
    for (index = 0; index < len; ++index)
    {
        i2c_timeout = I2C_TIMEOUT;
        /*如果是最后一个字节, 则发送非应答信号*/
        if (index == (len - 1))
        {
            /*发送NAK*/
            I2C_AcknowledgeConfig(cfg->i2c_periph, DISABLE);
        }
        /*等待标志位置位*/
        while (ERROR == I2C_CheckEvent(cfg->i2c_periph, I2C_EVENT_MASTER_BYTE_RECEIVED))
        {
            if ((i2c_timeout--) == 0)
            {
                /*超时了, 没有数据了, 直接退出*/
                I2C_AcknowledgeConfig(cfg->i2c_periph, ENABLE);
                /*发送结束信号*/
                I2C_GenerateSTOP(cfg->i2c_periph, ENABLE);
                return index;
            }
        }
        buf[index] = I2C_ReceiveData(cfg->i2c_periph);

        #if 1
        /*如果是最后一个字节, 则发送非应答信号*/
        if (index == (len - 1))
        {
            /*发送结束信号*/
            I2C_GenerateSTOP(cfg->i2c_periph, ENABLE);
        }
        #endif
    }

    /*使能应答, 方便下一次I2C传输*/
    I2C_AcknowledgeConfig(cfg->i2c_periph, ENABLE);
    return index;
}

/**
 * @brief I2C收发数据
 */
static int drv_stm32_i2c_bus_master_xfer(dy_bus_t *bus, dy_i2c_msg msgs[], uint32_t num)
{
    int ret = 0;
    uint32_t index = 0;
    for (index = 0; index < num; ++index)
    {
        if (msgs[index].flags & DY_I2C_RD)
        {
            ret = drv_stm32_i2c_bus_master_read(bus, msgs[index].addr, msgs[index].buf, msgs[index].len);
            switch (ret)
            {
                case DY_I2C_EBSY_TMOUT:
                case DY_I2C_ESBSEND_TMOUT:
                case DY_I2C_EADDSEND_TMOUT:
                case DY_I2C_EREAD_TMOUT:
                    printf("====================read ret = %d\r\n", ret);
                    return DY_ETMOUT;
                default:
                    break;
            }
        }
        else
        {
            ret = drv_stm32_i2c_bus_master_write(bus, msgs[index].addr, msgs[index].buf, msgs[index].len);
            switch (ret)
            {
                case DY_I2C_EBSY_TMOUT:
                case DY_I2C_ESBSEND_TMOUT:
                case DY_I2C_EADDSEND_TMOUT:
                case DY_I2C_EWRITE_TMOUT:
                    printf("====================write ret = %d\r\n", ret);
                    return DY_ETMOUT;
                default:
                    break;
            }
        }
    }
    return index;
}

/*检测设备*/
static int drv_stm32_i2c_bus_detect_dev(dy_bus_t *bus, dy_i2c_bus_ctl_cmd_detect_dev_t *dev)
{
    dy_stm32_i2c_bus_config_t *cfg = (dy_stm32_i2c_bus_config_t *)(bus->priv_data);
    uint32_t start_time = drv_time_get_sys_tick();
    uint16_t sr1_tmp = 0;
    do
    {
        if (drv_time_get_sys_tick() - start_time > dev->timeout_ms)
        {
            return DY_I2C_EADDSEND_TMOUT;
        }
        I2C_GenerateSTART(cfg->i2c_periph, ENABLE);
        sr1_tmp = I2C_ReadRegister(cfg->i2c_periph, I2C_Register_SR1);
        I2C_Send7bitAddress(cfg->i2c_periph, dev->dev_addr, I2C_Direction_Transmitter);
    }while(!(I2C_ReadRegister(cfg->i2c_periph, I2C_Register_SR1) & 0x0002));
    
    I2C_ClearFlag(cfg->i2c_periph, I2C_FLAG_AF);
    I2C_GenerateSTOP(cfg->i2c_periph, ENABLE);
    return DY_I2C_EOK;
}

static int drv_stm32_i2c_bus_control(dy_bus_t *bus, int cmd, void *arg)
{
    dy_i2c_bus_t *i2c_bus = (dy_i2c_bus_t *)bus;
    dy_stm32_i2c_bus_config_t *cfg = (dy_stm32_i2c_bus_config_t *)(bus->priv_data);
    int ret = 0;
    switch (cmd)
    {
        /*用于自定义值, 从DY_BUS_CTRL_CMD_MAX开始分配*/
        case DY_I2C_CTRL_CMD_RESET:
            break;
        /*发送起始信号*/
        case DY_I2C_CTRL_CMD_DETECT_DEV:
            ret = drv_stm32_i2c_bus_detect_dev(bus, (dy_i2c_bus_ctl_cmd_detect_dev_t*)arg);
            break;
        default:
            ret = bus->ops->control(bus, cmd, arg);
            break;
    }
    switch (ret)
    {
        case DY_I2C_EBSY_TMOUT:
        case DY_I2C_ESBSEND_TMOUT:
        case DY_I2C_EADDSEND_TMOUT:
        case DY_I2C_EREAD_TMOUT:
            printf("===ret = %d\r\n", ret);
            ret = DY_ETMOUT;
            break;
        default:
            ret = DY_EOK;
            break;
    }
    return ret;
}


dy_bus_ops_t i2c_bus_ops = {
    .init = drv_stm32_i2c_bus_init,
    .control = dy_bus_control

};


dy_stm32_i2c_bus_config_t i2c1_impl = 
{
    .i2c_periph = I2C1,
    .periph_clk = RCC_APB1Periph_I2C1,
    .scl_clk = RCC_AHB1Periph_GPIOB,
    .scl_port = GPIOB,
    .scl_pin = GPIO_Pin_8,
    .sda_clk = RCC_AHB1Periph_GPIOB,
    .sda_port = GPIOB,
    .sda_pin = GPIO_Pin_9,
    .ev_irq_type = 0,
    .er_irq_type = 0,
    .i2c_clock_hz = 400 * 1000,
    .dev_addr = 0x0A
};

dy_i2c_bus_t i2c1_stm32_bus = 
{
    .bus = 
    {
        .name = "I2C1",
        .type = DY_BUS_TYPE_I2C,
        .state = DY_BUS_STATE_UNINITIALIZED,
        .ops = &i2c_bus_ops,
        .device_count = 0,
        .device_list = NULL,
        .next = NULL,
        .priv_data = (void *)&i2c1_impl,
    },
    .ops = 
    {
        .master_xfer = drv_stm32_i2c_bus_master_xfer,
        .i2c_bus_control = drv_stm32_i2c_bus_control,
        .slave_xfer = NULL,
    },
    .send_stop = true
};

int drv_i2c_hw_init(void)
{
    /*初始化I2C0总线配置: 这里使用的是全局初始化*/
    /*注册I2C总线到管理器*/
    if (DY_EOK == dy_bus_register((dy_bus_t *)&i2c1_stm32_bus))
    {
        i2c1_stm32_bus.bus.ops->init(&i2c1_stm32_bus.bus);
    }
    else
    {
        return DY_ERROR;
    }
    
    return DY_EOK;
}

