
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "drv_dac.h"



/**********************************************************************
 * 
 *                              数模转换
 * 作用: 输入数字编码, 转换成对应的模拟电压输出.
 * 特点:
 *      位于APB1上, 具有2个通道; ITF是什么; 支持8位/12位模式
 *      存在左右数据对齐方式: 左对齐使用寄存器的高N位; 右对齐使用寄存器的低N位
 *      可以通过一个输入参考电压来提高分辨率; 输出缓冲器可直接驱动负载
 *      触发源
 *          可选择外部触发器TIM2, TIM4-8, EXTI-9; 
 *          软件触发(SW TRIG - 将该位置1, 转换立即触发)
 *      关于DMA下溢出
 *          (1)DMA数据传输停止
 *          (2)清空DAC下溢标志位
 *          (3)去使能DAC的DMA使能位
 *          (4)重新初始化DMA和DAC通道
 *          (5)可选择修改DAC触发频率减轻DMA工作负载
 *      DAC双通道转换
 *          独立触发: 不产生波形; 生成单个 LFSR; 生成不同 LFSR; 生成单个三角波; 生成不同三角波
 *          同步软件启动
 *          同步触发: 不产生波形; 生成单个 LFSR; 生成不同 LFSR; 生成单个三角波; 生成不同三角波
 * 应用
 *      生成噪声: 
 *          生成可变振幅的伪噪声(WAVEx)
 *          振幅: LFSR(线性反馈移位寄存器), LFSR的预加载值是0xAAAA, 可通过MAMP[3：0]位进行屏蔽, 在不发生溢出的情况下加到DAC_DHR数据寄存器的内容
 *      生成三角波: (WAVEx)
 *          振幅: 通过DAC_CR的MAMP[3:0]进行配置
 * 
 ***********************************************************************/



#define DAC_DHR12R1_Address     (uint32_t)(DAC_BASE+0x08)
#define DAC_DHR12R2_Address     (uint32_t)(DAC_BASE+0x14)

#define DAC_DHR12RD_Address     (uint32_t)(DAC_BASE+0x20)

typedef enum
{
    DAC_CH1,
    DAC_CH2,
    DAC_CH1_CH2
}dac_mode_t;

typedef struct 
{
    /*通道ID*/
    uint32_t channel_id;
    /*GPIO*/
    uint32_t gpio_clk;
    GPIO_TypeDef *gpio;
    uint16_t gpio_pin;

    /*输出的DAC形式*/
    uint32_t dac_trigger;                           //转换触发方式
    uint32_t dac_wave_generation;                   //波形
    uint32_t dac_LFSR_unmask_triangle_amplitude;    //波形振幅
    uint32_t dac_output_buffer;                     //是否使能输出缓冲器

    /*DMA使能: 内存到外设*/
    bool dma_enable;
    uint32_t dma_clk;
    uint32_t dma_channel;
    DMA_Stream_TypeDef *dma_stream;
}dac_channel_t;


typedef struct
{
    dac_mode_t mode;
    /*DAC句柄*/
    uint32_t periph_clk;         //使能DAC外设时钟
    /*DAC输出的GPIO*/
    dac_channel_t dac_ch1;
    dac_channel_t dac_ch2;

    /*使能DMA*/
    bool dac_dma_enable;
    /*是否同步: 如果同步, 则二者只需要配置一个DMA数据流即可*/
    bool dac_sync_enable;           //两个通道是否同步: 当mode的值是DMA_CH1_CH2时, 需要考虑
}dac_cfg_t;

typedef struct 
{
    dy_device_t device;
    dac_cfg_t cfg;

    uint16_t dac_value;     //待转换的数值
}dac_dev_t;

dac_dev_t g_dac;

const uint16_t g_Sine12bit[32] = {
    2048  , 2460  , 2856  , 3218  , 3532  , 3786  , 3969  , 4072  ,
    4093  , 4031  , 3887  , 3668  , 3382  , 3042  , 2661  , 2255  ,
    1841  , 1435  , 1054  , 714   , 428   , 209   , 65    , 3     ,
    24    , 127   , 310   , 564   , 878   , 1240  , 1636  , 2048

};

uint32_t g_DualSine12bit[32] = {
    ((2048 << 16) | (2048)), ((2460 << 16) | (2460)), ((2856 << 16) | (2856)), ((3218 << 16) | (3218)),
    ((3532 << 16) | (3532)), ((3786 << 16) | (3786)), ((3969 << 16) | (3969)), ((4072 << 16) | (4072)),
    ((4093 << 16) | (4093)), ((4031 << 16) | (4031)), ((3887 << 16) | (3887)), ((3668 << 16) | (3668)),
    ((3382 << 16) | (3382)), ((3042 << 16) | (3042)), ((2661 << 16) | (2661)), ((2255 << 16) | (2255)),
    ((1841 << 16) | (1841)), ((1435 << 16) | (1435)), ((1054 << 16) | (1054)), ((714 << 16) | (714)),
    ((428 << 16) | (428)), ((209 << 16) | (209)), ((65 << 16) | (65)), ((3 << 16) | (3)),
    ((24 << 16) | (24)), ((127 << 16) | (127)), ((310 << 16) | (310)), ((564 << 16) | (564)),
    ((878 << 16) | (878)), ((1240 << 16) | (1240)), ((1636 << 16) | (1636)), ((2048 << 16) | (2048)),
};


static int dac_dma_init(dy_device_t *dev)
{
    dac_dev_t *dac = (dac_dev_t *)dev;

    /* DAC1使用DMA1 通道7 数据流5 */
    RCC_AHB1PeriphClockCmd(dac->cfg.dac_ch1.dma_clk, ENABLE);

    /*配置DMA*/
    DMA_InitTypeDef  stDMAInit;
    stDMAInit.DMA_DIR = DMA_DIR_MemoryToPeripheral;             //数据传输方向内存至外设
    stDMAInit.DMA_PeripheralInc = DMA_PeripheralInc_Disable;    //外设数据地址固定
    stDMAInit.DMA_MemoryInc = DMA_MemoryInc_Enable;             //内存数据地址自增
    stDMAInit.DMA_Mode = DMA_Mode_Circular;                         //循环模式
    stDMAInit.DMA_Priority = DMA_Priority_High;                     //高DMA通道优先级
    /*FIFO*/
    stDMAInit.DMA_FIFOMode = DMA_FIFOMode_Disable;
    stDMAInit.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    /*突发模式*/
    stDMAInit.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    stDMAInit.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    if (dac->cfg.dac_ch1.dma_enable)
    {
        /*使能DAC通道1的DMA*/
        DAC_DMACmd(dac->cfg.dac_ch1.channel_id, ENABLE);

        /* 配置DMA2 */
        stDMAInit.DMA_Channel = dac->cfg.dac_ch1.dma_channel;
        if (dac->cfg.dac_sync_enable)
        {
            /*同步则使用RD*/
            stDMAInit.DMA_PeripheralBaseAddr = DAC_DHR12RD_Address;     //外设数据地址
            stDMAInit.DMA_BufferSize = 32;                              //缓存大小为32字节
            stDMAInit.DMA_Memory0BaseAddr = (uint32_t)g_DualSine12bit;
            /*外设数据*/
            stDMAInit.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
            stDMAInit.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
        }
        else
        {
            /*不是同步则使用R1*/
            stDMAInit.DMA_PeripheralBaseAddr = DAC_DHR12R1_Address;     //外设数据地址
            stDMAInit.DMA_BufferSize = 16;                              //缓存大小为16字节
            stDMAInit.DMA_Memory0BaseAddr = (uint32_t)g_Sine12bit ;
            stDMAInit.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
            stDMAInit.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
        }
        DMA_Init(dac->cfg.dac_ch1.dma_stream, &stDMAInit);
        /* 使能 DMA_Stream */
        DMA_Cmd(dac->cfg.dac_ch1.dma_stream, ENABLE);
    }
    /*如果两个通道是同步的, 这意味着两个通道的触发源是一样的: 不一样不能同步*/
    if (false == dac->cfg.dac_sync_enable)
    {
        if (dac->cfg.dac_ch2.dma_enable)
        {
            /*使能DAC通道2的DMA*/
            DAC_DMACmd(dac->cfg.dac_ch2.channel_id, ENABLE);

            stDMAInit.DMA_Channel = dac->cfg.dac_ch2.dma_channel;
            /*不是同步则使用R1*/
            stDMAInit.DMA_PeripheralBaseAddr = DAC_DHR12R2_Address;     //外设数据地址
            stDMAInit.DMA_BufferSize = 16;                              //缓存大小为16字节
            stDMAInit.DMA_Memory0BaseAddr = (uint32_t)g_Sine12bit ;
            stDMAInit.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
            stDMAInit.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
            DMA_Init(dac->cfg.dac_ch2.dma_stream, &stDMAInit);
            /* 使能 DMA_Stream */
            DMA_Cmd(dac->cfg.dac_ch2.dma_stream, ENABLE);
        }
    }
    return DY_EOK;
}


static int dac_init(dy_device_t *dev)
{
    dac_dev_t *dac = (dac_dev_t *)dev;

    GPIO_InitTypeDef stGPIOInit;
    DAC_InitTypeDef  stDACInit;


    /* DAC的GPIO配置，模拟功能 */
    stGPIOInit.GPIO_Mode = GPIO_Mode_AIN;
    stGPIOInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
    stGPIOInit.GPIO_Speed = GPIO_Speed_100MHz;
    stGPIOInit.GPIO_OType = GPIO_OType_PP;
    /*DAC工作模式, 使能GPIO*/
    switch(dac->cfg.mode)
    {
        case DAC_CH1:
            RCC_AHB1PeriphClockCmd(dac->cfg.dac_ch1.gpio_clk, ENABLE);
            stGPIOInit.GPIO_Pin =  dac->cfg.dac_ch1.gpio_pin;
            GPIO_Init(dac->cfg.dac_ch1.gpio, &stGPIOInit);
            break;
        case DAC_CH2:
            RCC_AHB1PeriphClockCmd(dac->cfg.dac_ch2.gpio_clk, ENABLE);
            stGPIOInit.GPIO_Pin =  dac->cfg.dac_ch2.gpio_pin;
            GPIO_Init(dac->cfg.dac_ch2.gpio, &stGPIOInit);
            break;
        case DAC_CH1_CH2:
            RCC_AHB1PeriphClockCmd(dac->cfg.dac_ch1.gpio_clk, ENABLE);
            RCC_AHB1PeriphClockCmd(dac->cfg.dac_ch2.gpio_clk, ENABLE);

            stGPIOInit.GPIO_Pin =  dac->cfg.dac_ch1.gpio_pin;
            GPIO_Init(dac->cfg.dac_ch1.gpio, &stGPIOInit);

            stGPIOInit.GPIO_Pin =  dac->cfg.dac_ch2.gpio_pin;
            GPIO_Init(dac->cfg.dac_ch2.gpio, &stGPIOInit);
            break;
        default:
            return DY_EINVAL;
    }

    
    /*使用DAC时钟*/
    RCC_APB1PeriphClockCmd(dac->cfg.periph_clk, ENABLE);
    switch(dac->cfg.mode)
    {
        case DAC_CH1:
            stDACInit.DAC_Trigger = dac->cfg.dac_ch1.dac_trigger;                       //使用TIM2作为触发源
            stDACInit.DAC_WaveGeneration = dac->cfg.dac_ch1.dac_wave_generation;        //不使用波形发生器
            stDACInit.DAC_OutputBuffer = dac->cfg.dac_ch1.dac_output_buffer;            //不使用DAC输出缓冲
            stDACInit.DAC_LFSRUnmask_TriangleAmplitude = dac->cfg.dac_ch1.dac_LFSR_unmask_triangle_amplitude;
            DAC_Init(dac->cfg.dac_ch1.channel_id, &stDACInit);
            DAC_Cmd(dac->cfg.dac_ch1.channel_id, ENABLE);
            break;
        case DAC_CH2:
            stDACInit.DAC_Trigger = dac->cfg.dac_ch2.dac_trigger;                       //使用TIM2作为触发源
            stDACInit.DAC_WaveGeneration = dac->cfg.dac_ch2.dac_wave_generation;        //不使用波形发生器
            stDACInit.DAC_OutputBuffer = dac->cfg.dac_ch2.dac_output_buffer;            //不使用DAC输出缓冲
            stDACInit.DAC_LFSRUnmask_TriangleAmplitude = dac->cfg.dac_ch2.dac_LFSR_unmask_triangle_amplitude;
            DAC_Init(dac->cfg.dac_ch2.channel_id, &stDACInit);
            DAC_Cmd(dac->cfg.dac_ch2.channel_id, ENABLE);
            break;
        case DAC_CH1_CH2:
            stDACInit.DAC_Trigger = dac->cfg.dac_ch1.dac_trigger;                       //使用TIM2作为触发源
            stDACInit.DAC_WaveGeneration = dac->cfg.dac_ch1.dac_wave_generation;        //不使用波形发生器
            stDACInit.DAC_OutputBuffer = dac->cfg.dac_ch1.dac_output_buffer;            //不使用DAC输出缓冲
            stDACInit.DAC_LFSRUnmask_TriangleAmplitude = dac->cfg.dac_ch1.dac_LFSR_unmask_triangle_amplitude;
            DAC_Init(dac->cfg.dac_ch1.channel_id, &stDACInit);

            stDACInit.DAC_Trigger = dac->cfg.dac_ch2.dac_trigger;                       //使用TIM2作为触发源
            stDACInit.DAC_WaveGeneration = dac->cfg.dac_ch2.dac_wave_generation;        //不使用波形发生器
            stDACInit.DAC_OutputBuffer = dac->cfg.dac_ch2.dac_output_buffer;            //不使用DAC输出缓冲
            stDACInit.DAC_LFSRUnmask_TriangleAmplitude = dac->cfg.dac_ch2.dac_LFSR_unmask_triangle_amplitude;
            DAC_Init(dac->cfg.dac_ch2.channel_id, &stDACInit);

            DAC_Cmd(dac->cfg.dac_ch1.channel_id, ENABLE);
            DAC_Cmd(dac->cfg.dac_ch2.channel_id, ENABLE);
            break;
        default:
            return DY_EINVAL;
    }

    if (dac->cfg.dac_dma_enable)
    {
        dac_dma_init(dev);
    }

    return DY_EOK;
}


static int dac_control(dy_device_t *dev, int cmd, void *arg)
{
    int ret = DY_EOK;
    dac_dev_t *dac = (dac_dev_t *)dev;
    switch(cmd)
    {
        case DAC_CH1_START:
            DAC_Cmd(dac->cfg.dac_ch1.channel_id, ENABLE);
            if (dac->cfg.dac_ch1.dma_enable)
            {
                DMA_Cmd(dac->cfg.dac_ch2.dma_stream, ENABLE);
            }
            break;
        case DAC_CH1_STOP:
            if (dac->cfg.dac_ch1.dma_enable)
            {
                DMA_Cmd(dac->cfg.dac_ch2.dma_stream, DISABLE);
            }
            DAC_Cmd(dac->cfg.dac_ch1.channel_id, DISABLE);
            break;
        case DAC_CH2_START:
            if (dac->cfg.dac_ch2.dma_enable)
            {
                DMA_Cmd(dac->cfg.dac_ch2.dma_stream, DISABLE);
            }
            DAC_Cmd(dac->cfg.dac_ch2.channel_id, ENABLE);
            break;
        case DAC_CH2_STOP:
            if (dac->cfg.dac_ch2.dma_enable)
            {
                DMA_Cmd(dac->cfg.dac_ch2.dma_stream, DISABLE);
            }
            DAC_Cmd(dac->cfg.dac_ch2.channel_id, DISABLE);
            break;
        case DAC_START:
            DAC_Cmd(dac->cfg.dac_ch1.channel_id, ENABLE);
            DAC_Cmd(dac->cfg.dac_ch2.channel_id, ENABLE);
            if (dac->cfg.dac_dma_enable)
            {
                DMA_Cmd(dac->cfg.dac_ch1.dma_stream, ENABLE);
                if (false == dac->cfg.dac_sync_enable)
                {
                    DMA_Cmd(dac->cfg.dac_ch2.dma_stream, ENABLE);
                }
            }
            break;
        case DAC_STOP:
            if (dac->cfg.dac_dma_enable)
            {
                DMA_Cmd(dac->cfg.dac_ch1.dma_stream, DISABLE);
                DMA_Cmd(dac->cfg.dac_ch2.dma_stream, DISABLE);
            }
            DAC_Cmd(dac->cfg.dac_ch1.channel_id, DISABLE);
            DAC_Cmd(dac->cfg.dac_ch2.channel_id, DISABLE);
            break;
        default:
            ret = dy_device_control(dev, cmd, arg);
            break;
    }
    return ret;
}

device_ops_t g_dac_opt = {
    .init = dac_init,
    .control = dac_control
};

dac_channel_t dac_ch1 = {
    .channel_id = DAC_Channel_1,
    .gpio_clk = RCC_AHB1Periph_GPIOA,
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_4,
    .dac_trigger = DAC_Trigger_T2_TRGO,
    .dac_wave_generation = DAC_WaveGeneration_None,
    .dac_LFSR_unmask_triangle_amplitude = DAC_TriangleAmplitude_4095, //0xAAAA, 
    .dac_output_buffer = DAC_OutputBuffer_Disable,      // 目前不需要驱动外部负载, 这里不使能
    .dma_enable = true,
    .dma_clk = RCC_AHB1Periph_DMA1,
    .dma_channel = DMA_Channel_7,
    .dma_stream = DMA1_Stream5,
};

dac_channel_t dac_ch2 = {
    .channel_id = DAC_Channel_2,
    .gpio_clk = RCC_AHB1Periph_GPIOA,
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_5,
    .dac_trigger = DAC_Trigger_T2_TRGO,
    .dac_wave_generation = DAC_WaveGeneration_None,
    .dac_LFSR_unmask_triangle_amplitude = DAC_TriangleAmplitude_4095, //0xAAAA, 
    .dac_output_buffer = DAC_OutputBuffer_Disable,      // 目前不需要驱动外部负载, 这里不使能
    .dma_enable = true,
    .dma_clk = RCC_AHB1Periph_DMA1,
    .dma_channel = DMA_Channel_7,
    .dma_stream = DMA1_Stream6,
};

dac_cfg_t g_cfg_ch1 = {
    .mode = DAC_CH1,
    .dac_dma_enable = true,
    .dac_sync_enable = false
};

dac_cfg_t g_cfg_ch2 = {
    .mode = DAC_CH2,
    .dac_dma_enable = true,
    .dac_sync_enable = false
};

dac_cfg_t g_cfg_ch1_ch2_async = {
    .mode = DAC_CH2,
    .dac_dma_enable = true,
    .dac_sync_enable = false
};

dac_cfg_t g_cfg_ch1_ch2_sync = {
    .mode = DAC_CH2,
    .dac_dma_enable = true,
    .dac_sync_enable = true
};


int drv_dac_init()
{
    int ret = DY_EOK;
    /*DAC: 位数, 通道, 寄存器(单, 双), 触发源(定时器, 外部中断, 软件), DMA*/
    strncpy(g_dac.device.name, "dac", sizeof(g_dac.device.name) - 1);
    g_dac.device.ops = &g_dac_opt;

    g_dac.cfg.mode = DAC_CH1;
    g_dac.cfg.dac_dma_enable = true;
    g_dac.cfg.dac_sync_enable = false;

    g_dac.cfg.dac_ch1.channel_id = DAC_Channel_1;
    g_dac.cfg.dac_ch1.gpio_clk = RCC_AHB1Periph_GPIOA;
    g_dac.cfg.dac_ch1.gpio = GPIOA;
    g_dac.cfg.dac_ch1.gpio_pin = GPIO_Pin_4;
    g_dac.cfg.dac_ch1.dac_trigger = DAC_Trigger_T2_TRGO;
    g_dac.cfg.dac_ch1.dac_wave_generation = DAC_WaveGeneration_None;
    g_dac.cfg.dac_ch1.dac_LFSR_unmask_triangle_amplitude = DAC_TriangleAmplitude_4095; //0xAAAA, 
    g_dac.cfg.dac_ch1.dac_output_buffer = DAC_OutputBuffer_Disable;      // 目前不需要驱动外部负载, 这里不使能
    g_dac.cfg.dac_ch1.dma_enable = true;
    g_dac.cfg.dac_ch1.dma_clk = RCC_AHB1Periph_DMA1;
    g_dac.cfg.dac_ch1.dma_channel = DMA_Channel_7;
    g_dac.cfg.dac_ch1.dma_stream = DMA1_Stream5;

    if (DY_EOK != dy_device_register(g_dac.device.name, &g_dac.device))
    {
        return DY_ERROR;
    }
    
    ret = g_dac.device.ops->init(&g_dac.device);

    return ret;
}
