#include "gd32f30x.h"
#include "gd32f30x_it.h"
#include "system_mng.h"

#define HW32_REG(ADDRESS) (*((volatile unsigned long *)(ADDRESS)))

/****************************************************
 * TCB块设计
 *      任务入口
 *      栈
 *      状态
 *      优先级
 * 任务调度
 *      简单任务调度: 检查任务运行时间是否满足5s, 满足则切换.
 * 上下文切换
 *      把当前任务的上下文存储起来
 *      获取下一个任务, 切换到下一个任务的上下文
 *****************************************************/
#define TASK_COUNT 4

typedef enum
{
    TASK_READY,         //就绪
    TASK_RUNNING,       //运行
    TASK_SUSPENDED,     //挂起
}task_state_t;

typedef struct
{
    void (*entry)(void);
    uint32_t stack_ptr;
    uint32_t stack_size;
    uint32_t psp_ptr;
    uint32_t state;
    uint32_t runing_time;   //运行时间
}tcb_t;

tcb_t tasks[4];

long long task0_stack[1024], task1_stack[1024], task2_stack[1024], task3_stack[1024];

uint32_t curr_task_id;
uint32_t next_task_id;

void LED_initialize(void);
void led_green_on();
void led_green_off();
void led_blue_on();
void led_blue_off();
void task_init();
/* 进入低功耗模式 */
void low_power_enter(void);
/* 退出低功耗模式 */
void low_power_exit(void);


void task0(void);
void task1(void);
void task2(void);
void task3(void);


int main_os(void)
{
    /*双字栈对齐的意义*/
    SCB->CCR |= SCB_CCR_STKALIGN_Msk;       //使能双字栈对齐
    LED_initialize();

    /*任务栈初始化*/
    task_init();

    curr_task_id = 0;
    __set_PSP(tasks[curr_task_id].psp_ptr + 16 * 4);
    /*挂起中断设置最低*/
    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    systick_config();

    /*使用PSP作为栈*/
    __set_CONTROL(0x3);
    __ISB();
    task0();

    while(1)
    {
        printf("\r\nos error\r\n");
    }

    return 0;
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

/* 进入低功耗模式 */
void low_power_enter(void)
{
    /* 简化实现：这里只是示意，实际需要更复杂的低功耗实现 */
    /* 低功耗模式需要配置PMU模块，这里占位 */
}

/* 退出低功耗模式 */
void low_power_exit(void)
{
    /* 低功耗退出逻辑 */
    /* 低功耗退出通常由中断唤醒，这里占位 */
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

void task_init()
{
    tasks[0].entry = task0;
    tasks[0].stack_ptr = ((unsigned int )task0_stack);
    tasks[0].stack_size = sizeof(task0_stack);
    tasks[0].psp_ptr = ((unsigned int )task0_stack) + (sizeof task0_stack) - 16 * 4;    //psp_ptr的指针指向的是16个寄存器的末尾, 也就说如果任务的栈想压数据进去, 从这里开始
    tasks[0].state = TASK_READY;
    HW32_REG((tasks[0].psp_ptr + (14 << 2))) = (unsigned long)task0;  //xPSR, PC, LR, R12, R3, R2, R1, R0, R4-R11. R13是SP指针 
    HW32_REG((tasks[0].psp_ptr + (15 << 2))) = 0x01000000;            //初始化xPSR指针
    

    tasks[1].entry = task1;
    tasks[1].stack_ptr = ((unsigned int )task1_stack);
    tasks[1].stack_size = sizeof(task1_stack);
    tasks[1].psp_ptr = ((unsigned int )task1_stack) + (sizeof task1_stack) - 16 * 4;
    tasks[1].state = TASK_READY;
    HW32_REG((tasks[1].psp_ptr + (14 << 2))) = (unsigned long)task1;  //xPSR, PC, LR, R12, R3, R2, R1, R0, R4-R11. R13是SP指针 
    HW32_REG((tasks[1].psp_ptr + (15 << 2))) = 0x01000000;            //初始化xPSR指针

    tasks[2].entry = task2;
    tasks[2].stack_ptr = ((unsigned int )task2_stack);
    tasks[2].stack_size = sizeof(task2_stack);
    tasks[2].psp_ptr = ((unsigned int )task2_stack) + (sizeof task2_stack) - 16 * 4;
    tasks[2].state = TASK_READY;
    HW32_REG((tasks[2].psp_ptr + (14 << 2))) = (unsigned long)task2;  //xPSR, PC, LR, R12, R3, R2, R1, R0, R4-R11. R13是SP指针 
    HW32_REG((tasks[2].psp_ptr + (15 << 2))) = 0x01000000;            //初始化xPSR指针

    tasks[3].entry = task3;
    tasks[3].stack_ptr = ((unsigned int )task3_stack);
    tasks[3].stack_size = sizeof(task3_stack);
    tasks[3].psp_ptr = ((unsigned int )task3_stack) + (sizeof task3_stack) - 16 * 4;
    tasks[3].state = TASK_READY;
    HW32_REG((tasks[3].psp_ptr + (14 << 2))) = (unsigned long)task3;  //xPSR, PC, LR, R12, R3, R2, R1, R0, R4-R11. R13是SP指针 
    HW32_REG((tasks[3].psp_ptr + (15 << 2))) = 0x01000000;            //初始化xPSR指针
}

void task_scheduler()
{
    static uint32_t systick_count = 0;
    systick_count++;
    if (systick_count % 5000 == 0)
	{
        next_task_id = (curr_task_id + 1) % TASK_COUNT;
        printf("curr task id = %d, next task id = %d\r\n", curr_task_id, next_task_id);
        if (curr_task_id != next_task_id)
        {
            SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;    //触发切换上下文
        }
    }
}

__asm void PendSV_Handler(void)
{
    /*保存上下文*/
    MRS R0, PSP                     //将当前进程栈的值加载到R0中: PSP此时是指向任务栈的
    STMDB R0!, {R4-R11}             //把R4-R11压栈(此时, xPSR, PC, LD, R12, R3, R2, R1, R0已经入栈了)
    LDR R1, = __cpp(&curr_task_id)    //获取curr_task_id的地址
    LDR R2, [R1]                    //获取curr_task_id的值
    LDR R3, = __cpp(&tasks)           //获取tasks的地址
    MOV R4, #24                     //
    MUL R5, R2, R4                  //将R2的值乘以24字节, 计算偏移: 一个任务是24字节
    ADD R3, R3, R5                  //tasks + 24 * curr_task_id, 值放在R3中
    ADD R3, R3, #12                 //&tasks[curr_task_id] + 12字节, 得到psp_ptr的位置
    STR R0, [R3]                    //把值存在psp_ptr中

    /*加载上下文*/
    LDR R4, = __cpp(&next_task_id)    //
    LDR R4, [R4]                    //将next_task_id的值赋给R4
    STR R4, [R1]                    //将next_task_id的值存到curr_task_id中
    LDR R3, = __cpp(&tasks)           //获取tasks的地址
    MOV R5, #24                     //
    MUL R6, R4, R5                  //将R4的值乘以24字节, 计算偏移: 一个任务是24字节
    ADD R3, R3, R6                  //tasks + 24 * next_task_id, 值放在R3中
    ADD R3, R3, #12                 //&tasks[next_task_id] + 12字节, 得到psp_ptr的位置
    LDR R0, [R3]                    //把tasks[next_task_id].psp_str的值加载进来
    LDMIA R0!, {R4-R11}             //把栈后续的内容加载到寄存器中
    
    MSR PSP, R0
    MOV LR, #0xFFFFFFFD
    BX LR
    ALIGN 4
}

