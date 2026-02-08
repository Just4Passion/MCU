
#include <stdbool.h>

#include "drv_misc_func.h"

#include "drv_timer.h"

#define TIMER2_OC_PWM 1
#define SINGLE_COLOR_PWM_LED 0
#define FULL_COLOR_PWM_LED 0
/**********************************************************************
 * 
 *                              定时器
 * 触发模式
 *      定时器可以由内部或外部的事件来启动、停止、重置或同步
 *      触发源
 *          外部引脚输入
 *          其他定时器的输出事件
 *          内部事件
 *      应用
 *          超声波测距触发、拍照快门
 * 门控模式
 *      定时器的计数使能受到一个外部信号的控制; 门控信号可以控制定时器的启动和停止
 *      工作方式
 *          门控信号为高电平时，定时器计数
 *          门控信号为低电平时，定时器停止计数
 *      应用
 *          PWM输入测量、电机使能控制
 * 
 * 关于时钟补偿机制
 *      为了APB总线频率较低时, 仍能获得较高精度: APBx预分频系数 = 1时, 定时器时钟 = APBx时钟; 否则是APBx时钟的2倍
 * 
 * DMA
 *      关键组成: DMA通道; 仲裁器(多个通道请求, 决定优先级); FIFO(数据缓冲区, 解决源和目标速度不匹配); 配置寄存器
 *      传输原理
 *          配置DMA->外设或软件触发DMA请求->源地址向目的地址写入数据->传输计数减1, 地址递增->传输完成, 产生中断, 释放总线
 *          DMA与CPU共享系统总线, 通过仲裁访问: 总线竞争的等待开销远小于CPU亲自进行数据搬运的开销; 且DMA搬运和CPU运算可以并行进行
 *      传输方向
 *          外设到内存
 *          内存到外设
 *          内存到内存
 *          外设到外设
 *      传输模式
 *          正常模式:
 *              特点: 传输完成后自动停止; 需要重新使能才能再次传输; 适合批量处理
 *          循环模式:
 *              特点: 永不停止, 自动循环; 适合流式数据; 需要双缓冲区或乒乓缓冲
 *          双缓冲模式
 *              特点: 两个缓冲区交替工作
 *          外设流控模式
 *              特点: 外设控制传输的节奏
 *              使用方式: 
 *                  外设通过其特定的硬件信号线（DMA请求线）向DMA控制器发出请求
 *                  例如，ADC转换完成、UART收到数据、定时器比较匹配等事件都可以触发DMA请求
 *      中断
 *          传输完成; 半传输完成; 传输错误; FIFO错误; 直接模式错误
 *      直接模式和FIFO
 *          直接模式: 接收到请求, 直接将数据从源地址写入到目标地址
 *          FIFO: 数据先被读取到FIFO中, 在从FIFO写入目标地址 —— 数据宽度不一致; 带宽不匹配; 突发传输
 * 
 ***********************************************************************/


/**********************************************************************
 * 
 *                              基础定时器
 * 本质: 纯时基发生器
 * 特点:    
 *      只有向上计数模式
 *      仅支持定时中断/DMA请求
 *      无外部I/O引脚(无法捕获/比较)
 *      最简单的内部时钟驱动
 *      DMA请求
 *          定时器事件（如更新事件、比较匹配、捕获事件）直接触发DMA传输
 * 简单应用场景:
 *      系统滴答定时
 *      DAC触发时钟: 从基本定时器框图可见一斑
 *          DAC在外部触发信号的作用下启用转换; 
 *          基本定时器可以生成周期性的触发信号, 用于自动触发DAC转换; 
 *          触发信号来源于计数器达到自动重载值时产生的更新事件
 *      简单延时生成
 * 组合应用场景
 *      波特率自动检测: 定时器6 + 外部中断
 *          把UART的RX引脚配置成外部中断; 定时器以一个比较高频率触发中断
 *          在外部中断中不断记录定时器6的计数器值; 在定时器6的中断中不断采样RX, 直到检测到停止位(上升沿)
 *          根据电平持续时间, 可以计算波特率
 * 
 * 一些特点
 *      基本定时器没有单脉冲模式, 没有重复计数器, 无法配置自动重载的次数. 对于基础定时器, 如果想要实现仅执行一次: 可以在第一次中断中停止定时器
 *
 * 
 ***********************************************************************/

/**********************************************************************
 * 
 *                              通用定时器
 * 本质: 多功能外设接口
 * 特点:    
 *      16(TIM3和TIM4)或32(TIM2和TIM5)位递增、递减和递增/递减自动重载计数器
 *      支持输入捕获
 *      支持输出比较(PWM生成、单脉冲)
 *      支持编码器接口(正交解码)
 *      多种计数模式(向上/向下/中央对齐)
 *      有外部引脚连接能力
 * 应用场景：
 *      电机PWM控制
 *      传感信号测量
 *      通用脉冲生成
 *      编码器计数
 * 
 ***********************************************************************/


 /**********************************************************************
 * 
 *                              高级定时器
 * 本质: 工业级控制引擎
 * 特点: 可编程死区互补输出、 重复计数器、带刹车(断路)功能
 *      互补输出带死区控制(驱动半桥/全桥)
 *      刹车功能(紧急关断，支持硬件刹车引脚)
 *      重复计数器(降低CPU中断频率)
 *      支持更复杂的PWM组合
 *      事件互连(与其他定时器同步)
 * 时钟源:
 *      内部时钟
 *          内部触发输入    
 *              内部触发输入是使用一个定时器作为另一个定时器的预分频器: 定时器同步或级联
 *              主模式的定时器可以对从模式定时器执行复位、启动、停止或提供时钟
 *              高级控制定时器和部分通用定时器(TIM2至TIM5)可以设置为主模式或从模式，TIM9和TIM10可设置为从模式
 *      外部时钟
 *          外部时钟模式1: 
 *              时钟信号输入引脚
 *              滤波器: 对外部时钟信号进行重新采样, 达到降频或去除高频干扰
 *              边沿检测: 上升沿有效/下降沿有效
 *              触发选择: 滤波器后的定时器输入1, 滤波后的定时器输入2
 *              从模式选择: 将触发源信号连接到触发引脚, 接入到外部时钟模式1输入信号线上
 *              使能计数器: 
 *          外部时钟模式2:
 *              时钟信号输入引脚: 时钟信号来自于定时器的特定输入通道
 *              外部触发极性: 上升沿或者下降沿有效
 *              外部触发预分频器: 进入的触发信号频率不能太高
 *              滤波器: 降频或去除高频干扰
 *              从模式选择: 选择外部时钟模式2
 *              使能计数器: 
 * 
 *  输入捕获：脉冲跳变沿时间测量；PWM输入测量
 *      测量频率: 两次上升沿捕获
 *      测量脉宽: 一次上升沿捕获, 一次下降沿捕获 —— 注意切换捕获方式; 关注定时器溢出
 *      PWM输入模式: 使用两个捕获寄存器, 占用两个通道
 *          两个寄存器捕获的极性相反
 *          将从模式控制器配置为复位模式
 * 输出比较：
 *      PWM输出模式：对外输出脉宽（即占空比）可调的方波信号
 * 
 * 断路功能：
 *      电机控制的刹车功能
 *              
 * 应用：
 *      电机FOC控制
 *      数字电源SMPS
 *      照明控制(呼吸灯)
 *      需要硬件保护的关键应用
 * 
 * 如何理解
 *      互补输出: 一对相位相反的PWM信号. 用于H桥驱动，防止直通
*          主输出: CHx, 高电平期间->上管导通
*          互补输出: CHxN, 低电平期间->下管导通
 * 
 *      死区插入: 安全延时，保护功率管
 *          上管关断 -> 死去时间(都关断) -> 下管导通 
 * 
 *      刹车功能：紧急关断，硬件保护
 * 
 ***********************************************************************/
typedef void(*adv_timer_irq_handle)();

typedef enum 
{
    TIMER_DEFAULT,
    TIMER_OUTPUT,
    TIMER_INPUT,
    TIMER_TIM2O_TIM8I
}adv_timer_io_t;

typedef struct
{
    TIM_TypeDef *timer_periph;      //不同的外设有不同的时钟源
    uint32_t periph_clk;            //RCC_APB1Periph_TIM6
    uint32_t io_flag;
}timer_hw_cfg_t;

typedef struct 
{
    /*定时多久*/
    uint16_t period;            //计数周期. 16位自动重载计数器
    uint16_t prescaler;         //预分频
    uint16_t counter_mode;      //上升沿计数; 下降沿计数
    uint16_t clk_division;      //时钟分频系数
    uint16_t repetition_counter;    //重复计数
}time_base_cfg_t;

/*输出配置*/
typedef struct
{
    uint8_t channel;            //可选择1, 2, 3, 4
    /***********************************
     * Timing: 固定速度
     * Active: 加速
     * Inactive:
     * Toggle: 
     * PWM1: 计数器 < CCR时输出有效电平
     * PWM2: 计数器 > CCR时输出有效电平
     * 
     ************************************/
    uint16_t oc_mode;       //输出模式: Timing, Active, Inactive, Toggle, PWM1, PWM2

    /*输出配置*/
    uint16_t output_state;      //主输出状态: 使能/禁止
    uint16_t oc_polarity;       //主输出极性: 高电平有效/低电平有效
    uint16_t oc_idle_state;     //空闲状态: 保持开启/关断

    /*互补输出配置*/
    uint16_t outputN_state;     //互补输出状态: 使能/禁止
    uint16_t ocN_polarity;      //互补输出极性
    uint16_t ocN_idle_state;    //互补空闲状态

    uint32_t pulse;             //脉冲电平维持时间计数: 定时器周期1000, 脉冲300, 则占空比是300/1000
}timer_OC_cfg_t;

/*输入捕获*/
typedef struct
{
    uint16_t channel;       //通道
    uint16_t ic_polarity;   //极性: 检测上升沿, 下降沿, 跳变沿
    uint16_t ic_selection;  //输入信号来源: 
    uint16_t ic_prescaler;  //采样频率: TIM_ICPSC_DIV1-每个边沿都捕获; DIV2-每2个边沿捕获一次
    uint16_t ic_filter;     //滤波等级
}timer_IC_cfg_t;

/*死区配置*/
typedef struct
{
    uint16_t ossr_state;                    //off-state in 运行模式: 运行模式下关闭状态. 可选择输出有效电平或无效电平
    uint16_t ossi_state;                    //off-state in 空闲模式: 空闲模式下关闭状态
    uint16_t lock_level;                    //锁定级别: 无锁定, 锁定部分, 锁定更多, 完全锁定(需要复位)
    uint16_t dead_time;                     //死区时间: 关到开之间的时间. 0x00 - 0xFF. 防止H桥上下管同时导通的延迟时间
    uint16_t break_enable;                  //刹车功能是否启用
    uint16_t break_polarity;                //刹车极性: 高电平/低电平触发刹车
    uint16_t automatic_output_enable;      //自动输出是否使能: 刹车后是否自动恢复输出
}timer_BDTR_cfg_t;

typedef struct
{
    timer_hw_cfg_t thw_cfg;
    time_base_cfg_t tbase_cfg;
    /*输出一路还是两路*/
    bool toc_motor_enable;  //电机驱动则双路输出
    timer_OC_cfg_t toc_cfg;
    /*捕获输入*/
    bool tic_enable;
    timer_IC_cfg_t tic_cfg;
    /*刹车, 死区, 自动输出, 锁定等级配置*/
    bool tBDTR_enable;
    timer_BDTR_cfg_t tbdtr_cfg;
}adv_timer_cfg_t;

typedef struct
{
    dy_device_t device;
    adv_timer_cfg_t cfg;
}adv_timer_dev_t;

adv_timer_dev_t g_adv_timer8;
/********************************************************
 *      TIM8的CH1和CH1N - 互补通道
 *          PC6: TIM8_CH1
 *          PA5: TIM8_CH1N
 *      TIM8_BKIN: 断路功能
 *          PA6引脚
 *          配置低电平有效
 *      按键用来调节PWM的占空比大小
 * 
 ********************************************************/
static int advance_timer_oc_init(dy_device_t *dev)
{
    /*GPIO输出*/
    GPIO_InitTypeDef stGPIOInit;

    /*基本配置*/
    TIM_TimeBaseInitTypeDef stTimerBaseInit;
    /*输出比较*/
    TIM_OCInitTypeDef stTimerOCInit;
    /*断路和死区配置*/
    TIM_BDTRInitTypeDef stTimerBDTRInit;

    /*GPIO时钟启用*/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);   //输出CH1
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);   //互补输出CH1N和断路输入

    /*复用配置*/
    GPIO_PinAFConfig(/*GPIO*/GPIOC, /*引脚*/GPIO_PinSource6, /*类型*/GPIO_AF_TIM8); //CH1 OUT
    GPIO_PinAFConfig(/*GPIO*/GPIOA, /*引脚*/GPIO_PinSource5, /*类型*/GPIO_AF_TIM8); //CH1N OUT
    GPIO_PinAFConfig(/*GPIO*/GPIOA, /*引脚*/GPIO_PinSource6, /*类型*/GPIO_AF_TIM8); //break IN

    stGPIOInit.GPIO_Mode = GPIO_Mode_AF;
    stGPIOInit.GPIO_OType = GPIO_OType_PP;
    stGPIOInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
    stGPIOInit.GPIO_Speed = GPIO_Speed_100MHz;
    /*PWM*/
    stGPIOInit.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOC, &stGPIOInit);
    /*PWM-N*/
    stGPIOInit.GPIO_Pin = GPIO_Pin_5;
    GPIO_Init(GPIOA, &stGPIOInit);
    /*Break IN*/
    stGPIOInit.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOA, &stGPIOInit);


    /*定时器时钟启用*/
    RCC_AHB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
    /*定时器基本配置: 预分频模式, 计数周期*/
    stTimerBaseInit.TIM_ClockDivision = TIM_CKD_DIV1;
    stTimerBaseInit.TIM_CounterMode = TIM_CounterMode_Up;
    stTimerBaseInit.TIM_Prescaler = 1680 - 1;        //168MHz / 168 = 1MHz / 10 = 100 * 1KHz
    stTimerBaseInit.TIM_Period = 1000 - 1;           //1s 100次重载. 1次重载10ms
    stTimerBaseInit.TIM_RepetitionCounter = 0;       //不进行重复计数
    TIM_TimeBaseInit(TIM8, &stTimerBaseInit);
    /*定时器输出配置*/
    stTimerOCInit.TIM_OCMode = TIM_OCMode_PWM1;
    stTimerOCInit.TIM_OutputState = TIM_OutputState_Enable;  //正向输出
    stTimerOCInit.TIM_OCPolarity = TIM_OCPolarity_High;
    stTimerOCInit.TIM_OCIdleState = TIM_OCIdleState_Set;
    stTimerOCInit.TIM_OutputNState = TIM_OutputNState_Enable;  //互补输出
    stTimerOCInit.TIM_OCNPolarity = TIM_OCNPolarity_High;
    stTimerOCInit.TIM_OCNIdleState = TIM_OCNIdleState_Reset;

    stTimerOCInit.TIM_Pulse = 127;                       //调节占空比: TIM_SetCompare1(TIM8, pulse);
    TIM_OC1Init(TIM8, &stTimerOCInit);                   //使能通道1
    TIM_OC1PreloadConfig(TIM8, TIM_OCPreload_Enable);   //使能通道1重载
    /*定时器: 自动输出, 断路, 死区时间, 锁定配置*/
    stTimerBDTRInit.TIM_OSSRState = TIM_OSSRState_Enable;                //运行模式下的关闭状态
    stTimerBDTRInit.TIM_OSSIState = TIM_OSSIState_Enable;                //空闲模式下的关闭状态
    stTimerBDTRInit.TIM_LOCKLevel = TIM_LOCKLevel_1;                     //锁定等级
    stTimerBDTRInit.TIM_DeadTime = 11;                                   //死区时间
    stTimerBDTRInit.TIM_Break = TIM_Break_Enable;                        //刹车使能
    stTimerBDTRInit.TIM_BreakPolarity = TIM_BreakPolarity_Low;           //刹车触发电平
    stTimerBDTRInit.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;    //刹车后自动恢复输出是否使能
    TIM_BDTRConfig(TIM8, &stTimerBDTRInit);

    /*使能定时器*/
    TIM_Cmd(TIM8, ENABLE);

    /*使能主输出*/
    TIM_CtrlPWMOutputs(TIM8, ENABLE);

    return DY_EOK;
}

/********************************************************
 *      通用定时器用于波形输出
 *          TIM2(RCC_APB1Periph_TIM2, APB1[42M]) - PA5: PWM输出 - TIM2_CH1_ETR
 *      高级定时器用于输入捕获
 *          TIM8() - PC6: PWM输入捕获 - TIM8_CH1
 *          两个捕获寄存器, 一个用于测周期, 一个用于测占空比
 ********************************************************/
static int advance_timer_ic_init(dy_device_t *dev)
{
    /**************************************
     *          
     *              TIM2输出PWM波
     * 
     ***************************************/
    /*GPIO端口*/
    GPIO_InitTypeDef stGPIOInit;
    /*输出PWM波形*/
    TIM_TimeBaseInitTypeDef  stTimerBaseInit;
    TIM_OCInitTypeDef  stTimerOCInit;

    /*使能GPIO时钟*/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);   //PA5
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_TIM2); //映射到TIM2
    stGPIOInit.GPIO_Mode = GPIO_Mode_AF;
    stGPIOInit.GPIO_OType = GPIO_OType_PP;
    stGPIOInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
    stGPIOInit.GPIO_Speed = GPIO_Speed_100MHz;
    stGPIOInit.GPIO_Pin = GPIO_Pin_5;
    GPIO_Init(GPIOA, &stGPIOInit);

    /*使能定时器时钟*/
    RCC_AHB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    /*基本配置*/
    stTimerBaseInit.TIM_ClockDivision = TIM_CKD_DIV1;
    stTimerBaseInit.TIM_Prescaler = (84 - 1); // 84MHz
    stTimerBaseInit.TIM_Period = (10 - 1);
    stTimerBaseInit.TIM_CounterMode = TIM_CounterMode_Up;   //上升沿计数
    stTimerBaseInit.TIM_RepetitionCounter = 0;
    /*输出比较*/
    stTimerOCInit.TIM_OCMode = TIM_OCMode_PWM1;
    stTimerOCInit.TIM_OutputState = TIM_OutputState_Enable; // 输出使能
    stTimerOCInit.TIM_OCPolarity = TIM_OCPolarity_High;     // 输出通道电平极性配置
    stTimerOCInit.TIM_Pulse = 5;
    TIM_OC1Init(TIM2, &stTimerOCInit);                // 使能通道
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);       //使能通道重载

    /*使能定时器*/
    TIM_Cmd(TIM2, ENABLE);

    /**************************************
     *          
     *              TIM8捕获输入
     * 
     ***************************************/
    /*GPIO端口*/
    GPIO_InitTypeDef stGPIOInputInit;
    /*输入PWM波形*/
    TIM_TimeBaseInitTypeDef  stTimerICBaseInit;
    TIM_ICInitTypeDef  stTimerICInit;
    NVIC_InitTypeDef stTimerNVICInit;

    /*GPIO配置: 时钟使能*/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);    //PC6
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_TIM8); //映射到TIM8
    stGPIOInputInit.GPIO_Mode = GPIO_Mode_AF;
    stGPIOInputInit.GPIO_OType = GPIO_OType_PP;
    stGPIOInputInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
    stGPIOInputInit.GPIO_Speed = GPIO_Speed_100MHz;

    stGPIOInputInit.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOC, &stGPIOInputInit);

    /*定时器基本配置*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
    stTimerICBaseInit.TIM_Period = 1000 - 1;
    stTimerICBaseInit.TIM_Prescaler = 168 - 1;
    stTimerICBaseInit.TIM_ClockDivision = TIM_CKD_DIV1;
    stTimerICBaseInit.TIM_CounterMode = TIM_CounterMode_Up;
    stTimerICBaseInit.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM8, &stTimerICBaseInit);

    /*捕获输入: 两路, 一路测量周期, 一路测量占空比. 配置一路即可, 另一路硬件自带设置*/
    stTimerICInit.TIM_Channel = TIM_Channel_1;                  //通道
    stTimerICInit.TIM_ICPolarity = TIM_ICPolarity_Rising;       //捕获边沿
    stTimerICInit.TIM_ICSelection = TIM_ICSelection_DirectTI;   //捕获信号输入
    stTimerICInit.TIM_ICPrescaler = TIM_ICPSC_DIV1;             //捕获信号的每个有效边沿都捕获
    stTimerICInit.TIM_ICFilter = 0x0;                           //不滤波
    TIM_PWMIConfig(TIM8, &stTimerICInit);

    /*输入捕获的触发信号*/
    TIM_SelectInputTrigger(TIM8, TIM_TS_TI1FP1);

    /*PWM输入模式, 选择[从模式-复位模式], 当捕获开始时,计数器CNT会被复位*/
    TIM_SelectSlaveMode(TIM8, TIM_SlaveMode_Reset);
    TIM_SelectMasterSlaveMode(TIM8, TIM_MasterSlaveMode_Enable);

    /*中断配置*/
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
    stTimerNVICInit.NVIC_IRQChannel = TIM8_CC_IRQn;
    stTimerNVICInit.NVIC_IRQChannelPreemptionPriority = 0;
    stTimerNVICInit.NVIC_IRQChannelSubPriority = 3;
    stTimerNVICInit.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&stTimerNVICInit);

    /*使能捕获中断,这个中断针对的是主捕获通道（测量周期那个）*/
    TIM_ITConfig(TIM8, TIM_IT_CC1, ENABLE);
    TIM_ClearITPendingBit(TIM8, TIM_IT_CC1);

    /*使能定时器*/
    TIM_Cmd(TIM8, ENABLE);

    return DY_EOK;
}

static int advance_timer_init(dy_device_t *dev)
{
    int ret = DY_EOK;
    adv_timer_dev_t *timer = (adv_timer_dev_t*)dev;
    switch(timer->cfg.thw_cfg.io_flag)
    {
        /*输出比较*/
        case TIMER_OUTPUT:
            ret = advance_timer_oc_init(dev);
            break;
        /*输入捕获*/
        case TIMER_INPUT:
            ret = advance_timer_ic_init(dev);
            break;
        /*TIM2输出PWM波, TIM8输入捕获*/
        case TIMER_TIM2O_TIM8I:
            ret = advance_timer_ic_init(dev);
            break;
        default:

            break;
    }
    return ret;
}

static int advance_timer_control(dy_device_t *dev, int cmd, void *arg)
{
    switch (cmd)
    {
        case TIMER_OC_START:
            TIM_CtrlPWMOutputs(TIM8, ENABLE);
            break;
        case TIMER_OC_STOP:
            TIM_CtrlPWMOutputs(TIM8, DISABLE);
            break;
        default:
            dy_device_control(dev, cmd, arg);
            break;
    }
    return DY_EOK;
}


device_ops_t g_adv_timer_ops = {
    .init = advance_timer_init,
    .control = advance_timer_control
};


int drv_adv_timer_init()
{
    int ret = DY_EOK;
    strncpy(g_adv_timer8.device.name, "timer8", sizeof(g_adv_timer8.device.name) - 1);
    g_adv_timer8.device.ops = &g_adv_timer_ops;

    g_adv_timer8.cfg.thw_cfg.timer_periph = TIM8;
    g_adv_timer8.cfg.thw_cfg.periph_clk = RCC_APB2Periph_TIM8;
    g_adv_timer8.cfg.thw_cfg.io_flag = TIMER_OUTPUT;

    g_adv_timer8.cfg.tbase_cfg.clk_division = TIM_CKD_DIV1;         //时钟分频系数
    g_adv_timer8.cfg.tbase_cfg.prescaler = 1680 - 1;                //预分频
    g_adv_timer8.cfg.tbase_cfg.period = 1000 - 1;                   //计数周期. 16位自动重载计数器
    g_adv_timer8.cfg.tbase_cfg.counter_mode = TIM_CounterMode_Up;   //上升沿计数; 下降沿计数
    g_adv_timer8.cfg.tbase_cfg.repetition_counter = 0;              //重复计数
    #if 0
    g_adv_timer8.cfg.toc_cfg;
    g_adv_timer8.cfg.tic_cfg;
    g_adv_timer8.cfg.tbdtr_cfg;
    #endif

    if (DY_EOK != dy_device_register(g_adv_timer8.device.name, &g_adv_timer8.device))
    {
        return DY_ERROR;
    }
    
    ret = g_adv_timer8.device.ops->init(&g_adv_timer8.device);

    return DY_EOK;
}

/**********************************************************************************************************************************
 * 
 * 
 *                                                  定时器配置
 * 
 * 
 * 
 ***********************************************************************************************************************************/
typedef void(*timer_irq_handler)(dy_device_t *dev);
typedef struct
{
    bool irq_enable;
    uint16_t it_flags;   //使能哪些中断标志位
    uint8_t irq_channel;
    uint8_t group_priority;
    uint8_t sub_priority;
    timer_irq_handler timerx_handler;
}timer_irq_cfg_t;

/*基本定时器, 计时作用*/
typedef struct
{
    dy_device_t device;
    /*定时器类型*/
    TIM_TypeDef *timer_periph;      //不同的外设有不同的时钟源
    uint32_t periph_clk;            //RCC_APB1Periph_TIM6

    time_base_cfg_t tbase_cfg;

    /*中断*/
    timer_irq_cfg_t tirq;
}base_timer_dev_t;

/*基本定时器, 计时作用*/
typedef struct
{
    dy_device_t device;
    /*定时器类型*/
    TIM_TypeDef *timer_periph;      //不同的外设有不同的时钟源
    uint32_t periph_clk;            //RCC_APB1Periph_TIM6

    /*工作模式*/
    uint8_t timer_mode;             //0, 输出单个PWM波; 1, 输出互补的PWM波用于驱动电机

    /*GPIO主输出配置*/
    GPIO_TypeDef *gpio_m;
    uint16_t gpio_mpin;
    uint8_t gpio_maf;
    /*GPIO互补配置*/
    GPIO_TypeDef *gpio_n;
    uint16_t gpio_npin;
    uint8_t gpio_naf;

    /*刹车配置 - 刹车输入触发GPIO*/
    bool break_enable;              //是否需要刹车功能
    GPIO_TypeDef *gpio_bk;
    uint16_t gpio_bkpin;
    uint8_t gpio_bkaf;

    time_base_cfg_t tbase_cfg;
    timer_OC_cfg_t toc_cfg;
    timer_BDTR_cfg_t tbdtr_cfg;

    /*中断*/
    timer_irq_cfg_t tirq;
}oc_timer_dev_t;

typedef struct
{
    dy_device_t device;
    /*定时器类型*/
    TIM_TypeDef *timer_periph;      //不同的外设有不同的时钟源
    uint32_t periph_clk;            //RCC_APB1Periph_TIM6

    /*GPIO主输入配置 - 两个捕获寄存器, 另一个硬件自动配置*/
    GPIO_TypeDef *gpio_m;
    uint16_t gpio_mpin;
    uint8_t gpio_maf;

    time_base_cfg_t tbase_cfg;
    timer_IC_cfg_t tic_cfg;

    uint16_t input_trigger_single;
    uint16_t slave_mode;

    /*中断*/
    timer_irq_cfg_t tirq;
}ic_timer_dev_t;

static void timer_APB_clk_enable_v1(uint32_t timer_periph, uint32_t periph_clk)
{
    switch(timer_periph)
    {
        /*高级定时器*/
        case TIM1:      //PWM
        case TIM8:      //PWM
            RCC_APB2PeriphClockCmd(periph_clk, ENABLE);
            break;
        /*通用定时器*/
        case TIM3:
        case TIM4:
        case TIM5:
            RCC_APB1PeriphClockCmd(periph_clk, ENABLE);
            break;
        /*基础定时器*/
        case TIM6:
        case TIM7:
            RCC_APB1PeriphClockCmd(periph_clk, ENABLE);
            break;
        /*通用定时器*/
        case TIM9:
        case TIM10:
        case TIM11:
            RCC_APB2PeriphClockCmd(periph_clk, ENABLE);
            break;
        /*通用定时器*/
        case TIM12:
        case TIM13:
        case TIM14:
            RCC_APB1PeriphClockCmd(periph_clk, ENABLE);
            break;
        default:
            break;
    }
}

static void base_timer_irq_handler(dy_device_t *dev)
{
    base_timer_dev_t *timer = (base_timer_dev_t*)dev;
    static uint32_t start_time = 0;
    uint32_t record_time = 0;
    /*中断标志位: 更新事件发生*/
    if (SET == TIM_GetFlagStatus(timer->timer_periph, TIM_FLAG_Update))
    {
        /*统计每次中断的间隔时间*/
        record_time = drv_time_get_sys_tick();
        printf("%s irq happen, cost = %u\r\n", timer->device.name, (record_time - start_time));

        /*清空中断标志*/
        start_time = record_time;
        TIM_ClearFlag(timer->timer_periph, TIM_FLAG_Update);
    }
}

/************************************************************************************************************************
 * 
 *                                              基本定时功能定时器
 * 
 * 
 * 功能: 支持定时功能; 支持DMA; 支持定时触发输出
 * 包括: 基础定时器TIM6, TIM7; 通用定时器TIM2-TIM5, TIM9-TIM14; 高级定时器TIM1, TIM8
 * 
 * TIM2-TIM5: 它们都是单独的中断函数, TIMx_IRQHandler()
 * TIM9-TIM14: 它们的断函数都是组合中断, 比如TIM1_BRK_TIM9_IRQHandler, TIM8_BRK_TIM12_IRQHandler
 * 
 * 
 * 
 *************************************************************************************************************************/
static int base_timer_init(dy_device_t *dev)
{
    base_timer_dev_t *timer = (base_timer_dev_t*)dev;

    TIM_TimeBaseInitTypeDef  stTimerBase;
    /*启动时钟*/
    timer_APB_clk_enable_v1(timer->timer_periph, timer->periph_clk);

    /*开始配置定时器*/
    stTimerBase.TIM_ClockDivision = timer->tbase_cfg.clk_division;  //分频
    stTimerBase.TIM_Prescaler = timer->tbase_cfg.prescaler;         //预分频值
    stTimerBase.TIM_Period = timer->tbase_cfg.period;               //计数值
    stTimerBase.TIM_CounterMode = TIM_CounterMode_Up;               //触发计数条件
    stTimerBase.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(timer->timer_periph, &stTimerBase);

    if (timer->tirq.irq_enable)
    {
        /*使能中断标志位*/
        TIM_ITConfig(timer->timer_periph, timer->tirq.it_flags, ENABLE);

        /*使能NVIC中断号*/
        NVIC_InitTypeDef stNVICInit = {0};
        stNVICInit.NVIC_IRQChannel = timer->tirq.irq_channel;   //中断号
        stNVICInit.NVIC_IRQChannelPreemptionPriority = timer->tirq.group_priority; //抢占优先级
        stNVICInit.NVIC_IRQChannelSubPriority = timer->tirq.sub_priority;          //子优先级
        stNVICInit.NVIC_IRQChannelCmd = ENABLE;                 //使能
        NVIC_Init(&stNVICInit);
    }

    /*启动定时器*/
    TIM_Cmd(timer->timer_periph, ENABLE);
    return DY_EOK;
}

static int base_timer_control(dy_device_t *dev, int cmd, void *arg)
{
    base_timer_dev_t *timer = (base_timer_dev_t*)dev;
    switch(cmd)
    {
        case BTIMER_START:          //启动
            TIM_Cmd(timer->timer_periph, ENABLE);
            break;
        case BTIMER_STOP:           //停止
            TIM_Cmd(timer->timer_periph, DISABLE);
            break;
        case BTIMER_EN_IRQ:         //中断使能
            TIM_ClearFlag(timer->timer_periph, TIM_FLAG_Update);
            TIM_ITConfig(timer->timer_periph, TIM_IT_Update, ENABLE);
            /*还要配置NVIC*/
            break;
        case BTIMER_DN_IRQ:         //关闭中断
            TIM_ITConfig(timer->timer_periph, TIM_IT_Update, DISABLE);
            /*还要配置NVIC*/
            break;
        case BTIMER_EN_DMA:         //使能DMA
            TIM_DMACmd(timer->timer_periph, TIM_DMA_Update, ENABLE);
            /*还要配置DMA*/
            break;
        case BTIMER_DN_DMA:         //关闭DMA
            /*还要关闭DMA*/
            TIM_DMACmd(timer->timer_periph, TIM_DMA_Update, DISABLE);
            break;
        case BTIMER_OUTPUT_TRIGGER: //输出触发
            TIM_SelectOutputTrigger(timer->timer_periph, TIM_TRGOSource_Update);
            break;
        case BTIMER_IRQ_EXE:
            if (timer->tirq.irq_enable && timer->tirq.timerx_handler != NULL)
            {
                timer->tirq.timerx_handler(dev);
            }
            break;
        case BTIMER_GET_CNT:
            (*(uint16_t*)arg) = TIM_GetCounter(timer->timer_periph);
            break;
        default:
            break;
    }
    return DY_EOK;
}

/*测试成功: TIM6_DAC_IRQHandler*/
base_timer_dev_t g_timer6 = {
    .timer_periph = TIM6,
    .periph_clk = RCC_APB1Periph_TIM6,
    .tbase_cfg.clk_division = TIM_CKD_DIV1,
    .tbase_cfg.prescaler = (8400 - 1),
    .tbase_cfg.period = (10000 - 1),
    .tbase_cfg.counter_mode = TIM_CounterMode_Up,
    .tbase_cfg.repetition_counter = 0,
    .tirq.irq_enable = true,
    .tirq.irq_channel = TIM6_DAC_IRQn,
    .tirq.group_priority = 0,
    .tirq.sub_priority = 3,
    .tirq.it_flags = TIM_FLAG_Update,   //更新中断
    .tirq.timerx_handler = base_timer_irq_handler //中断函数回调
};

/*测试成功: TIM1_UP_TIM10_IRQHandler*/
base_timer_dev_t g_base_timer1 = {
    .timer_periph = TIM1,
    .periph_clk = RCC_APB2Periph_TIM1,
    .tbase_cfg.clk_division = TIM_CKD_DIV1,
    .tbase_cfg.prescaler = (16800 - 1),          //168M / 16800 = 10000
    .tbase_cfg.period = (10000 - 1),            //
    .tbase_cfg.counter_mode = TIM_CounterMode_Up,
    .tbase_cfg.repetition_counter = 0,
    .tirq.irq_enable = true,
    .tirq.irq_channel = TIM1_UP_TIM10_IRQn,
    .tirq.group_priority = 0,
    .tirq.sub_priority = 3,
    .tirq.it_flags = TIM_FLAG_Update,   //更新中断
    .tirq.timerx_handler = base_timer_irq_handler //中断函数回调
};

/*测试成功：TIM1_UP_TIM10_IRQHandler*/
base_timer_dev_t g_base_timer10 = {
    .timer_periph = TIM10,
    .periph_clk = RCC_APB2Periph_TIM10,
    .tbase_cfg.clk_division = TIM_CKD_DIV1,
    .tbase_cfg.prescaler = (16800 - 1),         //168M / 16800 = 10000
    .tbase_cfg.period = (10000 - 1),            //
    .tbase_cfg.counter_mode = TIM_CounterMode_Up,
    .tbase_cfg.repetition_counter = 0,
    .tirq.irq_enable = true,
    .tirq.irq_channel = TIM1_UP_TIM10_IRQn,
    .tirq.group_priority = 0,
    .tirq.sub_priority = 3,
    .tirq.it_flags = TIM_FLAG_Update,             //更新中断
    .tirq.timerx_handler = base_timer_irq_handler //中断函数回调
};

device_ops_t g_base_timer_ops = {
    .init = base_timer_init,
    .control = base_timer_control
};

int drv_base_timer_init()
{
    int ret = DY_EOK;
    #if 1
    strncpy(g_timer6.device.name, "timer6", sizeof(g_timer6.device.name) - 1);
    g_timer6.device.ops = &g_base_timer_ops;

    if (DY_EOK != dy_device_register(g_timer6.device.name, &g_timer6.device))
    {
        return DY_ERROR;
    }
    ret = g_timer6.device.ops->init(&g_timer6.device);
    #endif

    #if 0
    strncpy(g_base_timer1.device.name, "timer1", sizeof(g_base_timer1.device.name) - 1);
    g_base_timer1.device.ops = &g_base_timer_ops_v1;

    if (DY_EOK != dy_device_register(g_base_timer1.device.name, &g_base_timer1.device))
    {
        return DY_ERROR;
    }
    ret = g_base_timer1.device.ops->init(&g_base_timer1.device);
    #endif

    #if 0
    strncpy(g_base_timer10.device.name, "timer10", sizeof(g_base_timer10.device.name) - 1);
    g_base_timer10.device.ops = &g_base_timer_ops_v1;

    if (DY_EOK != dy_device_register(g_base_timer10.device.name, &g_base_timer10.device))
    {
        return DY_ERROR;
    }
    ret = g_base_timer10.device.ops->init(&g_base_timer10.device);
    #endif

    return DY_EOK;
}


/**************************************************************************************************************
 * 
 *                              OC定时器配置
 * 
 * 
 * 输出比较: 驱动电机; 输出PWM波
 * 选择通道进行初始化
 * 
 * 
 * 
 ***************************************************************************************************************/
static int oc_timer_init(dy_device_t *dev)
{
    oc_timer_dev_t *timer = (oc_timer_dev_t*)dev;

    /*GPIO输出*/
    GPIO_InitTypeDef stGPIOInit;

    /*基本配置*/
    TIM_TimeBaseInitTypeDef stTimerBaseInit;
    /*输出比较*/
    TIM_OCInitTypeDef stTimerOCInit;
    /*断路和死区配置*/
    TIM_BDTRInitTypeDef stTimerBDTRInit;

    /*时钟*/
    drv_gpio_enable_clk((uint32_t)(timer->gpio_m));
    /*复用*/
    GPIO_PinAFConfig(timer->gpio_m, drv_gpio_get_af_src(timer->gpio_mpin), timer->gpio_maf);
    /*基本配置*/
    stGPIOInit.GPIO_Mode = GPIO_Mode_AF;
    stGPIOInit.GPIO_OType = GPIO_OType_PP;
    stGPIOInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
    stGPIOInit.GPIO_Speed = GPIO_Speed_100MHz;
    stGPIOInit.GPIO_Pin = timer->gpio_mpin;
    GPIO_Init(timer->gpio_m, &stGPIOInit);
    if (timer->timer_mode)
    {
        /*时钟*/
        drv_gpio_enable_clk((uint32_t)(timer->gpio_n));
        /*复用*/
        GPIO_PinAFConfig(timer->gpio_n, drv_gpio_get_af_src(timer->gpio_npin), timer->gpio_naf);
        /*基本配置*/
        stGPIOInit.GPIO_Mode = GPIO_Mode_AF;
        stGPIOInit.GPIO_OType = GPIO_OType_PP;
        stGPIOInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
        stGPIOInit.GPIO_Speed = GPIO_Speed_100MHz;
        stGPIOInit.GPIO_Pin = timer->gpio_npin;
        GPIO_Init(timer->gpio_n, &stGPIOInit);
    }

    /*定时器时钟启用*/
    timer_APB_clk_enable_v1(timer->timer_periph, timer->periph_clk);
    /*定时器基本配置: 预分频模式, 计数周期*/
    stTimerBaseInit.TIM_ClockDivision = timer->tbase_cfg.clk_division;              //时钟分频
    stTimerBaseInit.TIM_Prescaler = timer->tbase_cfg.prescaler;                     //168MHz / 168 = 1MHz / 10 = 100 * 1KHz
    stTimerBaseInit.TIM_Period = timer->tbase_cfg.period;                           //1s 100次重载. 1次重载10ms
    stTimerBaseInit.TIM_CounterMode = timer->tbase_cfg.counter_mode;                //计数模式
    stTimerBaseInit.TIM_RepetitionCounter = timer->tbase_cfg.repetition_counter;    //不进行重复计数
    TIM_TimeBaseInit(timer->timer_periph, &stTimerBaseInit);
    /*定时器输出配置*/
    stTimerOCInit.TIM_OCMode = timer->toc_cfg.oc_mode;
    stTimerOCInit.TIM_OutputState = timer->toc_cfg.output_state;  //正向输出
    stTimerOCInit.TIM_OCPolarity = timer->toc_cfg.oc_polarity;
    stTimerOCInit.TIM_OCIdleState = timer->toc_cfg.oc_idle_state;
    if (timer->timer_mode)
    {
        stTimerOCInit.TIM_OutputNState = timer->toc_cfg.outputN_state;  //互补输出
        stTimerOCInit.TIM_OCNPolarity = timer->toc_cfg.ocN_polarity;
        stTimerOCInit.TIM_OCNIdleState = timer->toc_cfg.ocN_idle_state;
    }
    stTimerOCInit.TIM_Pulse = timer->toc_cfg.pulse;                      //调节占空比: TIM_SetCompare1(TIM8, pulse);
    switch (timer->toc_cfg.channel)
    {
        case 1:
            TIM_OC1Init(timer->timer_periph, &stTimerOCInit);                    //使能通道1
            TIM_OC1PreloadConfig(timer->timer_periph, TIM_OCPreload_Enable);     //使能通道1重载: 比较寄存器CCRx, CCRx存储比较值, 比如PWM的占空比
            break;
        case 2:
            TIM_OC2Init(timer->timer_periph, &stTimerOCInit); 
            TIM_OC2PreloadConfig(timer->timer_periph, TIM_OCPreload_Enable);
            break;
        case 3:
            TIM_OC3Init(timer->timer_periph, &stTimerOCInit); 
            TIM_OC3PreloadConfig(timer->timer_periph, TIM_OCPreload_Enable);
        case 4:
            TIM_OC4Init(timer->timer_periph, &stTimerOCInit);
            TIM_OC4PreloadConfig(timer->timer_periph, TIM_OCPreload_Enable);
        default:
            return DY_EINVAL;
    }
    TIM_ARRPreloadConfig(timer->timer_periph, ENABLE);                  //使能重载寄存器ARR: 定时器的周期/频率控制器(向上计数, 向下计数, 中央对齐)
    
    /*刹车功能配置*/
    if (timer->break_enable)
    {
        /*时钟*/
        drv_gpio_enable_clk((uint32_t)(timer->gpio_bk));
        /*复用*/
        GPIO_PinAFConfig(timer->gpio_bk, drv_gpio_get_af_src(timer->gpio_bkpin), timer->gpio_bkaf);
        /*基本配置*/
        stGPIOInit.GPIO_Mode = GPIO_Mode_AF;
        stGPIOInit.GPIO_OType = GPIO_OType_PP;
        stGPIOInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
        stGPIOInit.GPIO_Speed = GPIO_Speed_100MHz;
        stGPIOInit.GPIO_Pin = timer->gpio_bkpin;
        GPIO_Init(timer->gpio_bk, &stGPIOInit);

        /*定时器: 自动输出, 断路, 死区时间, 锁定配置*/
        stTimerBDTRInit.TIM_OSSRState = timer->tbdtr_cfg.ossr_state;                        //运行模式下的关闭状态
        stTimerBDTRInit.TIM_OSSIState = timer->tbdtr_cfg.ossi_state;                        //空闲模式下的关闭状态
        stTimerBDTRInit.TIM_LOCKLevel = timer->tbdtr_cfg.lock_level;                        //锁定等级
        stTimerBDTRInit.TIM_DeadTime = timer->tbdtr_cfg.dead_time;                                   //死区时间
        stTimerBDTRInit.TIM_Break = timer->tbdtr_cfg.break_enable;                          //刹车使能
        stTimerBDTRInit.TIM_BreakPolarity = timer->tbdtr_cfg.break_polarity;                //刹车触发电平
        stTimerBDTRInit.TIM_AutomaticOutput = timer->tbdtr_cfg.automatic_output_enable;     //刹车后自动恢复输出是否使能
        TIM_BDTRConfig(timer->timer_periph, &stTimerBDTRInit);
    }

    /*中断*/
    if (timer->tirq.irq_enable)
    {
        /*使能中断标志位*/
        TIM_ITConfig(timer->timer_periph, timer->tirq.it_flags, ENABLE);

        /*使能NVIC中断号*/
        NVIC_InitTypeDef stNVICInit = {0};
        stNVICInit.NVIC_IRQChannel = timer->tirq.irq_channel;   //中断号
        stNVICInit.NVIC_IRQChannelPreemptionPriority = timer->tirq.group_priority; //抢占优先级
        stNVICInit.NVIC_IRQChannelSubPriority = timer->tirq.sub_priority;          //子优先级
        stNVICInit.NVIC_IRQChannelCmd = ENABLE;                 //使能
        NVIC_Init(&stNVICInit);
    }

    /*使能定时器*/
    TIM_Cmd(timer->timer_periph, ENABLE);

    if (timer->timer_mode)
    {
        /*使能主输出*/
        TIM_CtrlPWMOutputs(timer->timer_periph, ENABLE);
    }

    return DY_EOK;
}

static int oc_timer_control(dy_device_t *dev, int cmd, void *arg)
{
    oc_timer_dev_t *timer = (oc_timer_dev_t*)dev;
    switch(cmd)
    {
        case OCTIMER_START:          //启动
            TIM_Cmd(timer->timer_periph, ENABLE);
            break;
        case OCTIMER_STOP:           //停止
            TIM_Cmd(timer->timer_periph, DISABLE);
            break;
        case OCTIMER_EN_IRQ:         //中断使能
            TIM_ClearFlag(timer->timer_periph, TIM_FLAG_Update);
            TIM_ITConfig(timer->timer_periph, TIM_IT_Update, ENABLE);
            /*还要配置NVIC*/
            break;
        case OCTIMER_DN_IRQ:         //关闭中断
            TIM_ITConfig(timer->timer_periph, TIM_IT_Update, DISABLE);
            /*还要配置NVIC*/
            break;
        case OCTIMER_EN_DMA:         //使能DMA
            TIM_DMACmd(timer->timer_periph, TIM_DMA_Update, ENABLE);
            /*还要配置DMA*/
            break;
        case OCTIMER_DN_DMA:         //关闭DMA
            /*还要关闭DMA*/
            TIM_DMACmd(timer->timer_periph, TIM_DMA_Update, DISABLE);
            break;
        case OCTIMER_IRQ_EXE:
            if (timer->tirq.irq_enable && timer->tirq.timerx_handler != NULL)
            {
                timer->tirq.timerx_handler(dev);
            }
            break;
        case OCTIMER_GET_CNT:
            (*(uint16_t*)arg) = TIM_GetCounter(timer->timer_periph);
            break;
        case OCTIMER_SET_PULSE:
            if (1 == timer->toc_cfg.channel)
            { TIM_SetCompare1(timer->timer_periph, (*(uint32_t*)arg)); }
            else if (2 == timer->toc_cfg.channel)
            { TIM_SetCompare2(timer->timer_periph, (*(uint32_t*)arg)); }
            else if (3 == timer->toc_cfg.channel)
            { TIM_SetCompare3(timer->timer_periph, (*(uint32_t*)arg)); }
            else if (4 == timer->toc_cfg.channel)
            { TIM_SetCompare4(timer->timer_periph, (*(uint32_t*)arg)); }
            break;
        default:
            break;
    }
    return DY_EOK;
}


oc_timer_dev_t g_timer8 = {
    .timer_periph = TIM8,
    .periph_clk = RCC_APB2Periph_TIM8,
    .tbase_cfg.clk_division = TIM_CKD_DIV1,
    .tbase_cfg.prescaler = (1680 - 1),      //10MHz
    .tbase_cfg.period = (1000 - 1),
    .tbase_cfg.counter_mode = TIM_CounterMode_Up,
    .tbase_cfg.repetition_counter = 0,
    .timer_mode = 1,        //互补输出
    /*主输出*/
    .gpio_m = GPIOC,
    .gpio_mpin = GPIO_Pin_6,
    .gpio_maf = GPIO_AF_TIM8,
    /*互补输出*/
    .gpio_n = GPIOA,
    .gpio_npin = GPIO_Pin_5,
    .gpio_naf = GPIO_AF_TIM8,
    .toc_cfg.channel = 1,
    .toc_cfg.oc_mode = TIM_OCMode_PWM1,
    .toc_cfg.output_state = TIM_OutputState_Enable,
    .toc_cfg.oc_polarity = TIM_OCPolarity_High,
    .toc_cfg.oc_idle_state = TIM_OCIdleState_Set,
    .toc_cfg.outputN_state = TIM_OutputNState_Enable,
    .toc_cfg.ocN_polarity = TIM_OCNPolarity_High,
    .toc_cfg.ocN_idle_state = TIM_OCNIdleState_Reset,
    .toc_cfg.pulse = 127,
    /*刹车配置*/
    .break_enable = true,
    .gpio_bk = GPIOA,
    .gpio_bkpin = GPIO_Pin_6,
    .gpio_bkaf = GPIO_AF_TIM8,
    .tbdtr_cfg.ossr_state = TIM_OSSRState_Enable,
    .tbdtr_cfg.ossi_state = TIM_OSSIState_Enable,
    .tbdtr_cfg.lock_level = TIM_LOCKLevel_1,
    .tbdtr_cfg.dead_time = 11,
    .tbdtr_cfg.break_enable = TIM_Break_Enable,
    .tbdtr_cfg.break_polarity = TIM_BreakPolarity_Low,
    .tbdtr_cfg.automatic_output_enable = TIM_AutomaticOutput_Enable,
    .tirq.irq_enable = false,
};

device_ops_t g_oc_timer_ops = {
    .init = oc_timer_init,
    .control = oc_timer_control
};

/********************************
 * 
 * LED_Red -> PF6 -> TIM10_CH1
 * LED_Green -> PF7 -> TIM11_CH1
 * LED_Blue -> PF8 -> TIM13_CH1
 * 
 ********************************/
#if (SINGLE_COLOR_PWM_LED)
/*测试: 输出PWM波, 控制灯光闪烁. 测试LED灯的初始化需要配置成AF*/
oc_timer_dev_t g_pwm_timer10 = {
    .timer_periph = TIM10,
    .periph_clk = RCC_APB2Periph_TIM10,
    .tbase_cfg.clk_division = TIM_CKD_DIV1,
    .tbase_cfg.prescaler = (1680 - 1),        //100 * 1000
    .tbase_cfg.period = (1000 - 1),           //1s中变化100次
    .tbase_cfg.counter_mode = TIM_CounterMode_Up,
    .tbase_cfg.repetition_counter = 0,
    .timer_mode = 0,        //输出PWM波
    /*主输出*/
    .gpio_m = GPIOF,
    .gpio_mpin = GPIO_Pin_6,
    .gpio_maf = GPIO_AF_TIM10,
    /*OC配置*/
    .toc_cfg.channel = 1,
    .toc_cfg.oc_mode = TIM_OCMode_PWM1,
    .toc_cfg.output_state = TIM_OutputState_Enable,
    .toc_cfg.oc_polarity = TIM_OCPolarity_Low,          //控制LED, 输出低电平
    .toc_cfg.oc_idle_state = TIM_OCIdleState_Set,
    .toc_cfg.pulse = 500,                               //占空比不能超过period
    /*刹车配置*/
    .break_enable = false,
    .tirq.irq_enable = false,
    .tirq.irq_channel = TIM1_UP_TIM10_IRQn,
    .tirq.group_priority = 0,
    .tirq.sub_priority = 3,
    .tirq.it_flags = TIM_FLAG_Update,             //更新中断
    .tirq.timerx_handler = base_timer_irq_handler //中断函数回调
};
#elif (FULL_COLOR_PWM_LED)
/*测试: 输出PWM波, 控制灯光闪烁. 测试LED灯的初始化需要配置成AF*/
oc_timer_dev_t g_pwm_timer10 = {
    .timer_periph = TIM10,
    .periph_clk = RCC_APB2Periph_TIM10,
    .tbase_cfg.clk_division = TIM_CKD_DIV1,
    .tbase_cfg.prescaler = (168000 - 1),        //1KHz
    .tbase_cfg.period = (256 - 1),              //
    .tbase_cfg.counter_mode = TIM_CounterMode_Up,
    .tbase_cfg.repetition_counter = 0,
    .timer_mode = 0,        //输出PWM波
    /*主输出*/
    .gpio_m = GPIOF,
    .gpio_mpin = GPIO_Pin_6,    //红灯
    .gpio_maf = GPIO_AF_TIM10,
    /*OC配置*/
    .toc_cfg.channel = 1,
    .toc_cfg.oc_mode = TIM_OCMode_PWM1,
    .toc_cfg.output_state = TIM_OutputState_Enable,
    .toc_cfg.oc_polarity = TIM_OCPolarity_Low,          //控制LED, 输出低电平
    .toc_cfg.oc_idle_state = TIM_OCIdleState_Set,
    .toc_cfg.pulse = 0,                               //占空比不能超过period
    /*刹车配置*/
    .break_enable = false,
    .tirq.irq_enable = true,
    .tirq.irq_channel = TIM1_UP_TIM10_IRQn,
    .tirq.group_priority = 0,
    .tirq.sub_priority = 3,
    .tirq.it_flags = TIM_FLAG_Update,             //更新中断
    .tirq.timerx_handler = base_timer_irq_handler //中断函数回调
};

/*测试: 输出PWM波, 控制灯光闪烁. 测试LED灯的初始化需要配置成AF*/
oc_timer_dev_t g_pwm_timer11 = {
    .timer_periph = TIM11,
    .periph_clk = RCC_APB2Periph_TIM11,
    .tbase_cfg.clk_division = TIM_CKD_DIV1,
    .tbase_cfg.prescaler = (168000 - 1),        //168 * 1000000 / 168 / 1000 = 1000
    .tbase_cfg.period = (256 - 1),
    .tbase_cfg.counter_mode = TIM_CounterMode_Up,
    .tbase_cfg.repetition_counter = 0,
    .timer_mode = 0,        //输出PWM波
    /*主输出*/
    .gpio_m = GPIOF,
    .gpio_mpin = GPIO_Pin_7,        //绿灯
    .gpio_maf = GPIO_AF_TIM11,
    /*OC配置*/
    .toc_cfg.channel = 1,
    .toc_cfg.oc_mode = TIM_OCMode_PWM1,
    .toc_cfg.output_state = TIM_OutputState_Enable,
    .toc_cfg.oc_polarity = TIM_OCPolarity_Low,          //控制LED, 输出低电平
    .toc_cfg.oc_idle_state = TIM_OCIdleState_Set,
    .toc_cfg.pulse = 0,                               //占空比不能超过period
    /*刹车配置*/
    .break_enable = false,
    .tirq.irq_enable = true,
    .tirq.irq_channel = TIM1_UP_TIM10_IRQn,
    .tirq.group_priority = 0,
    .tirq.sub_priority = 3,
    .tirq.it_flags = TIM_FLAG_Update,             //更新中断
    .tirq.timerx_handler = base_timer_irq_handler //中断函数回调
};

/*测试: 输出PWM波, 控制灯光闪烁. 测试LED灯的初始化需要配置成AF*/
oc_timer_dev_t g_pwm_timer13 = {
    .timer_periph = TIM13,
    .periph_clk = RCC_APB1Periph_TIM13,
    .tbase_cfg.clk_division = TIM_CKD_DIV1,
    .tbase_cfg.prescaler = (84000 - 1),        //84 * 1000000 / 84 / 1000 = 1000
    .tbase_cfg.period = (1000 - 1),
    .tbase_cfg.counter_mode = TIM_CounterMode_Up,
    .tbase_cfg.repetition_counter = 0,
    .timer_mode = 0,        //输出PWM波
    /*主输出*/
    .gpio_m = GPIOF,
    .gpio_mpin = GPIO_Pin_8,        //蓝灯
    .gpio_maf = GPIO_AF_TIM13,
    /*OC配置*/
    .toc_cfg.channel = 1,
    .toc_cfg.oc_mode = TIM_OCMode_PWM1,
    .toc_cfg.output_state = TIM_OutputState_Enable,
    .toc_cfg.oc_polarity = TIM_OCPolarity_Low,          //控制LED, 输出低电平
    .toc_cfg.oc_idle_state = TIM_OCIdleState_Set,
    .toc_cfg.pulse = 0,                               //占空比不能超过period
    /*刹车配置*/
    .break_enable = false,
    .tirq.irq_enable = true,
    .tirq.irq_channel = TIM1_UP_TIM10_IRQn,
    .tirq.group_priority = 0,
    .tirq.sub_priority = 3,
    .tirq.it_flags = TIM_FLAG_Update,             //更新中断
    .tirq.timerx_handler = base_timer_irq_handler //中断函数回调
};
#elif (TIMER2_OC_PWM)
/*测试: 输出PWM波, 控制灯光闪烁. 测试LED灯的初始化需要配置成AF*/
oc_timer_dev_t g_pwm_timer2 = {
    .timer_periph = TIM2,
    .periph_clk = RCC_APB1Periph_TIM2,
    .tbase_cfg.clk_division = TIM_CKD_DIV1,
    .tbase_cfg.prescaler = (840 - 1),        //100 * 1000
    .tbase_cfg.period = (10000 - 1),           //1s中变化100次
    .tbase_cfg.counter_mode = TIM_CounterMode_Up,
    .tbase_cfg.repetition_counter = 0,
    .timer_mode = 0,        //输出PWM波
    /*主输出*/
    .gpio_m = GPIOA,
    .gpio_mpin = GPIO_Pin_5,
    .gpio_maf = GPIO_AF_TIM2,
    /*OC配置*/
    .toc_cfg.channel = 1,
    .toc_cfg.oc_mode = TIM_OCMode_PWM1,
    .toc_cfg.output_state = TIM_OutputState_Enable,
    .toc_cfg.oc_polarity = TIM_OCPolarity_High,          //控制LED, 输出低电平
    //.toc_cfg.oc_idle_state = TIM_OCIdleState_Set,
    .toc_cfg.pulse = 3000 - 1,                               //占空比不能超过period
    /*刹车配置*/
    .break_enable = false,
    .tirq.irq_enable = false,
    .tirq.irq_channel = TIM1_UP_TIM10_IRQn,
    .tirq.group_priority = 0,
    .tirq.sub_priority = 3,
    .tirq.it_flags = TIM_FLAG_Update,               //更新中断
    .tirq.timerx_handler = NULL                     //中断函数回调
};
#endif

int drv_oc_timer_init()
{
    int ret = DY_EOK;

#if 0
    strncpy(g_timer8.device.name, "timer8", sizeof(g_timer8.device.name) - 1);
    g_timer8.device.ops = &g_oc_timer_ops;

    if (DY_EOK != dy_device_register(g_timer8.device.name, &g_timer8.device))
    {
        return DY_ERROR;
    }
    
    ret = g_timer8.device.ops->init(&g_timer8.device);
#endif

#if (SINGLE_COLOR_PWM_LED)
    /*定时器10-红灯*/
    strncpy(g_pwm_timer10.device.name, "timer10", sizeof(g_pwm_timer10.device.name) - 1);
    g_pwm_timer10.device.ops = &g_oc_timer_ops;
    if (DY_EOK != dy_device_register(g_pwm_timer10.device.name, &g_pwm_timer10.device))
    {
        return DY_ERROR;
    }
    ret = g_pwm_timer10.device.ops->init(&g_pwm_timer10.device);
#elif (FULL_COLOR_PWM_LED)
    /*定时器10-红灯*/
    strncpy(g_pwm_timer10.device.name, "timer10", sizeof(g_pwm_timer10.device.name) - 1);
    g_pwm_timer10.device.ops = &g_oc_timer_ops;
    if (DY_EOK != dy_device_register(g_pwm_timer10.device.name, &g_pwm_timer10.device))
    {
        return DY_ERROR;
    }
    ret = g_pwm_timer10.device.ops->init(&g_pwm_timer10.device);

    /*定时器11-绿灯*/
    strncpy(g_pwm_timer11.device.name, "timer11", sizeof(g_pwm_timer11.device.name) - 1);
    g_pwm_timer11.device.ops = &g_oc_timer_ops;
    if (DY_EOK != dy_device_register(g_pwm_timer11.device.name, &g_pwm_timer11.device))
    {
        return DY_ERROR;
    }   
    ret = g_pwm_timer11.device.ops->init(&g_pwm_timer11.device);

    /*定时器13-蓝灯*/
    strncpy(g_pwm_timer13.device.name, "timer13", sizeof(g_pwm_timer13.device.name) - 1);
    g_pwm_timer13.device.ops = &g_oc_timer_ops;
    if (DY_EOK != dy_device_register(g_pwm_timer13.device.name, &g_pwm_timer13.device))
    {
        return DY_ERROR;
    }
    ret = g_pwm_timer13.device.ops->init(&g_pwm_timer13.device);
#elif (TIMER2_OC_PWM)
    /******************************
     * TIM2: 输出PWM波形
     *******************************/
    strncpy(g_pwm_timer2.device.name, "timer2", sizeof(g_pwm_timer2.device.name) - 1);
    g_pwm_timer2.device.ops = &g_oc_timer_ops;
    if (DY_EOK != dy_device_register(g_pwm_timer2.device.name, &g_pwm_timer2.device))
    {
        return DY_ERROR;
    }
    ret = g_pwm_timer2.device.ops->init(&g_pwm_timer2.device);
#endif

    return DY_EOK;
}

/**********************************************************************
 * 
 *                              IC定时器配置
 * 输入捕获: 测量周期, 测量占空比
 * 选择模式进行初始化
 * 
 ***********************************************************************/
static int ic_timer_init(dy_device_t *dev)
{
    ic_timer_dev_t *timer = (ic_timer_dev_t*)dev;

    /*GPIO输出*/
    GPIO_InitTypeDef stGPIOInit;

    /*基本配置*/
    TIM_TimeBaseInitTypeDef stTimerBaseInit;
    /*输入捕获*/
    TIM_ICInitTypeDef stTimerICInit;

    /*时钟*/
    drv_gpio_enable_clk((uint32_t)(timer->gpio_m));
    /*复用*/
    GPIO_PinAFConfig(timer->gpio_m, drv_gpio_get_af_src(timer->gpio_mpin), timer->gpio_maf);
    /*基本配置*/
    stGPIOInit.GPIO_Mode = GPIO_Mode_AF;
    stGPIOInit.GPIO_OType = GPIO_OType_PP;
    stGPIOInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
    stGPIOInit.GPIO_Speed = GPIO_Speed_100MHz;
    stGPIOInit.GPIO_Pin = timer->gpio_mpin;
    GPIO_Init(timer->gpio_m, &stGPIOInit);
    
    /*定时器时钟启用*/
    timer_APB_clk_enable_v1(timer->timer_periph, timer->periph_clk);
    stTimerBaseInit.TIM_ClockDivision = timer->tbase_cfg.clk_division;              //时钟分频
    stTimerBaseInit.TIM_Prescaler = timer->tbase_cfg.prescaler;                     //168MHz / 168 = 1MHz / 10 = 100 * 1KHz
    stTimerBaseInit.TIM_Period = timer->tbase_cfg.period;                           //1s 100次重载. 1次重载10ms
    stTimerBaseInit.TIM_CounterMode = timer->tbase_cfg.counter_mode;                //计数模式
    stTimerBaseInit.TIM_RepetitionCounter = timer->tbase_cfg.repetition_counter;    //不进行重复计数
    TIM_TimeBaseInit(timer->timer_periph, &stTimerBaseInit);

    /*捕获输入: 两路, 一路测量周期, 一路测量占空比. 配置一路即可, 另一路硬件自带设置*/
    stTimerICInit.TIM_Channel = timer->tic_cfg.channel;            //通道
    stTimerICInit.TIM_ICPolarity = timer->tic_cfg.ic_polarity;     //捕获边沿
    stTimerICInit.TIM_ICSelection = timer->tic_cfg.ic_selection;   //捕获信号输入
    stTimerICInit.TIM_ICPrescaler = timer->tic_cfg.ic_prescaler;   //捕获信号的每个有效边沿都捕获
    stTimerICInit.TIM_ICFilter = timer->tic_cfg.ic_filter;         //不滤波
    TIM_PWMIConfig(timer->timer_periph, &stTimerICInit);

    if (1)
    {
        /*捕获输入: 两路, 一路测量周期, 一路测量占空比. 配置一路即可, 另一路硬件自带设置*/
        stTimerICInit.TIM_Channel = TIM_Channel_2;                  //通道
        stTimerICInit.TIM_ICPolarity = TIM_ICPolarity_Falling;      //捕获边沿
        stTimerICInit.TIM_ICSelection = TIM_ICSelection_IndirectTI; //捕获信号输入
        stTimerICInit.TIM_ICPrescaler = TIM_ICPSC_DIV1;             //捕获信号的每个有效边沿都捕获
        stTimerICInit.TIM_ICFilter = 0x00;                          //不滤波
        TIM_PWMIConfig(timer->timer_periph, &stTimerICInit);
    }

    /*输入捕获的触发信号*/
    TIM_SelectInputTrigger(timer->timer_periph, timer->input_trigger_single);
    //TIM_SelectInputTrigger(timer->timer_periph, TIM_TS_TI1FP1);

    /*PWM输入模式, 选择[从模式-复位模式], 当捕获开始时,计数器CNT会被复位*/
    TIM_SelectSlaveMode(timer->timer_periph, timer->slave_mode);  //复位, 门, 触发, 外部
    if (TIM_SlaveMode_Reset == timer->slave_mode)
    {
        TIM_SelectMasterSlaveMode(timer->timer_periph, TIM_MasterSlaveMode_Enable);
    }
    //TIM_SelectSlaveMode(timer->timer_periph, TIM_SlaveMode_Reset);
    //TIM_SelectMasterSlaveMode(timer->timer_periph, TIM_MasterSlaveMode_Enable);

    /*中断*/
    if (timer->tirq.irq_enable)
    {
        /*使能中断标志位*/
        TIM_ITConfig(timer->timer_periph, timer->tirq.it_flags, ENABLE);

        /*使能NVIC中断号*/
        NVIC_InitTypeDef stNVICInit = {0};
        stNVICInit.NVIC_IRQChannel = timer->tirq.irq_channel;   //中断号
        stNVICInit.NVIC_IRQChannelPreemptionPriority = timer->tirq.group_priority; //抢占优先级
        stNVICInit.NVIC_IRQChannelSubPriority = timer->tirq.sub_priority;          //子优先级
        stNVICInit.NVIC_IRQChannelCmd = ENABLE;                 //使能
        NVIC_Init(&stNVICInit);
    }

    /*使能定时器*/
    TIM_Cmd(timer->timer_periph, ENABLE);

    return DY_EOK;
}

static int ic_timer_control(dy_device_t *dev, int cmd, void *arg)
{
    ic_timer_dev_t *timer = (ic_timer_dev_t*)dev;
    switch(cmd)
    {
        case ICTIMER_START:          //启动
            TIM_Cmd(timer->timer_periph, ENABLE);
            break;
        case ICTIMER_STOP:           //停止
            TIM_Cmd(timer->timer_periph, DISABLE);
            break;
        case ICTIMER_EN_IRQ:         //中断使能
            TIM_ClearFlag(timer->timer_periph, TIM_FLAG_Update);
            TIM_ITConfig(timer->timer_periph, TIM_IT_Update, ENABLE);
            /*还要配置NVIC*/
            break;
        case ICTIMER_DN_IRQ:         //关闭中断
            TIM_ITConfig(timer->timer_periph, TIM_IT_Update, DISABLE);
            /*还要配置NVIC*/
            break;
        case ICTIMER_EN_DMA:         //使能DMA
            TIM_DMACmd(timer->timer_periph, TIM_DMA_Update, ENABLE);
            /*还要配置DMA*/
            break;
        case ICTIMER_DN_DMA:         //关闭DMA
            /*还要关闭DMA*/
            TIM_DMACmd(timer->timer_periph, TIM_DMA_Update, DISABLE);
            break;
        default:
            break;
    }
    return DY_EOK;
}

ic_timer_dev_t g_ic_timer8 = {
    .timer_periph = TIM8,
    .periph_clk = RCC_APB2Periph_TIM8,
    .tbase_cfg.clk_division = TIM_CKD_DIV1,
    .tbase_cfg.prescaler = (1680 - 1),
    .tbase_cfg.period = (0xFFFF - 1),
    .tbase_cfg.counter_mode = TIM_CounterMode_Up,
    .tbase_cfg.repetition_counter = 0,
    /*主输出*/
    .gpio_m = GPIOC,
    .gpio_mpin = GPIO_Pin_6,
    .gpio_maf = GPIO_AF_TIM8,
    .tic_cfg.channel = TIM_Channel_1,
    .tic_cfg.ic_polarity = TIM_ICPolarity_Rising,
    .tic_cfg.ic_selection = TIM_ICSelection_DirectTI,
    .tic_cfg.ic_prescaler = TIM_ICPSC_DIV1,
    .tic_cfg.ic_filter = 0x0,
    /*输入触发信号*/
    .input_trigger_single = TIM_TS_TI1FP1,
    /*从模式*/
    .slave_mode = TIM_SlaveMode_Reset,
    /*中断配置*/
    .tirq.irq_enable = true,
    .tirq.irq_channel = TIM8_CC_IRQn,
    .tirq.group_priority = 0,
    .tirq.sub_priority = 3,
    .tirq.it_flags = TIM_IT_CC1,   //更新中断
    .tirq.timerx_handler = NULL //中断函数回调
};

device_ops_t g_ic_timer_ops = {
    .init = ic_timer_init,
    .control = ic_timer_control
};

int drv_ic_timer_init()
{
    int ret = DY_EOK;
    strncpy(g_ic_timer8.device.name, "timer8", sizeof(g_ic_timer8.device.name) - 1);
    g_ic_timer8.device.ops = &g_ic_timer_ops;

    if (DY_EOK != dy_device_register(g_ic_timer8.device.name, &g_ic_timer8.device))
    {
        return DY_ERROR;
    }
    
    ret = g_ic_timer8.device.ops->init(&g_ic_timer8.device);

    return DY_EOK;
}

