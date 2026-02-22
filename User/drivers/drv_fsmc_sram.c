

#include "drv_fsmc.h"

#include "drv_fsmc_sram.h"

/******************************************************
 * SRAM: IS62WV51216
 *      1、特性
 *          (1)地址宽度19位, 数据宽度16位, 容量大小1MB
 *          (2)地址输入: A0-A18. 行地址, 列地址(该型号无列地址)
 *          (3)数据: IO0-IO15
 *          (4)片选信号: CS
 *          (5)输出使能信号: OE, 低电平有效
 *          (6)写使能: WE, 低电平有效
 *          (7)UB: 数据掩码, Upper Byte, 高字节允许访问, 低电平有效. 访问宽度16位数据时, UB, LB都拉低; 
 *          (8)LB: 数据掩码, Lower Byte, 低字节允许访问, 低电平有效
 *          (9)利用片选信号可以把多个SRAM组成一个大容量的内存条
 * 
 *      2、读取时序
 *          (1)发送地址信号
 *          (2)读使能   
 *          (3)片选信号
 *          (4)掩码信号
 *          (5)数据信号
 * 
 *      3、写时序
 *          (1)发送地址信号
 *          (2)写使能   
 *          (3)片选信号
 *          (4)掩码信号
 *          (5)数据信号
 * 
 *      4、时间参数
 *          (1)读取操作总时间: 不小于55ns
 *          (2)从接收到地址到给出有效数据的时间: 不大于55ns
 *          (3)接收到读使能到给出有效数据的时间: 不大于25ns
 *          (4)写操作总时间: 不小于55ns
 *          (5)发送地址信号到给出写使能信号的时间: 大于0ns(也就是先发地址, 再发写使能)
 *          (6)接收到写使能信号到数据采样时间: 不小于40ns
 * 
 *******************************************************/

 /********************************************************
  * 
  *         先直接把FSMC与SRAM绑定死, 不做过多的设计
  * FSMC_A[25:0] - A0 - A18
  * FSMC_D[15:0], FSMC_D[31:16] - IO0 - I015
  * CS(片选信号)
  * OE(输出使能, 低电平有效)
  * WE(写入使能, 低电平有效)
  * UB(高字节允许访问)
  * LB(低字节允许访问)
  * 
  * ======================== 地址映射 =============================
  * FSMC_A0 -> PF0
  * FSMC_A1 -> PF1
  * FSMC_A2 -> PF2
  * FSMC_A3 -> PF3
  * FSMC_A4 -> PF4
  * FSMC_A5 -> PF5
  * FSMC_A6 -> PF12
  * FSMC_A7 -> PF13
  * FSMC_A8 -> PF14
  * FSMC_A9 -> PF15
  * FSMC_A10 -> PG0
  * FSMC_A11 -> PG1
  * FSMC_A12 -> PG2
  * FSMC_A13 -> PG3
  * FSMC_A14 -> PG4
  * FSMC_A15 -> PG5
  * FSMC_A16 -> PD11
  * FSMC_A17 -> PD12
  * FSMC_A18 -> PD13
  * FSMC_A19 -> PE3
  * FSMC_A20 -> PE4
  * FSMC_A21 -> PE5
  * FSMC_A22 -> PE6
  * FSMC_A23 -> PE2
  * FSMC_A24 -> PG13
  * FSMC_A25 -> PG14
  * ========================== 数据映射 =============================
  * FSMC_D0 -> PD14
  * FSMC_D1 -> PD15
  * FSMC_D2 -> PD0
  * FSMC_D3 -> PD1
  * FSMC_D4 -> PE7
  * FSMC_D5 -> PE8
  * FSMC_D6 -> PE9
  * FSMC_D7 -> PE10
  * FSMC_D8 -> PE11
  * FSMC_D9 -> PE12
  * FSMC_D10 -> PE13
  * FSMC_D11 -> PE14
  * FSMC_D12 -> PE15
  * FSMC_D13 -> PD8
  * FSMC_D14 -> PD9
  * FSMC_D15 -> PD10
  * ========================== 公共控制信号 =============================
  * FSMC_NOE -> PD4
  * FSMC_NWE -> PD5
  * FSMC_NWAIT -> PD6
  * FSMC_NE4 -> PG12
  * FSMC_NADV -> PB7
  * ========================== 数据掩码 ==============================
  * FSMC_NBL0 -> PE0
  * FSMC_NBL1 -> PE1
  * ========================== 其他信号 ================================
  * FSMC_NIORD -> PF6
  * FSMC_NREG -> PF7
  * FSMC_NIOWR -> PF8
  * FSMC_CD -> PF9 
  * FSMC_INTR -> PF10 
  * FSMC_CLE -> PD11
  * FSMC_ALE -> PD12
  * FSMC_INT2 -> PG6
  * FSMC_INT3 -> PG7
  * FSMC_CLK -> PD3
  * FSMC_NCE4_2 -> PG11
  * FSMC_NL -> PB7
  *********************************************************/
/*数据线*/
typedef struct
{
    /*GPIO端口*/
    GPIO_TypeDef *gpio_d;
    /*GPIO引脚*/
    uint16_t gpio_dpin;
    /*GPIO复用关系*/
    //uint8_t gpio_daf;
}fsmc_DLine_t;

/*地址线*/
typedef struct
{
    /*GPIO端口*/
    GPIO_TypeDef *gpio_a;
    /*GPIO引脚*/
    uint16_t gpio_apin;
    /*GPIO复用关系*/
    //uint8_t gpio_aaf;
}fsmc_ALine_t;

/*控制线*/
typedef struct
{
    GPIO_TypeDef *gpio_c;
    uint16_t gpio_cpin;
    //uint8_t gpio_caf;
}fsmc_CLine_t;

/*****************************
 * 对于FSMC, 它的端口映射都是固定的
 * 所有它的端口只需要初始化一次即可
 * 
 ******************************/
typedef struct {
    /*FSMC本身的句柄, 时钟*/
    uint32_t periph_clk;
    /*数据线*/
    fsmc_DLine_t dports[16];
    /*地址线*/
    fsmc_ALine_t aports[19];
    /*控制线*/
    fsmc_CLine_t cs;
    fsmc_CLine_t we;
    fsmc_CLine_t oe;
    fsmc_CLine_t ub;
    fsmc_CLine_t lb;
}fsmc_sram_hw_cfg_t;

/* FSMC SRAM设备配置结构体 */
typedef struct
{
    uint8_t addr_setup_time;
    uint8_t addr_hold_time;
    uint8_t data_setup_time;
    /*数据地址复用状态下, 需要配置*/
    uint8_t bus_turn_around_duration;
    /*同步类型NOR存储器*/
    uint8_t clk_division;
    uint8_t data_latency;

    uint8_t access_mode;
}fsmc_sram_timing_cfg_t;

typedef struct
{
    uint8_t bank_num;       //块号
    uint8_t mem_type;       //存储器类型
    uint16_t data_with;     //数据宽度 
    /*读写两种配置*/
    fsmc_sram_timing_cfg_t rd_cfg;
    fsmc_sram_timing_cfg_t we_cfg;
}fsmc_sram_pg_cfg_t;

typedef struct
{
    fsmc_sram_hw_cfg_t hw_cfg;
    fsmc_sram_pg_cfg_t pg_cfg;  //编程配置
}fsmc_sram_cfg_t;

typedef struct
{
    dy_device_t device;              // 基础设备
    fsmc_sram_cfg_t cfg;
}fsmc_sram_dev_t;

fsmc_sram_dev_t g_fsmc_sram;

static int fsmc_sram_init(dy_device_t *dev)
{
    fsmc_sram_dev_t *fsmc_sram = (fsmc_sram_dev_t*)dev;
    /******************************************************************
     * 
     *                      FSMC端口配置
     * 
     ******************************************************************/
    GPIO_InitTypeDef  stGPIOInit;
    uint8_t i = 0;

    stGPIOInit.GPIO_OType = GPIO_OType_PP;
    stGPIOInit.GPIO_PuPd = GPIO_PuPd_UP;
    stGPIOInit.GPIO_Mode = GPIO_Mode_AF;
    stGPIOInit.GPIO_Speed = GPIO_Speed_100MHz;

    /*****************************************
     *              数据端口初始化
     *****************************************/
    /*数据初始化*/
    for (i = 0; i < 16; ++i)
    {
        /*时钟*/
        drv_gpio_enable_clk(fsmc_sram->cfg.hw_cfg.dports[i].gpio_d);
        /*初始化*/
        stGPIOInit.GPIO_Pin = fsmc_sram->cfg.hw_cfg.dports[i].gpio_dpin;
        GPIO_Init(fsmc_sram->cfg.hw_cfg.dports[i].gpio_d, &stGPIOInit);
        /*复用*/
        GPIO_PinAFConfig(fsmc_sram->cfg.hw_cfg.dports[i].gpio_d, 
            drv_gpio_get_af_src(fsmc_sram->cfg.hw_cfg.dports[i].gpio_dpin), GPIO_AF_FSMC);
    }

    /*****************************************
     *              地址端口初始化
     *****************************************/
    /*地址初始化*/
    for (i = 0; i < 19; ++i)
    {
        /*时钟*/
        drv_gpio_enable_clk(fsmc_sram->cfg.hw_cfg.aports[i].gpio_a);
        /*初始化*/
        stGPIOInit.GPIO_Pin = fsmc_sram->cfg.hw_cfg.aports[i].gpio_apin;
        GPIO_Init(fsmc_sram->cfg.hw_cfg.aports[i].gpio_a, &stGPIOInit);
        /*复用*/
        GPIO_PinAFConfig(fsmc_sram->cfg.hw_cfg.aports[i].gpio_a,
            drv_gpio_get_af_src(fsmc_sram->cfg.hw_cfg.aports[i].gpio_apin), GPIO_AF_FSMC);
    }

    /*****************************************
     *          控制线路初始化 - 片选
     *****************************************/
    /*片选*/
    drv_gpio_enable_clk(fsmc_sram->cfg.hw_cfg.cs.gpio_c);
    /*初始化*/
    stGPIOInit.GPIO_Pin = fsmc_sram->cfg.hw_cfg.cs.gpio_cpin;
    GPIO_Init(fsmc_sram->cfg.hw_cfg.cs.gpio_c, &stGPIOInit);
    /*复用*/
    GPIO_PinAFConfig(fsmc_sram->cfg.hw_cfg.cs.gpio_c,
        drv_gpio_get_af_src(fsmc_sram->cfg.hw_cfg.cs.gpio_cpin), GPIO_AF_FSMC);

    /*****************************************
     *          控制线路初始化 - 写入使能
     *****************************************/
    drv_gpio_enable_clk(fsmc_sram->cfg.hw_cfg.we.gpio_c);
    stGPIOInit.GPIO_Pin = fsmc_sram->cfg.hw_cfg.we.gpio_cpin;
    GPIO_Init(fsmc_sram->cfg.hw_cfg.we.gpio_c, &stGPIOInit);
    /*复用*/
    GPIO_PinAFConfig(fsmc_sram->cfg.hw_cfg.we.gpio_c,
        drv_gpio_get_af_src(fsmc_sram->cfg.hw_cfg.we.gpio_cpin), GPIO_AF_FSMC);
    
    /*****************************************
     *          控制线路初始化 - 输出使能
     *****************************************/
    drv_gpio_enable_clk(fsmc_sram->cfg.hw_cfg.oe.gpio_c);
    stGPIOInit.GPIO_Pin = fsmc_sram->cfg.hw_cfg.oe.gpio_cpin;
    GPIO_Init(fsmc_sram->cfg.hw_cfg.oe.gpio_c, &stGPIOInit);
    /*复用*/
    GPIO_PinAFConfig(fsmc_sram->cfg.hw_cfg.oe.gpio_c,
        drv_gpio_get_af_src(fsmc_sram->cfg.hw_cfg.oe.gpio_cpin), GPIO_AF_FSMC);
    
    /*****************************************
     *          控制线路初始化 - 高字节
     *****************************************/
    drv_gpio_enable_clk(fsmc_sram->cfg.hw_cfg.ub.gpio_c);
    stGPIOInit.GPIO_Pin = fsmc_sram->cfg.hw_cfg.ub.gpio_cpin;
    GPIO_Init(fsmc_sram->cfg.hw_cfg.ub.gpio_c, &stGPIOInit);
    /*复用*/
    GPIO_PinAFConfig(fsmc_sram->cfg.hw_cfg.ub.gpio_c,
        drv_gpio_get_af_src(fsmc_sram->cfg.hw_cfg.ub.gpio_cpin), GPIO_AF_FSMC);

    /*****************************************
     *          控制线路初始化 - 低字节
     *****************************************/
    drv_gpio_enable_clk(fsmc_sram->cfg.hw_cfg.lb.gpio_c);
    stGPIOInit.GPIO_Pin = fsmc_sram->cfg.hw_cfg.lb.gpio_cpin;
    GPIO_Init(fsmc_sram->cfg.hw_cfg.lb.gpio_c, &stGPIOInit);
    /*复用*/
    GPIO_PinAFConfig(fsmc_sram->cfg.hw_cfg.lb.gpio_c,
        drv_gpio_get_af_src(fsmc_sram->cfg.hw_cfg.lb.gpio_cpin), GPIO_AF_FSMC);

    
    /******************************************************************
     * 
     *                      FSMC基础配置
     * 
     ******************************************************************/
    /*使能时钟*/
    RCC_AHB3PeriphClockCmd(fsmc_sram->cfg.hw_cfg.periph_clk, ENABLE);
    /*初始化: 使用什么控制器, 使用哪个区域, 数据宽度, 传输模式*/
    FSMC_NORSRAMInitTypeDef  stSramInit;

    /*建立时间*/
	FSMC_NORSRAMTimingInitTypeDef  stWRTimingInit;
	stWRTimingInit.FSMC_AddressSetupTime = 0x00;        //地址建立时间
	stWRTimingInit.FSMC_AddressHoldTime = 0x00;	        //地址保持时间
	stWRTimingInit.FSMC_DataSetupTime = 0x08;		    //数据保持时间：（DATAST）+ 1个HCLK = 9/168M=54ns(对EM的SRAM芯片)	
	stWRTimingInit.FSMC_BusTurnAroundDuration = 0x00;   //设置总线转换周期，仅用于复用模式的NOR操作
	stWRTimingInit.FSMC_CLKDivision = 0x00;	            //设置时钟分频，仅用于同步类型的存储器
	stWRTimingInit.FSMC_DataLatency = 0x00;		        //数据保持时间，仅用于同步型的NOR
	stWRTimingInit.FSMC_AccessMode = FSMC_AccessMode_A;	//选择匹配SRAM的模式
    /*初始化*/
	stSramInit.FSMC_Bank = FSMC_Bank1_NORSRAM4;                         // 选择FSMC映射的存储区域： Bank1 sram4
	stSramInit.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;       //设置地址总线与数据总线是否复用，仅用于NOR
	stSramInit.FSMC_MemoryType = FSMC_MemoryType_SRAM;                   //设置要控制的存储器类型：SRAM类型
	stSramInit.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;         //存储器数据宽度：16位
	stSramInit.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;      //设置是否使用突发访问模式，仅用于同步类型的存储器
	stSramInit.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;     //设置是否使能等待信号，仅用于同步类型的存储器
	stSramInit.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;   //设置等待信号的有效极性，仅用于同步类型的存储器
	stSramInit.FSMC_WrapMode = FSMC_WrapMode_Disable;                   //设置是否支持把非对齐的突发操作，仅用于同步类型的存储器
	stSramInit.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;   //设置等待信号插入的时间，仅用于同步类型的存储器
	stSramInit.FSMC_WriteOperation = FSMC_WriteOperation_Enable;        //存储器写使能 
	stSramInit.FSMC_WaitSignal = FSMC_WaitSignal_Disable;  		        //不使用等待信号
	stSramInit.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;           // 不使用扩展模式，读写使用相同的时序
	stSramInit.FSMC_WriteBurst = FSMC_WriteBurst_Disable;               //突发写操作
	stSramInit.FSMC_ReadWriteTimingStruct = &stWRTimingInit;            //读写时序配置
	stSramInit.FSMC_WriteTimingStruct = &stWRTimingInit;                //读写同样时序，使用扩展模式时这个配置才有效

    FSMC_NORSRAMInit(&stSramInit);                  //初始化FSMC配置
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE);   //使能BANK

    printf("=======================fsmc sram init\r\n");
    return DY_EOK;
}

static int fsmc_sram_control(dy_device_t *dev, int cmd, void *arg)
{
    
    return DY_EOK;
}

device_ops_t g_fsmc_sram_ops = {
    .init = fsmc_sram_init,
    .control = fsmc_sram_control
};

static int drv_fsmc_sram_port_init()
{
    /*硬件配置初始化*/
    g_fsmc_sram.cfg.hw_cfg.periph_clk = RCC_AHB3Periph_FSMC;
    /* 数据端口初始化 (16位数据线) */
    g_fsmc_sram.cfg.hw_cfg.dports[0].gpio_d = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.dports[0].gpio_dpin = GPIO_Pin_14;  // FSMC_D0
    
    g_fsmc_sram.cfg.hw_cfg.dports[1].gpio_d = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.dports[1].gpio_dpin = GPIO_Pin_15;  // FSMC_D1
    
    g_fsmc_sram.cfg.hw_cfg.dports[2].gpio_d = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.dports[2].gpio_dpin = GPIO_Pin_0;   // FSMC_D2
    
    g_fsmc_sram.cfg.hw_cfg.dports[3].gpio_d = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.dports[3].gpio_dpin = GPIO_Pin_1;   // FSMC_D3
    
    g_fsmc_sram.cfg.hw_cfg.dports[4].gpio_d = GPIOE;
    g_fsmc_sram.cfg.hw_cfg.dports[4].gpio_dpin = GPIO_Pin_7;   // FSMC_D4
    
    g_fsmc_sram.cfg.hw_cfg.dports[5].gpio_d = GPIOE;
    g_fsmc_sram.cfg.hw_cfg.dports[5].gpio_dpin = GPIO_Pin_8;   // FSMC_D5
    
    g_fsmc_sram.cfg.hw_cfg.dports[6].gpio_d = GPIOE;
    g_fsmc_sram.cfg.hw_cfg.dports[6].gpio_dpin = GPIO_Pin_9;   // FSMC_D6
    
    g_fsmc_sram.cfg.hw_cfg.dports[7].gpio_d = GPIOE;
    g_fsmc_sram.cfg.hw_cfg.dports[7].gpio_dpin = GPIO_Pin_10;  // FSMC_D7
    
    g_fsmc_sram.cfg.hw_cfg.dports[8].gpio_d = GPIOE;
    g_fsmc_sram.cfg.hw_cfg.dports[8].gpio_dpin = GPIO_Pin_11;  // FSMC_D8
    
    g_fsmc_sram.cfg.hw_cfg.dports[9].gpio_d = GPIOE;
    g_fsmc_sram.cfg.hw_cfg.dports[9].gpio_dpin = GPIO_Pin_12;  // FSMC_D9
    
    g_fsmc_sram.cfg.hw_cfg.dports[10].gpio_d = GPIOE;
    g_fsmc_sram.cfg.hw_cfg.dports[10].gpio_dpin = GPIO_Pin_13; // FSMC_D10
    
    g_fsmc_sram.cfg.hw_cfg.dports[11].gpio_d = GPIOE;
    g_fsmc_sram.cfg.hw_cfg.dports[11].gpio_dpin = GPIO_Pin_14; // FSMC_D11
    
    g_fsmc_sram.cfg.hw_cfg.dports[12].gpio_d = GPIOE;
    g_fsmc_sram.cfg.hw_cfg.dports[12].gpio_dpin = GPIO_Pin_15; // FSMC_D12
    
    g_fsmc_sram.cfg.hw_cfg.dports[13].gpio_d = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.dports[13].gpio_dpin = GPIO_Pin_8;  // FSMC_D13
    
    g_fsmc_sram.cfg.hw_cfg.dports[14].gpio_d = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.dports[14].gpio_dpin = GPIO_Pin_9;  // FSMC_D14
    
    g_fsmc_sram.cfg.hw_cfg.dports[15].gpio_d = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.dports[15].gpio_dpin = GPIO_Pin_10; // FSMC_D15
    
    /* 地址端口初始化 (19位地址线) */
    g_fsmc_sram.cfg.hw_cfg.aports[0].gpio_a = GPIOF;
    g_fsmc_sram.cfg.hw_cfg.aports[0].gpio_apin = GPIO_Pin_0;   // FSMC_A0
    
    g_fsmc_sram.cfg.hw_cfg.aports[1].gpio_a = GPIOF;
    g_fsmc_sram.cfg.hw_cfg.aports[1].gpio_apin = GPIO_Pin_1;   // FSMC_A1
    
    g_fsmc_sram.cfg.hw_cfg.aports[2].gpio_a = GPIOF;
    g_fsmc_sram.cfg.hw_cfg.aports[2].gpio_apin = GPIO_Pin_2;   // FSMC_A2
    
    g_fsmc_sram.cfg.hw_cfg.aports[3].gpio_a = GPIOF;
    g_fsmc_sram.cfg.hw_cfg.aports[3].gpio_apin = GPIO_Pin_3;   // FSMC_A3
    
    g_fsmc_sram.cfg.hw_cfg.aports[4].gpio_a = GPIOF;
    g_fsmc_sram.cfg.hw_cfg.aports[4].gpio_apin = GPIO_Pin_4;   // FSMC_A4
    
    g_fsmc_sram.cfg.hw_cfg.aports[5].gpio_a = GPIOF;
    g_fsmc_sram.cfg.hw_cfg.aports[5].gpio_apin = GPIO_Pin_5;   // FSMC_A5
    
    g_fsmc_sram.cfg.hw_cfg.aports[6].gpio_a = GPIOF;
    g_fsmc_sram.cfg.hw_cfg.aports[6].gpio_apin = GPIO_Pin_12;  // FSMC_A6
    
    g_fsmc_sram.cfg.hw_cfg.aports[7].gpio_a = GPIOF;
    g_fsmc_sram.cfg.hw_cfg.aports[7].gpio_apin = GPIO_Pin_13;  // FSMC_A7
    
    g_fsmc_sram.cfg.hw_cfg.aports[8].gpio_a = GPIOF;
    g_fsmc_sram.cfg.hw_cfg.aports[8].gpio_apin = GPIO_Pin_14;  // FSMC_A8
    
    g_fsmc_sram.cfg.hw_cfg.aports[9].gpio_a = GPIOF;
    g_fsmc_sram.cfg.hw_cfg.aports[9].gpio_apin = GPIO_Pin_15;  // FSMC_A9
    
    g_fsmc_sram.cfg.hw_cfg.aports[10].gpio_a = GPIOG;
    g_fsmc_sram.cfg.hw_cfg.aports[10].gpio_apin = GPIO_Pin_0;  // FSMC_A10
    
    g_fsmc_sram.cfg.hw_cfg.aports[11].gpio_a = GPIOG;
    g_fsmc_sram.cfg.hw_cfg.aports[11].gpio_apin = GPIO_Pin_1;  // FSMC_A11
    
    g_fsmc_sram.cfg.hw_cfg.aports[12].gpio_a = GPIOG;
    g_fsmc_sram.cfg.hw_cfg.aports[12].gpio_apin = GPIO_Pin_2;  // FSMC_A12
    
    g_fsmc_sram.cfg.hw_cfg.aports[13].gpio_a = GPIOG;
    g_fsmc_sram.cfg.hw_cfg.aports[13].gpio_apin = GPIO_Pin_3;  // FSMC_A13
    
    g_fsmc_sram.cfg.hw_cfg.aports[14].gpio_a = GPIOG;
    g_fsmc_sram.cfg.hw_cfg.aports[14].gpio_apin = GPIO_Pin_4;  // FSMC_A14
    
    g_fsmc_sram.cfg.hw_cfg.aports[15].gpio_a = GPIOG;
    g_fsmc_sram.cfg.hw_cfg.aports[15].gpio_apin = GPIO_Pin_5;  // FSMC_A15
    
    g_fsmc_sram.cfg.hw_cfg.aports[16].gpio_a = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.aports[16].gpio_apin = GPIO_Pin_11; // FSMC_A16
    
    g_fsmc_sram.cfg.hw_cfg.aports[17].gpio_a = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.aports[17].gpio_apin = GPIO_Pin_12; // FSMC_A17
    
    g_fsmc_sram.cfg.hw_cfg.aports[18].gpio_a = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.aports[18].gpio_apin = GPIO_Pin_13; // FSMC_A18
    
    /* 控制端口初始化 */
    g_fsmc_sram.cfg.hw_cfg.cs.gpio_c = GPIOG;
    g_fsmc_sram.cfg.hw_cfg.cs.gpio_cpin = GPIO_Pin_12;        // FSMC_NE4 (片选)
    
    g_fsmc_sram.cfg.hw_cfg.oe.gpio_c = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.oe.gpio_cpin = GPIO_Pin_4;         // FSMC_NOE (输出使能)
    
    g_fsmc_sram.cfg.hw_cfg.we.gpio_c = GPIOD;
    g_fsmc_sram.cfg.hw_cfg.we.gpio_cpin = GPIO_Pin_5;         // FSMC_NWE (写使能)
    
    g_fsmc_sram.cfg.hw_cfg.ub.gpio_c = GPIOE;
    g_fsmc_sram.cfg.hw_cfg.ub.gpio_cpin = GPIO_Pin_1;         // FSMC_NBL1 (高字节掩码)
    
    g_fsmc_sram.cfg.hw_cfg.lb.gpio_c = GPIOE;
    g_fsmc_sram.cfg.hw_cfg.lb.gpio_cpin = GPIO_Pin_0;         // FSMC_NBL0 (低字节掩码)

    return DY_EOK;
}


/*配置*/
static int drv_fsmc_sram_ctrl_init()
{
     /******************************************************************
     * 
     *                      FSMC基础配置
     * 
     ******************************************************************/
    g_fsmc_sram.cfg.pg_cfg.bank_num = FSMC_Bank1_NORSRAM4;
    g_fsmc_sram.cfg.pg_cfg.mem_type = FSMC_MemoryType_SRAM;
    g_fsmc_sram.cfg.pg_cfg.data_with = FSMC_MemoryDataWidth_16b;

    g_fsmc_sram.cfg.pg_cfg.rd_cfg.addr_setup_time = 0x00;
    g_fsmc_sram.cfg.pg_cfg.rd_cfg.addr_hold_time = 0x00;
    g_fsmc_sram.cfg.pg_cfg.rd_cfg.data_setup_time = 0x08;
    g_fsmc_sram.cfg.pg_cfg.rd_cfg.bus_turn_around_duration = 0x00;
    g_fsmc_sram.cfg.pg_cfg.rd_cfg.clk_division = 0x00;
    g_fsmc_sram.cfg.pg_cfg.rd_cfg.data_latency = 0x00;
    g_fsmc_sram.cfg.pg_cfg.rd_cfg.access_mode = FSMC_AccessMode_A;

    g_fsmc_sram.cfg.pg_cfg.we_cfg.addr_setup_time = 0x00;
    g_fsmc_sram.cfg.pg_cfg.we_cfg.addr_hold_time = 0x00;
    g_fsmc_sram.cfg.pg_cfg.we_cfg.data_setup_time = 0x08;
    g_fsmc_sram.cfg.pg_cfg.we_cfg.bus_turn_around_duration = 0x00;
    g_fsmc_sram.cfg.pg_cfg.we_cfg.clk_division = 0x00;
    g_fsmc_sram.cfg.pg_cfg.we_cfg.data_latency = 0x00;
    g_fsmc_sram.cfg.pg_cfg.we_cfg.access_mode = FSMC_AccessMode_A;
    
    return DY_EOK;
}

int drv_fsmc_sram_init()
{
    int ret = DY_EOK;
    /*device初始化*/
    strncpy(g_fsmc_sram.device.name, "sram", sizeof(g_fsmc_sram.device.name) - 1);
    g_fsmc_sram.device.ops = &g_fsmc_sram_ops;

    /*配置初始化*/
    ret = drv_fsmc_sram_port_init();
    ret = drv_fsmc_sram_ctrl_init();

    /*注册*/
    if (DY_EOK != dy_device_register(g_fsmc_sram.device.name, &g_fsmc_sram.device))
    {
        return DY_ERROR;
    }  

    /*调用初始化接口*/
    ret = g_fsmc_sram.device.ops->init(&g_fsmc_sram.device);
    return DY_EOK;
}


