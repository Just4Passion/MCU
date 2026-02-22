#ifndef RT_CONFIG_H__
#define RT_CONFIG_H__

/***********************************************************
 * 
 *          最小内核配置: 纳米级内核
 * 多线程调度（支持优先级抢占）
 * 线程间同步：信号量、互斥锁
 * 线程间通信：邮箱、消息队列
 * 内存管理：小内存管理算法
 * 软件定时器
 * 
 ***********************************************************/
#define RT_USING_NANO
#define RT_CPUS_NR 1
#define RT_NAME_MAX 8
#define RT_ALIGN_SIZE 4
#define RT_USING_OVERFLOW_CHECK             //栈溢出检查
#define RT_TICK_PER_SECOND 1000             //1秒钟多少个tick
#define RT_THREAD_PRIORITY_MAX 32           //任务优先级
#define IDLE_THREAD_STACK_SIZE 256          //空闲线程栈
#define RT_BACKTRACE_LEVEL_MAX_NR 32        //回溯信息最大层级. 定义函数调用栈回溯的最大层级数; 用于调试功能，如异常时的调用栈打印

//#define RT_USING_IPC                        // 启用IPC功能
#define RT_USING_SEMAPHORE                  //信号量
#define RT_USING_MUTEX                      //互斥锁
#define RT_USING_MAILBOX                    //邮箱
#define RT_USING_MESSAGEQUEUE               //消息队列
/************************************************
 * mempool.c和memheap.c是两种不同的内存管理机制
 * 1. mempool.c: 内存池管理
 *  (1)特点: 固定大小内存块; 无内存碎片; 快速分配; 线程安全
 *  (2)数据结构: 内存池起始地址, 总大小, 每个内存块大小, 内存块链表, 总块数, 空闲块数
 *  (3)创建: rt_mp_init(start_addr, BLOCK_SIZE * BLOCK_COUNT, BLOCK_SIZE)
 *  (4)使用场景: 固定大小, 高频分配, 性能要求高
 * 2. memheap.c: 内存堆管理
 *  (1)特点: 可变大小分配; 多内存区域管理; 内存碎片处理; 通用性强
 *  (2)数据结构: 堆起始地址, 总大小, 可用大小, 空闲链表
 *  (3)使用场景: 变长大小, 灵活性好
 * 
 * 
 * 1. 小内存算法作为堆: rt_malloc() 使用小内存算法
 * #define RT_USING_HEAP                    // 启用堆框架
 * #define RT_USING_SMALL_MEM               // 启用小内存算法
 * #define RT_USING_SMALL_MEM_AS_HEAP       // 将小内存算法绑定到堆
 * 
 * 2. SLAB算法作为堆: rt_malloc() 使用SLAB算法
 * #define RT_USING_HEAP                    // 启用堆框架  
 * #define RT_USING_SLAB                    // 启用SLAB算法
 * #define RT_USING_SLAB_AS_HEAP            // 将SLAB算法绑定到堆
 * 
 * 3. 使用memheap算法作为堆
 * #define RT_USING_HEAP                    // 启用堆框架
 * #define RT_USING_MEMHEAP                 // 启用内存堆算法
 * 
 ************************************************/
//#define RT_USING_MEMPOOL                  //RT_USING_MEMPOOL 是独立功能，与其他宏无关
#define RT_USING_SMALL_MEM                 //启用小内存算法
#define RT_USING_SMALL_MEM_AS_HEAP         //使用小内存管理算法管理堆
#define RT_USING_HEAP                      //RT_USING_HEAP 是堆功能的开关，但需要具体算法支持

#define RT_USING_TIMER_SOFT                 //软件定时器
#define RT_TIMER_THREAD_PRIO 4              //定时器线程优先级
#define RT_TIMER_THREAD_STACK_SIZE 256      //定时器线程栈大小

//#define RT_USING_DEVICE

/***********************************************************
 * 
 *                  最小系统要求
 *  RT-Thread 的内核本身是独立的，只需要以下基本组件就能运行：
 *  内核核心 (src/) - 调度器、线程管理、IPC 等
 *  CPU 移植层 (libcpu/) - 上下文切换、中断处理等
 *  板级支持包 (bsp/) - 时钟配置、串口初始化等
 *  C 库适配 (components/libc/ 中的最小实现)
 * 
 ***********************************************************/


 /***********************************************************
 * 
 *                  从RT-Thread BSP - gd32-f407 DEMO查看依赖
 *  
 *  
 *  
 *  
 * 
 * 
 ***********************************************************/


/***********************************************************
 * 
 *          RT-Thread 内核部分
 * 
 ***********************************************************/
#if 0
/* 表示内核对象的名称的最大长度，若代码中对象名称的最大长度大于宏定义的长度，
 * 多余的部分将被截掉。*/
#define RT_NAME_MAX 8

/* 字节对齐时设定对齐的字节个数。常使用 ALIGN(RT_ALIGN_SIZE) 进行字节对齐。*/
#define RT_ALIGN_SIZE 4

/* 定义系统线程优先级数；通常用 RT_THREAD_PRIORITY_MAX-1 定义空闲线程的优先级 */
#define RT_THREAD_PRIORITY_MAX 32

/* 定义时钟节拍，为 100 时表示 100 个 tick 每秒，一个 tick 为 10ms */
#define RT_TICK_PER_SECOND 1000

/* 检查栈是否溢出，未定义则关闭 */
#define RT_USING_OVERFLOW_CHECK

/* 定义该宏开启 debug 模式，未定义则关闭 */
#define RT_DEBUG
/* 开启 debug 模式时：该宏定义为 0 时表示关闭打印组件初始化信息，定义为 1 时表示启用 */
#define RT_DEBUG_INIT 0
/* 开启 debug 模式时：该宏定义为 0 时表示关闭打印线程切换信息，定义为 1 时表示启用 */
#define RT_DEBUG_THREAD 0

/* 定义该宏表示开启钩子函数的使用，未定义则关闭 */
#define RT_USING_HOOK

/* 定义了空闲线程的栈大小 */
#define IDLE_THREAD_STACK_SIZE 256
#endif

/***********************************************************
 * 
 *          线程间同步与通信部分，该部分会使用到的对象有信号量、互斥量、事件、邮箱、消息队列、信号等。
 * 
 ***********************************************************/
#if 0
/* 定义该宏可开启信号量的使用，未定义则关闭 */
#define RT_USING_SEMAPHORE

/* 定义该宏可开启互斥量的使用，未定义则关闭 */
#define RT_USING_MUTEX

/* 定义该宏可开启事件集的使用，未定义则关闭 */
#define RT_USING_EVENT

/* 定义该宏可开启邮箱的使用，未定义则关闭 */
#define RT_USING_MAILBOX

/* 定义该宏可开启消息队列的使用，未定义则关闭 */
#define RT_USING_MESSAGEQUEUE

/* 定义该宏可开启信号的使用，未定义则关闭 */
#define RT_USING_SIGNALS
#endif

/***********************************************************
 * 
 *          内存管理
 * 
 ***********************************************************/
#if 0
/* 开启静态内存池的使用 */
#define RT_USING_MEMPOOL

/* 定义该宏可开启两个或以上内存堆拼接的使用，未定义则关闭 */
#define RT_USING_MEMHEAP

/* 开启小内存管理算法 */
#define RT_USING_SMALL_MEM

/* 关闭 SLAB 内存管理算法 */
//#define RT_USING_SLAB

/* 开启堆的使用 */
#define RT_USING_HEAP
#endif


/***********************************************************
 * 
 *                  内核设备对象
 * 
 ***********************************************************/
#if 0
/* 表示开启了系统设备的使用 */
#define RT_USING_DEVICE

/* 定义该宏可开启系统控制台设备的使用，未定义则关闭 */
#define RT_USING_CONSOLE
/* 定义控制台设备的缓冲区大小 */
#define RT_CONSOLEBUF_SIZE 128
/* 控制台设备的名称 */
#define RT_CONSOLE_DEVICE_NAME "uart1"
#endif

/***********************************************************
 * 
 *                 自动初始化方式
 * 
 ***********************************************************/
#if 0
/* 定义该宏开启自动初始化机制，未定义则关闭 */
#define RT_USING_COMPONENTS_INIT

/* 定义该宏开启设置应用入口为 main 函数 */
#define RT_USING_USER_MAIN
/* 定义 main 线程的栈大小 */
#define RT_MAIN_THREAD_STACK_SIZE 2048
#endif


/***********************************************************
 * 
 *                 FinSH 调试工具
 * 
 ***********************************************************/
#if 0
/* 定义该宏可开启系统 FinSH 调试工具的使用，未定义则关闭 */
#define RT_USING_FINSH

/* 开启系统 FinSH 时：将该线程名称定义为 tshell */
#define FINSH_THREAD_NAME "tshell"

/* 开启系统 FinSH 时：使用历史命令 */
#define FINSH_USING_HISTORY
/* 开启系统 FinSH 时：对历史命令行数的定义 */
#define FINSH_HISTORY_LINES 5

/* 开启系统 FinSH 时：定义该宏开启使用 Tab 键，未定义则关闭 */
#define FINSH_USING_SYMTAB

/* 开启系统 FinSH 时：定义该线程的优先级 */
#define FINSH_THREAD_PRIORITY 20
/* 开启系统 FinSH 时：定义该线程的栈大小 */
#define FINSH_THREAD_STACK_SIZE 4096
/* 开启系统 FinSH 时：定义命令字符长度 */
#define FINSH_CMD_SIZE 80

/* 开启系统 FinSH 时：定义该宏开启 MSH 功能 */
#define FINSH_USING_MSH
/* 开启系统 FinSH 时：开启 MSH 功能时，定义该宏默认使用 MSH 功能 */
#define FINSH_USING_MSH_DEFAULT
/* 开启系统 FinSH 时：定义该宏，仅使用 MSH 功能 */
#define FINSH_USING_MSH_ONLY
#endif
/***********************************************************
 * 
 *                 关于MCU
 * 
 ***********************************************************/
/* 定义该工程使用的 MCU 为 GD32F407RE；系统通过对芯片类型的定义，来定义芯片的管脚 */
#define GD32F407RE

/* 定义时钟源频率 */
#define RT_HSE_VALUE 72000000

/* 定义该宏开启 UART1 的使用 */
#define RT_USING_UART1
#endif
