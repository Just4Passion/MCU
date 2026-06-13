#include "gd32f30x.h"
#include "gd32f30x_it.h"
#include "system_mng.h"

/****************************************************
 * 每个任务有一个独立的栈
 * 使用PSP作为栈指针
 * 在PendSV中实现任务上下文切换
 * 在SysTick中触发任务上下文切换
 * 
 *****************************************************/

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

volatile uint32_t systick_count = 0;

//每个任务使用的栈
long long task0_stack[1024], task1_stack[1024], task2_stack[1024], task3_stack[1024];

//OS使用的数据
uint32_t curr_task = 0;     //当前任务
uint32_t next_task = 1;     //下一个任务
uint32_t PSP_array[4];      //每个任务的进程栈指针

int main_os(void)
{
    /*双字栈对齐的意义*/
    SCB->CCR |= SCB_CCR_STKALIGN_Msk;       //使能双字栈对齐
    LED_initialize();

    /*开始任务调度*
    /*创建任务0的栈帧*/ 
    task_stack_init();

    /*配置PSP指针*/
    curr_task = 0;  //当前任务
    __set_PSP((PSP_array[curr_task] + 16 * 4));   //设置进程栈指针
    NVIC_SetPriority(PendSV_IRQn, 0xFF);        //把这个中断配置成最低
    systick_config();

    /*切换环境, 执行任务*/
    __set_CONTROL(0x3);                         //切换到进程栈, 非特权状态, 此时用的是PSP指针
    __ISB();                                    //修改CONTROL后执行ISB(架构推荐)
    task0();                                    //开始任务0
    while(1)
    {
        printf("\r\nos error\r\n");
    }

    return 0;
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

void task_stack_init()
{
    /*创建任务0的栈帧*/ 
    PSP_array[0] = ((unsigned int )task0_stack) + (sizeof task0_stack) - 16 * 4;    //16个寄存器
    HW32_REG((PSP_array[0] + ((14 << 2)))) = (unsigned long)task0;  //初始化程序计数器
    HW32_REG((PSP_array[0] + (15 << 2))) = 0x01000000;              //初始化xPSR
    
    /*创建任务1的栈帧*/
    PSP_array[1] = ((unsigned int )task1_stack) + (sizeof task1_stack) - 16 * 4;    //16个寄存器
    HW32_REG((PSP_array[1] + ((14 << 2)))) = (unsigned long)task1;   //初始化程序计数器
    HW32_REG((PSP_array[1] + (15 << 2))) = 0x01000000;               //初始化xPSR

    /*创建任务2的栈帧*/
    PSP_array[2] = ((unsigned int )task2_stack) + (sizeof task2_stack) - 16 * 4;    //16个寄存器
    HW32_REG((PSP_array[2] + ((14 << 2)))) = (unsigned long)task2;   //初始化程序计数器
    HW32_REG((PSP_array[2] + (15 << 2))) = 0x01000000;               //初始化xPSR

    /*创建任务3的栈帧*/
    PSP_array[3] = ((unsigned int )task3_stack) + (sizeof task3_stack) - 16 * 4;    //16个寄存器
    HW32_REG((PSP_array[3] + ((14 << 2)))) = (unsigned long)task3;   //初始化程序计数器
    HW32_REG((PSP_array[3] + (15 << 2))) = 0x01000000;               //初始化xPSR
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


__asm void PendSV_Handler(void)
{
    #if 1
    MRS R0, PSP                     //读取当前进程栈指针数值
    STMDB R0!, {R4-R11}             //将R4~R11保存到任务栈中(8个寄存器)
    LDR R1, = __cpp(&curr_task)
    LDR R2, [R1]                    
    LDR R3, = __cpp(&PSP_array)
    STR R0, [R3, R2, LSL #2]        //将PSP数值保存到PSP_array

    /*加载下一个上下文*/
    LDR R4, = __cpp(&next_task)
    LDR R4, [R4]                    //得到下个任务ID
    STR R4, [R1]
    LDR R0, [R3, R4, LSL #2]        //从PSP_array加载PSP
    LDMIA R0!, {R4-R11}             //从任务栈中加载R4~R11
    MSR PSP, R0
    
    MOV LR, #0xFFFFFFFD
    BX LR                           //从中返回: 但是PC和LR变了吗？退出后自动填充? 
    ALIGN 4
    #endif
}

/* 调用任务调度器 */
void task_scheduler()
{
	extern uint32_t curr_task;
	extern uint32_t next_task;
	extern volatile uint32_t systick_count;
	systick_count++;

	if (systick_count % 5000 == 0)
	{
		switch (curr_task)
		{
			case (0):
				next_task = 1; break;
			case (1):
				next_task = 2; break;
			case (2):
				next_task = 3; break;
			case (3):
				next_task = 0; break;
			default:
				next_task = 0;
				break;
		}
		printf("\r\nsystick_count = %u, curr_task = %u, next_task = %u\r\n", 
			systick_count, curr_task, next_task);

		if (curr_task != next_task)
		{
			SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
		}
	}
}


