

#include <rthw.h>
#include <rtthread.h>

#include "gd32f4xx.h"


#define THREAD_PRIORITY         25
#define THREAD_STACK_SIZE       256
#define THREAD_TIMESLICE        5
static void thread_debug_deal_entry(void *parameter)
{
    rt_uint32_t count = 0;

    while (1)
    {
        /* 线程1采用低优先级运行，一直打印计数值 */
		debug_deal();
        rt_thread_mdelay(50);
    }
}

void rt_application_init()
{
	static char thread2_stack[1024];
	static struct rt_thread thread2;
	/*启动一个debug线程*/
	static rt_thread_t tid1 = RT_NULL;
	tid1 = rt_thread_create("thread1",
		thread_debug_deal_entry, 
		RT_NULL,
		THREAD_STACK_SIZE,
		THREAD_PRIORITY, 
		THREAD_TIMESLICE);

	/* 如果获得线程控制块，启动这个线程 */
	if (tid1 != RT_NULL)
	{
		rt_thread_startup(tid1);
		printf("\r\n==============create thread1 success\r\n");
	}
	else
	{
		printf("\r\n==============create thread1 failed\r\n");
	}

	#if 0
	rt_thread_init(&thread2,
		"thread2",
		thread_debug_deal_entry,
		RT_NULL,
		&thread2_stack[0],
		sizeof(thread2_stack),
		THREAD_PRIORITY - 1, THREAD_TIMESLICE);
	rt_thread_startup(&thread2);
	#endif
}

#define GD32_SRAM_SIZE         96
#define GD32_SRAM_END          (0x20000000 + GD32_SRAM_SIZE * 1024)
/*************************
	Image：表示程序镜像（可执行文件）
	RW_IRAM1：内存区域名称（在分散加载文件中定义）
	ZI：Zero Initialized 区域（未初始化的全局/静态变量）
	Limit：该区域的结束地址（上限）

extern unsigned long Image$$RW_IRAM1$$RO$$Limit;    // RO 区域结束
extern unsigned long Image$$RW_IRAM1$$RW$$Base;     // RW 区域开始  
extern unsigned long Image$$RW_IRAM1$$RW$$Limit;    // RW 区域结束
extern unsigned long Image$$RW_IRAM1$$ZI$$Base;     // ZI 区域开始
extern unsigned long Image$$RW_IRAM1$$ZI$$Limit;    // ZI 区域结束

内存布局示例：
+-------------------+ 低地址
|   RO（代码/常量）  |
+-------------------+
|   RW（已初始化数据）|
+-------------------+
|   ZI（未初始化数据）| ← Image$$RW_IRAM1$$ZI$$Base
|                   |
|                   | ← Image$$RW_IRAM1$$ZI$$Limit
+-------------------+
|       堆(Heap)     |
+-------------------+
|       栈(Stack)    |
+-------------------+ 高地址


Stack_Size      EQU     0x00002800		//10KB
Heap_Size       EQU     0x0001C500		//113KB

***************************/
extern int Image$$RW_IRAM1$$ZI$$Limit;
#define HEAP_BEGIN    (&Image$$RW_IRAM1$$ZI$$Limit)
#define HEAP_END          GD32_SRAM_END			//96KB, 栈空间是10KB, 总空间是112KB, 没有重叠
int main()
{
	rt_size_t total, used, max_used;
	/*第一步为什么是禁用中断*/
	rt_hw_interrupt_disable();

	/*硬件初始化 - 初始化启用Systick, USART, GPIO*/
	SysTick_Config(SystemCoreClock / RT_TICK_PER_SECOND);	//滴答时钟
	NVIC_SetPriority(SysTick_IRQn, 0);						//设置Systick中断优先级
	debug_init();											//usart初始化:包括串口驱动初始化

	rt_system_heap_init((void *)HEAP_BEGIN, (void *)HEAP_END);

	rt_system_timer_init();									//启动系统定时器
	rt_system_scheduler_init();                             //初始化系统调度器

	/*APP初始化*/
	rt_application_init();
	rt_memory_info(&total, &used, &max_used);
	
	rt_system_timer_thread_init();                          //初始化系统定时器线程
	rt_thread_idle_init();                                  //初始化空闲线程
	rt_thread_defunct_init();                              //初始化错误处理线程
	rt_system_scheduler_start();



	return 0;
}



