#include "gd32f30x.h"
#include "gd32f30x_it.h"
#include "system_mng.h"


/**************************
 * 
 * 扩展上下文切换支持浮点寄存器并混用特权和非特权任务
 *      关注特权和非特权任务的切换 - 特权任务也需要一个栈
 *      增加CONTROL和EXC_RETURN的记录
 * 
 ****************************/
/*字访问的宏定义*/
#define HW32_REG(ADDRESS) (*((volatile unsigned long *)(ADDRESS)))


/*OS切换任务经典实现*/
/*红灯PA6, 绿灯PA7*/

void LED_initialize(void);
void led_green_on();
void led_green_off();
void led_blue_on();
void led_blue_off();
void task_stack_init();
void task0(void);
void task1(void);
void task2(void);
void task3(void);

void __svc(0x00) os_start(void);

//发送给任务的事件
volatile uint32_t systick_count = 0;
//每个任务用的栈
long long task0_stack[1024], task1_stack[1024], task2_stack[1024], task3_stack[1024];
//OS使用的数据
uint32_t curr_task = 0;     //当前任务
uint32_t next_task = 1;     //下一个任务
uint32_t PSP_array[4];      //每个任务的进程栈指针
uint32_t svc_exc_return;    //SVC用的EXC_RETRUN

int main_os(void)
{
    SCB->CCR |= SCB_CCR_STKALIGN_Msk;       //使能双子栈对齐
    LED_initialize();
    os_start();
    while(1)
    {
        printf("\r\nos error\r\n");
    }
    return 0;
}

void task_stack_init()
{
    /*创建任务0的栈帧*/ 
    PSP_array[0] = ((unsigned int )task0_stack) + (sizeof task0_stack) - 18 * 4;    //18个寄存器
    HW32_REG((PSP_array[0] + ((16 << 2)))) = (unsigned long)task0;  //初始化程序计数器
    HW32_REG((PSP_array[0] + (17 << 2))) = 0x01000000;              //初始化xPSR
    HW32_REG((PSP_array[0])) = 0xFFFFFFFD;                          //返回时使用PSP
    HW32_REG((PSP_array[0] + (1 << 2))) = 0X3;                      //CONTROL使用PSP
    
    /*创建任务1的栈帧*/
    PSP_array[1] = ((unsigned int )task1_stack) + (sizeof task1_stack) - 18 * 4;    //18个寄存器
    HW32_REG((PSP_array[1] + ((16 << 2)))) = (unsigned long)task1;   //初始化程序计数器
    HW32_REG((PSP_array[1] + (17 << 2))) = 0x01000000;               //初始化xPSR
    HW32_REG((PSP_array[1])) = 0xFFFFFFFD;                           //返回时使用PSP
    HW32_REG((PSP_array[1] + (1 << 2))) = 0X3;                       //CONTROL使用PSP

    /*创建任务2的栈帧*/
    PSP_array[2] = ((unsigned int )task2_stack) + (sizeof task2_stack) - 18 * 4;    //18个寄存器
    HW32_REG((PSP_array[2] + ((16 << 2)))) = (unsigned long)task2;   //初始化程序计数器
    HW32_REG((PSP_array[2] + (17 << 2))) = 0x01000000;               //初始化xPSR
    HW32_REG((PSP_array[2])) = 0xFFFFFFFD;                           //返回时使用PSP
    HW32_REG((PSP_array[2] + (1 << 2))) = 0X3;                       //CONTROL使用PSP

    /*创建任务3的栈帧*/
    PSP_array[3] = ((unsigned int )task3_stack) + (sizeof task3_stack) - 18 * 4;    //18个寄存器
    HW32_REG((PSP_array[3] + ((16 << 2)))) = (unsigned long)task3;   //初始化程序计数器
    HW32_REG((PSP_array[3] + (17 << 2))) = 0x01000000;               //初始化xPSR
    HW32_REG((PSP_array[3])) = 0xFFFFFFFD;                           //返回时使用PSP
    HW32_REG((PSP_array[3] + (1 << 2))) = 0X3;                       //CONTROL使用PSP
}

__asm void PendSV_Handler(void)
{
    /*上下文切换, 保存当前上下文*/
    MRS R0, PSP
    TST LR, #0x10
    IT EQ
    VSTMDBEQ R0!, {S16-S31}
    MOV R2, LR
    MRS R3, CONTROL
    STMDB R0!, {R2-R11}             //保存LR, CONTROL, R4~R11到任务栈(10个寄存器)
    LDR R1, = __cpp(&curr_task);    
    LDR R2, [R1]                    //获取当前任务ID
    LDR R3, = __cpp(&PSP_array);
    STR R0, [R3, R2, LSL #2]        //保存PSP数值到PSP_array

    /*加载下一个上下文*/
    LDR R4, = __cpp(&next_task)
    LDR R4, [R4]                    //获得下一个任务ID
    STR R4, [R1]                    //设置curr_task = next_task
    LDR R0, [R3, R4, LSL #2]        //从PSP_array中加载PSP
    LDMIA R0!, {R2-R11}             //从进程栈加载LR, CONTROL以及R4~R11(10个寄存器)
    MOV LR, R2
    MSR CONTROL, R3
    ISB
    TST LR, #0x10                   //测试第4位, 若为0, 则需要出栈浮点寄存器
    IT EQ
    VLDMIAEQ R0!, {S16-S31}         //加载浮点寄存器
    MSR PSP, R0                     //设置PSP为下一个任务
    BX LR
    ALIGN 4
}

void SVC_Handler_C(unsigned int *svc_args)
{
    uint8_t svc_number;
    svc_number = ((char *)svc_args[6])[-2];
    switch (svc_number)
    {
        case 0:
            /*栈帧初始化*/
            task_stack_init();

            curr_task = 0;
            svc_exc_return = HW32_REG((PSP_array[curr_task]));

            /*返回到线程, 使用PSP, 设置PSP为@R0*/
            __set_PSP((PSP_array[curr_task] + 10 * 4));  //XPSR, PC, LR, R12, R3, R2, R1, R0
            /*设置最低优先级*/
            NVIC_SetPriority(PendSV_IRQn, 0xFF);
            systick_config();
            
            /*切换环境, 执行任务*/
            __set_CONTROL(0x3);                         //切换到进程栈, 非特权状态, 此时用的是PSP指针
            __ISB();                                    //修改CONTROL后执行ISB(架构推荐)
            break;
        default:
            printf("- SVC number 0x%x\r\n", svc_number);
            break;
    }
}

/**************************
 * 
 * 使用特权模式, 完成栈初始化, 任务启动
 * 
 * 
 ****************************/
__asm void SVC_Handler1(void)
{
    TST LR, #4          //001, 提去栈栈位置
    ITE EQ
    MRSEQ R0, MSP
    MRSNE R0, PSP

    LDR R1, = __cpp(&svc_exc_return)
    STR LR, [R1]
    BL __cpp(SVC_Handler_C)
    LDR R1, = __cpp(&svc_exc_return)
    LDR LR, [R1]
    BX LR

    ALIGN 4
}

void task0(void)
{
    while(1)
    {
        led_blue_off();
        led_green_on();
    }
}

void task1(void)
{
    while(1)
    {
        led_green_off();
        led_blue_on();
    }
}

void task2(void)
{
    while(1)
    {
        led_blue_on();
        led_green_on();
    }
}

void task3(void)
{
    while(1)
    {
        led_green_off();
        led_blue_off();
    }
}

void LED_initialize(void)
{
    /* 使能GPIOE时钟 */
    rcu_periph_clock_enable(RCU_GPIOA);
    
    /* 配置PA6为推挽输出模式(绿灯) */
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);  //高电平亮灯
    /* 配置PA7为推挽输出模式(红灯) */
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);

}

void led_green_on()
{
    gpio_bit_set(GPIOA, GPIO_PIN_6);
}
void led_green_off()
{
    gpio_bit_reset(GPIOA, GPIO_PIN_6);
}
void led_blue_on()
{
    gpio_bit_set(GPIOA, GPIO_PIN_7);
}
void led_blue_off()
{
    gpio_bit_reset(GPIOA, GPIO_PIN_7);
}
