
# FreeRTOS移植前的准备
1. **芯片**
    - 根据芯片类型, 找到port.c文件和portmacro.h文件, 这个文件是适配FreeRTOS内核的核心移植文件
2. **FreeRTOSConfig.h**
    - 根据需求选择合适的配置, 配置直接关系到你的系统能否正常运行


# FreeRTOS配置: FreeRTOSConfig.h
    - 系统时钟, 节拍
    - 堆栈, 内存管理
    - 队列, 信号量, 互斥量
    - 钩子函数
    - 定时器
    - 中断: 中断优先级, 中断屏蔽, 中断函数
1. **configKERNEL_INTERRUPT_PRIORITY和configMAX_SYSCALL_INTERRUPT_PRIORITY**
    - configKERNEL_INTERRUPT_PRIORITY: 内核中断优先级
        - FreeRTOS自身关键中断的中断优先级
        - 确保内核关键中断(SysTick_Handler, PendSV_Handler等)以最低优先级运行, 避免抢占其他重要中断
        - 通用配置成最低优先级
    - configMAX_SYSCALL_INTERRUPT_PRIORITY: 系统调用中断优先级
        - 可以安全调用FreeRTOS API函数的最高中断优先级, 小于这个优先级的不能调用FreeRTOS API
        - 其数值必须比configKERNEL_INTERRUPT_PRIORITY小
        - 由于系统嵌套中断, 高优先级打断低优先级, 二者同时通过FreeRTOS API访问了临界区, 则可能导致死锁
        - 这是可以屏蔽的中断最高优先级, 屏蔽后, 比这个优先级小的中断就无法抢占
        - 那些能够抢占这个中断的其他中断则不能调用FreeRTOS API, 因为抢断可能导致访问死锁
    - 对于CM3或CM4, 它最多能配置16个优先级, 对于同一个优先级的中断, 按照向量表顺序执行

2. **SMP: 多处理器**
    - configNUMBER_OF_CORES: 几个核心
    - configUSE_CORE_AFFINITY: 任务与CPU绑定, 亲核性


# 重要的函数实现: prot.c
    - 不同型号的ARM Cortex(M0, M3, M4_MPU, M4F, M9等), 它的PendSV_Handler函数实现不同
    - 不同型号的ARM Cortex的上下文切换, 任务调度也有所不同
    - 为了实现任务启动和任务切换, 使用了3个异常: SVC(系统调用, 资源访问权限切换), PsendSV(可挂起系统调用, 实现任务切换), SysTick(时基)

1. **SysTick_Handler**
    - FreeRTOS中实现了一个通用的xPortSysTickHandler
    - 所以在SysTick_Handler中调用xPortSysTickHandler即可


2. **PendSV_Handler**: 可挂起服务调用, 上下文切换
    - 主要用于上下文切换
    - 当系统执行任务切换时, 会触发一个PendSV异常


3. **SVC_Handler**: 监管调用, 系统调用
    - 主要用于系统调用
    - 应用程序通过执行SVC指令(通常嵌入在汇编或C代码中)触发SVC异常。该指令包含一个编号（例如，SVC 0x01），用于标识具体的系统服务
    - SVC_Handler()函数会解析这个编号，并调用相应的内核服务函数

## PendSV_Handler: stm32f10x的port.c
````c
__asm void xPortPendSVHandler( void )
{
    extern vTaskSwitchContext
    extern pxCurrentTCB

/* *INDENT-OFF* */
    PRESERVE8

    /*保存当前上下文*/
    mrs r0, psp             //首先把psp放入到r0中, r0本身存放的是函数返回值

    /*获取TCB的地址, 一些上下文需要存放在TCB中, TCB的首地址就是TCB的栈空间*/
    ldr r3, = pxCurrentTCB 
    /*TCB块的第一个成员是pxTopOfStack, 它指向栈顶, pxStack指向了栈的其实位置*/
    ldr r2, [ r3 ]         

    /*sp指着是指向了栈顶的, 首先从psp减去了32字节
    栈是向下生长的, 这意味着向下拉开了32字节的空间
    并把结果存在r0中
    */
    subs r0, # 32
    /*r2指向了pxCurrentTCB的开头, 也就是TCB的栈顶指针
    于是TCB的栈顶就变成了psp向下移动32字节后的栈顶
    */
    str r0, [ r2 ]
    /*保存低寄存器R4-47到r0, r0自动递增(r0此时的值是psp-30)*/
    stmia r0 !, { r4 - r7 }
    /*保存高寄存器的值*/
    mov r4, r8
    mov r5, r9
    mov r6, r10
    mov r7, r11
    /*保存数值到r0指向的地址*/
    stmia r0 !, { r4 - r7 }

    /*执行任务切换*/
    /*push把r3和LR压入到主栈*/
    push { r3, r14 }
    /*关中断*/
    cpsid i
    /*跳转到 vTaskSwitchContext执行, 更新pxCurrentTCB*/
    bl vTaskSwitchContext
    /*开中断*/
    cpsie i
    /*弹出堆栈, r2=pxCurrentTCB, r3=LR*/
    pop { r2, r3 } /* lr goes in r3. r2 now holds tcb pointer. */


    /*恢复新的上下文*/
    /*r2的值给了r1*/
    ldr r1, [ r2 ]
    /*r1的值给了r0, 也就是pxCurrentTCB->pxTopOfStack*/
    ldr r0, [ r1 ]
    /**/
    adds r0, # 16  /* Move to the high registers. */
    /*把r0指向的数据弹出来*/
    ldmia r0 !, { r4 - r7 } /* Pop the high registers. */
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7

    /*r0的指针给psp*/
    msr psp, r0   /* Remember the new top of stack for the task. */

    /*回退这32字节的空间*/
    subs r0, # 32 /* Go back for the low registers that are not automatically restored. */
    ldmia r0 !, { r4 - r7 } /* Pop low registers.  */

    /*跳到r3去执行*/
    bx r3
    ALIGN
/* *INDENT-ON* */
}
````
    - Cortex-M3通用寄存器: R0-R12
        - R0-R7: 可以被所有指令直接访问, 通常用语函数参数传递、局部变量存储和计算操作
            - R0: 通常用于函数返回值
            - R1-R3: 通常用于函数参数传递, ARM架构中
            - R4-R7: 用于保护中间值
        - R8-R12: 只能被部分指令访问(MOV, ADD等), 通常用于保存全局变量或跨函数调用的数据
    - Cortex-M3专用寄存器
        - R13(SP, Stack Pointer): 堆栈指针， 用于管理堆栈内存
            - MSP(Main Stack Pointer): 用于操作系统内核和中断处理, 是内核模式, 中断处理下的栈顶指针
            - PSP(Process Stack Pointer): 用于用户任务, 用户栈顶指针
        - R14(LR, Link Register): 链接寄存器, 存储函数调用后的返回地址
        - R15(PC, Program Counter): 程序计数器, 指向下一条要执行的指令地址
        - xPSR(Program Status Register): 程序状态寄存器, 处理器的状态标志(条件标志, 中断标志, 执行状态)
        - CONTROL Register: 控制寄存器, 管理处理器模式(特权模式/用户模式)和堆栈选择(MSP/PSP)
    - Cortex-M3异常入栈顺序: 硬件行为
        - 发生异常时, PC, LR, xPSR, R0-R3, R12, 硬件自动压栈
    - 上下文切换
        - TCB创建的时候会分配栈空间：其中pxStack指向栈底部, pxTopOfStack指向栈顶部, 栈向下增长, pxTopOfStack不断减小
        - 进行上下文切换时, PSP(Process SP)指向栈顶(pxTopOfStack), 此时需要向下移动栈顶位置, 腾出空间来存储上下文
        - 保存当前线程的上下文到TCB
        - 执行切换, 全局TCB句柄保存切换后的上下文
        - 从全局TCB句柄获取切换到的TCB, 重载当前TCB的上下文


## SVC_Handler： stm32f10x的port.c
```c
void vPortSVCHandler( void )
{
    /* This function is no longer used, but retained for backward
     * compatibility. */
}
```



# Kernel的使用
    - ISR接口: 专门设计用于中断服务程序 (ISR) 内部, 不能被阻塞
    - portmacro.h: portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT, 通过配置寄存器, 触发PendSV异常, 从而调用PendSV_Handler, 实现上下文切换

## 内存
1. **基本概念**
    - 栈空间地址对齐: FreeRTOS以8字节大小对齐
        - 操作系统内核（如 FreeRTOS）要求任务的栈内存区域的起始地址 (pxStackBuffer) 必须满足特定的对齐要求
        - 栈数组的首地址在内存中不能是任意位置，而必须是某个特定边界（如 4 字节或 8 字节边界）的倍数

2. **静态内存分配**: configSUPPORT_STATIC_ALLOCATION = 1
    - 此定义下需要用户实现两个函数
        - vApplicationGetIdleTaskMemory(): 设定空闲任务堆栈大小
        - vApplicationGetTimerTaskMemory(): 设定定时器任务堆栈大小
```
static StackType_t Idle_Task_Stack[configMINIMAL_STACK_SIZE];
static StackType_t Timer_Task_Stack[configTIMER_TASK_STACK_DEPTH];
static StaticTask_t Idle_Task_TCB;
static StaticTask_t Timer_Task_TCB;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                StackType_t **ppxIdleTaskStackBuffer,uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer=&Idle_Task_TCB;/* 任务控制块内存 */
    *ppxIdleTaskStackBuffer=Idle_Task_Stack;/* 任务堆栈内存 */ 
    *pulIdleTaskStackSize=configMINIMAL_STACK_SIZE;/* 任务堆栈大小 */
}
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, 
                                StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer=&Timer_Task_TCB;/* 任务控制块内存 */
    *ppxTimerTaskStackBuffer=Timer_Task_Stack;/* 任务堆栈内存 */ 
    *pulTimerTaskStackSize=configTIMER_TASK_STACK_DEPTH;/* 任务堆栈大小 */ 
}
```
3. **动态内存分配**
    - 

## queue: 队列

### API接口
1. **创建队列**
    - xQueueCreate(uxQueueLength, uxItemSize)
        - 队列的长度, 元素的大小, 作为参数
        - 除了元素的大小, 还需要元素的类型, 这样取出元素才能使用
        - 没有带Static的都是动态内存创建接口

2. **使用队列**
    - xQueueReceive(QueueHandle_t xQueue,void * const pvBuffer,TickType_t xTicksToWait )
        - 取出一个元素, pvBuffer的空间确保和队列元素大小一致
    - xQueueSend( xQueue, pvItemToQueue, xTicksToWait )
        - 把一个元素中的内容复制到队列中, 使用指针可以避免生成临时变量
    - xQueueReceiveFromISR(QueueHandle_t xQueue, void * const pvBuffer, BaseType_t * const pxHigherPriorityTaskWoken )
        - 用于中断中取出一个元素
    - uxQueueMessagesWaiting(const QueueHandle_t xQueue )
        - 返回已经使用的空间大小
    - xQueueOverwrite(): 覆盖写入
        - 要求创建的队列长度只能是1
    - xQueueSendToFront
    - xQueueSendToBack

3. **队列集** 
    - 作用
        - 队列集是 FreeRTOS 中用于同时监控多个队列/信号量的高级机制, 允许任务在单次阻塞中等待多个事件源
        - 其核心价值在于解决复杂场景下的多事件同步问题
    - xQueueCreateSet
    - xQueueAddToSet
    - xQueueRemoveFromSet
    - vQueueDelete
    - xQueuePeek
        - 查看队列头部数据不移除
    - xQueueSelectFromSet
        - 核心阻塞接口，等待集合中任一成员就绪

4. **销毁队列**


## task: 任务
```
任务创建, 主要包括多核任务创建, 动态内存分配和静态内存分配
组成: 主体函数, 控制块, 任务栈
```
1. **创建任务**
    - 多核心: configNUMBER_OF_CORES, configUSE_CORE_AFFINITY
    - 静态内存分配: configSUPPORT_STATIC_ALLOCATION
        - prvCreateStaticTask(pxTaskCode, ..., puxStackBuffer, pxTaskBuffer, pxCreatedTask )
            - puxStackBuffer: 任务栈空间
            - pxTaskBuffer: TCB空间
		- xTaskCreateStatic
        - xTaskCreateStaticAffinitySet: 多核, 绑定CPU
    - 动态内存分配
        - prvCreateTask
		- xTaskCreate
        - vTaskCoreAffinitySet: 多核, 绑定CPU
2. **任务延时**
    - xTaskDelayUntil: INCLUDE_xTaskDelayUntil
        - 绝对时间: startTimePoint + IncrementTime
    - vTaskDelay: INCLUDE_vTaskDelay
        - 阻塞延时, 任务挂起, 切换到其他就绪任务
        - 不能使用独占CPU的延迟方式

3. **任务属性获取**
    - eTaskGetState: INCLUDE_eTaskGetState
    - uxTaskPriorityGet: INCLUDE_uxTaskPriorityGet

4. **任务删除**
    - vTaskDelete: INCLUDE_vTaskDelete

5. **任务挂起和恢复**
    - vTaskSuspend(TaskHandle_t xTaskToSuspend)
        - vTaskSuspend(NULL): 挂起所属线程
    - vTaskResume(TaskHandle_t xTaskToResume)
    - vTaskSuspendAll(): 挂起所有任务, 其实就是锁住调度器, 不再进行调度. 
        - 此时中断还可以运行, 但是中断中触发的调度任务依然会被挂起
        - 当我们不想其他程序打断或访问时, 可以执行挂起所有任务
    - xTaskResumeAll(): vTaskSuspendAll可以嵌套调用, 执行多少次vTaskSuspendAll, 就需要执行多少次xTaskResumeAll()

6. **任务调度触发**
    - taskYIELD(): 通过软件中断强制触发任务调度
    - portYIELD_FROM_ISR(): 在中断中, FromISR执行后, 有一个延迟调度标志, 作为该函数入参, 触发调度


## 信号量: semphr.h
```
对于共享资源, 可以使用信号量来互斥访问
底层数据结构是队列
信号量有二进制信号量(同步)
计数信号量(资源计数): 初始化的时候可以根据使用场景分配初始资源数(0 or All)
```
1. **信号量创建**
    - xSemaphoreCreateMutex: xQueueCreateMutex(queueQUEUE_TYPE_MUTEX)
        - typedef QueueHandle_t SemaphoreHandle_t: 信号量就是队列句柄
        - queueQUEUE_TYPE_MUTEX: 表示队列类型是互斥量
    - xSemaphoreCreateCounting(uxMaxCount, uxInitialCount): 计数信号量
        - uxMaxCount: 最大计数值
        - uxInitialCount: 初始计数值
    
2. **信号量操作**
    - xSemaphoreTake: 从队列中取走一个元素
    - xSemaphoreGive: 向队列中放入一个元素
    - uxQueueMessagesWaiting: 返回队列中的元素数量
    - uxQueueSpacesAvailable: 返回队列剩余的空闲的元素空间

3. **关于多线程互斥访问问题**
    - 被阻塞挂起
    - 被通知执行

4. **优先级继承**: 避免高优先级因低优先级任务的阻塞而无限期延迟
    - 当一个高优先级任务因为等待一个低优先级任务的信号量而阻塞时, 会发生优先级继承
        - 低优先级任务的优先级被临时提升到高优先级任务的优先级
        - 当低优先级释放了信号量, 高优先级可以执行; 同时, 低优先级任务恢复到原来的优先级
    - 可以防止高优先级任务优先级反转问题: 变成了低优先级

5. **queueQUEUE_TYPE_BINARY_SEMAPHORE和queueQUEUE_TYPE_MUTEX**
    - queueQUEUE_TYPE_BINARY_SEMAPHORE: 二进制信号量, 初始值为0, 表示空信号量
        - 主要用于任务同步
        - 特点
            - 无资源所有权概念
            - 无所有者概念, 任何任务均可释放
            - 无优先级继承
            - 支持中断中执行 Give
        - 使用场景
            - 任务与中断同步(如 ISR 发送事件，任务处理)
            - 简单事件通知(无资源竞争)
            - 同步两个任务的执行顺序
    - queueQUEUE_TYPE_MUTEX: 初始值为1, 表示资源可用
        - 专为资源互斥访问设计
        - 特点
            - 必须由获取它的任务释放
            - 自动提升持有者优先级
            - 递归锁, 同一任务可以重复获取
        - 使用场景
            - 保护共享资源(如全局变量、外设寄存器)
            - 需要避免优先级反转的场景(高优先级任务等待低优先级任务释放资源)
            - 需要递归锁的复杂临界区
6. **递归锁**
    - xSemaphoreCreateRecursiveMutex
    - xSemaphoreTakeRecursive
    - xSemaphoreGiveRecursive
    - 递归锁的拥有者是任务本身(线程), 一个线程可以递归调用递归锁, 但是要执行与上锁深度匹配的解锁流程
    - 其他的线程也可以调用这个递归锁, 但是需要等待其他线程解锁完毕后, 才能获取并上锁
    - 拥有互斥量后, 会继承互斥量阻塞的任务的优先级(比自己高的情况下)



## 调度器
1. **抢占式调度**
    - 中断函数不可抢占: 中断处理之间存在优先级关系, 高优先级可抢占低优先级, 但是如果中断屏蔽了, 则执行期间不会响应被屏蔽中断
    - 调度器上锁部分的代码不可抢占
    - 禁止中断的代码不可抢占

2. **查找最高优先级任务**
    - configUSE_PORT_OPTIMISED_TASK_SELECTION = 1, 则一般限制最大可用优先级数目为32: 可以使用CLZ指令计算
    - 方法一: 从就绪链表中从高优先级往低查找uxTopPriority(创建任务时, 已经根据优先级进行排序)
    - 方法二: 使用前导零指令CLZ, 直接在uxTopReadyPriority这个32位变量中直接得出uxTopPriority

3. **FromISR**
    - 这个后缀的函数不会触发任务切换, 但是会返回一个值用于表示此次中断是否唤醒了一个高优先级任务
    - 它把任务切换延迟到中断执行完毕后执行: 也就是所谓的"中断无法抢占"
    - 在比"系统可管理的最大中断优先级"还要高的中断中不能调用任何FromISR, 因为FreeRTOS API总的临界区保护是通过屏蔽系统管理的中断实现, 高优先级调用抢占会导致临界区被破坏


## timer: 定时器
```
定时器机制: OS时基
    定时器服务任务: 处理所有定时器的到期事件, 定时器服务任务按顺序检查和处理到期的定时器
    定时器队列: 所有定时器操作(启动, 停止, 重置, 删除等)都通过一个队列传递给定时器服务任务
    时间基准: 系统滴答计数
    定时器列表管理: 定时器按照过期时间排序存储在链表中
定时器特性
    自动重载定时器: 对于自动重载定时器, 一直处于激活状态, 除非删除或停止
    一次性定时器: 对于一次性定时器, 执行完后, 就失活了
定时器参数
    周期
    自动重载标志
    回调处理函数
```

1. **创建定时器**
    - xTimerCreate(pcTimerName, xTimerPeriodInTicks, xAutoReload, pvTimerID, pxCallbackFunction)
        - pcTimerName: 定时器名字
        - xTimerPeriodInTicks: 定时器超时时间
        - xAutoReload: 是否自动重载
        - pvTimerID: 定时器ID(私有数据)
        - pxCallbackFunction: 定时器超时回调
    - 创建只是创建了一个定时器实体, 此时并没有加入到定时器链表中
2. **启动定时器**: configTIMER_QUEUE_LENGTH
    - xTimerStart
    - configTIMER_QUEUE_LENGTH
        - 当创建和启动的定时器达到configTIMER_QUEUE_LENGTH, 同时调度器还没有启动时, 则会因为队列的原因导致死等
    - 启动定时器后, 定时器添加到定时器链表中


## 任务通知
```
每个任务自带通知状态
    TCB->ulNotifiedValue: uint32_t, 通知值
    TCB->ucNotifyState: uint8_t, 通知状态
        taskNOT_WAITING_NOTIFICATION：任务未在等待通知。
        taskWAITING_NOTIFICATION：任务正在调用 xTaskNotifyWait 或 ulTaskNotifyTake 等待通知
        taskNOTIFICATION_RECEIVED：任务已收到通知但尚未被取走/处理
操作原子性
    所有操作都在临界区(关中断或调度器锁)内完成, 确保线程安全和ISR安全
通知是会被覆盖或累加的
```

1. **xTaskNotify和xTaskNotifyWait**
    - xTaskNotify(xTaskToNotify, ulValue, eAction)
        - 向指定任务发送通知
        - xTaskToNotify: 任务句柄
        - eAction
            - eNoAction: 仅设置通知状态为 taskNOTIFICATION_RECEIVED, 不修改 ulNotifiedValue。相当于轻量事件标志
            - eSetBits: 将 ulValue 按位或到任务的 ulNotifiedValue 上。用于事件标志组
            - eIncrement: 将任务的 ulNotifiedValue 加 1。用于轻量计数信号量
            - eSetValueWithOverwrite: 无条件覆盖任务的 ulNotifiedValue 为 ulValue。用于传输单个值
            - eSetValueWithoutOverwrite: 仅在当前通知未被接收 (ucNotifyState != taskNOTIFICATION_RECEIVED) 时覆盖 ulNotifiedValue 为 ulValue。避免覆盖未处理的值
        - 返回值
            - pdPASS: 成功发送（对于 eSetValueWithoutOverwrite，仅在覆盖成功时返回）
            - pdFAIL: 仅当 eSetValueWithoutOverwrite 且通知值未被取走（覆盖失败）时返回
    - xTaskNotifyWait(ulBitsToClearOnEntry, ulBitsToClearOnExit, pulNotificationValue, xTicksToWait)
        - 等待"当前任务"接收到通知, 获取想要的值, 并恢复成配置的值
            - 如果已经接收到了通知值(taskNOTIFICATION_RECEIVED), ulBitsToClearOnEntry不会作用在任务的 ulNotifiedValue
            - 如果没有接收到通知值, 先用 ulBitsToClearOnEntry 清除 ulNotifiedValue, 然后再把 ulNotifiedValue 返回给pulNotificationValue
        - ulBitsToClearOnEntry: 进入等待状态前, 清除当前 ulNotifiedValue 中的哪些位
        - ulBitsToClearOnExit: 在成功接收到通知后, 函数返回前, 清除 ulNotifiedValue 中的哪些位
        - pulNotificationValue: 输出参数, 用于接收退出等待时的通知值
        - xTicksToWait: 等待超时时间

2. **xTaskNotifyGive和ulTaskNotifyTake**
    - 专为模拟"计数信号量"设计的简化发送函数
    - xTaskNotifyGive:  等价于xTaskNotify(xTaskToNotify, 0, eIncrement)
    - ulTaskNotifyTake(xClearCountOnExit, xTicksToWait)
        - xClearCountOnExit
            - pdTRUE: 成功获取后, 将通知值清零
            - pdFALSE: 成功获取后, 将通知值减1
        - 返回值: 返回没有减1之前的值

3. **xTaskNotifyStateClear和ulTaskNotifyValueClear**
    - ulTaskNotifyValueClear: 返回清除之前的值
    - xTaskNotifyStateClear: 清空状态

4. **configTASK_NOTIFICATION_ARRAY_ENTRIES: 通知数组大小**
    - xTaskNotifyWaitIndexed
    - xTaskNotifyAndQueryIndexed
    - xTaskNotifyAndQueryIndexedFromISR
    - 通知存在很多通道, 每个通道有对应的状态

## FromISR接口: 专为中断服务程序设计接口

1. 特点
    - 无阻塞设计: 中断必须快速完成, 不能有阻塞
    - 延迟调度指示: 通过参数返回调度需求
    - 临界区保护: taskENTER_CRITICAL_FROM_ISR()和taskEXIT_CRITICAL_FROM_ISR(), 屏蔽中断, 防止中断嵌套破坏数据
    - 轻量级上下文切换: portYIELD_FROM_ISR(), 直接操作PendSV 异常


## 事件组: 32位的位图

1. **事件通知**
    - xEventGroupCreate(): 创建一个时间组
    - xEventGroupWaitBits(): 等待事件组的标志位被置位
    - xEventGroupSetBits(): 置位事件组的标志位


## 流缓冲区: stream_buffer.c

1. **简介**
    - 支持消息缓冲和流缓冲两种模式
    - 支持阻塞和非阻塞操作
    - 支持中断上下文通信
2. **StreamBuffer类型**
    - __Stream Buffer__：连续数据流，无消息边界
    - __Message Buffer__：离散消息，每个消息有长度信息
    - __Batching Buffer__：批处理缓冲区，只有当数据量达到触发水平时才唤醒任务

3. **核心API函数**
    - 创建
        - xStreamBufferCreate
        - xStreamBufferCreateStatic
    - 发送
        - xStreamBufferSend
        - xStreamBufferSendFromISR
    - 接收
        - xStreamBufferReceive
        - xStreamBufferReceiveFromISR
    - 状态检查
        - xStreamBufferBytesAvailable(): 获取可用字节数
        - xStreamBufferSpacesAvailable(): 获取可用空间
        - xStreamBufferIsEmpty(): 检查是否为空
        - xStreamBufferIsFull(): 检查是否已满





# FreeRTOS-Plus简介

## FreeRTOS Plus
1. **FreeRTOS Plus TCP**
2. **FreeRTOS Plus CLI**: 命令行接口
3. **FreeRTOS Plus IO**
4. **FreeRTOS Plus Trace**

## FreeRTOS Core
1. **coreMQTT**
2. **coreMQTT Agent**
3. **coreHTTP**
4. **coreSNTP**
5. **coreJSON**
6. **corePKCS11**
7. **文件流**
8. **蜂窝接口**

## FreeRTOS Labs: 可移植性
1. **LoRaWAN**
2. **FreeRTOS Plus POSIX**
3. **FreeRTOS Plus FAT**
4. **FreeRTOS MCUBoot**


# FreeRTOS-Plus简介

## corePKCS11

### PKCS#11: Public-Key Cryptography Standards
1. **是什么**: 公共秘钥加密标准
    - 标准就是接口: RSA实验室发布的一套使用密钥的接口
    - 接口: PKCS#11标准的核心是"一套使用密钥的API函数和数据类型""
2. **由什么组成: 运行的机理**
    - 标准中的 __Object__ (对象): 为哪些实体定义了标准
        - Data Objects: 数据对象, 用于存储任意数据
        - Certificate Objects: 证书对象, 用于存储X.509数字证书
        - Key Objects: 密钥对象
            - Public Key Objects: 公钥
            - Private Key Objects: 私钥
            - Secret Key Objects: 对称密钥
    - 标准中的 __Mechanisms__: 管理这些对象的机制, 感觉更像是引擎
        - Encrypt/Decrypt: 加密/解密. RSA加密, AES加密
        - Sign/Verify: 签名/验签. RSA-PKCS, ECDSA签名
        - Digest: 哈希
        - Key Generation: 密钥生成. 生成RSA、EC等密钥对
        - Key Derivation: 密钥派生. 从密钥派生密钥
    - Sessions & States
        - 定义了应用如何与设备建立连接、如何管理操作的状态等
3. **如何使用**
    - 只要设备厂商提供了符合标准的PKCS#11库, 相同PKCS#11代码就能操作不同的密码硬件设备
    - __使用场景__
        - 数字证书和签名
        - 身份认证
        - 区块链和加密货币
        - 金融支付系统


### PKCS#11: FreeRTOS移植
1. **corePKCS#11**
    - 核心数据结构
        - Slot: 槽, 放密码硬件的槽
            - CK_SLOT_INFO: 槽
                - 描述信息
                - 厂商ID
                - 硬件版本
                - 固件版本
            - CK_SLOT_ID: CK_ULONG, unsigned long int
        - Token: 抽象概念, __代表一个物理或逻辑的加密设备或存储介质(一个安全的加密存储区域)__
            - 主要用途: 密钥存储; 证书存储; 身份认证(加密/解密, 签名/验签); 数据保护
            - __App__ 通过 __Session__ 与 __Token__ 建立连接, 执行加密/认证操作
            - __Pin码__: 令牌本身的安全机制
            - __Memroy__: 内存总量. 包括公共内存, 私有内存
        - Session
            - CK_SESSION_INFO
                - Slot ID
                - 会话状态: 只读公共会话, 只读用户功能, 读写公共会话, 读写用户功能 
                - 标志: 串行会话; 读写会话?
            - __SlotID与Token绑定__
            - __C_Openssion关联SlotID与Session__
            - __C_CreateObject关联(Object+Attribute[密钥])与Session__
            - 使用 __Mechanism__ 通过Session对Object和Token进行操作
        - Object
            - CK_OBJECT_CLASS: CK_ULONG, 定义了不同的类别
                - CKO_DATA: 数据
                - CKO_CERTIFICATE: 证书
                    - CK_CERTIFICATE_TYPE: 证书类型, 包括CKC_X_509, CKC_X_509_ATTR_CERT, CKC_WTLS, CKC_VENDOR_DEFINED(供应商自定义)
                - CKO_PUBLIC_KEY: 公钥
                - CKO_PRIVATE_KEY: 私钥
                - CKO_SECRET_KEY: 对称加密密钥
                - CKO_HW_FEATURE
                - CKO_DOMAIN_PARAMETERS
                - CKO_MECHANISM
                - CKO_OTP_KEY
        - Object子类
            - CK_KEY_TYPE: 密钥类型
                - CKK_RSA
                - CKK_DSA
                - CKK_DH
                - CKK_AES
                - CKK_MD5_HMAC
                - CKK_SHA256_HMAC
            - CK_MECHANISM_TYPE: 机构类型, 各种PKCS#11的应用
                - CKM_RSA_PKCS_KEY_PAIR_GEN: 密钥生成
                - CKM_RSA_PKCS
                - CKM_SHA1_RSA_PKCS: 
                - CKM_SHA256_RSA_PKCS
                - CKM_DSA_SHA256: 
                - CKM_DH_PKCS_DERIVE: 密钥派生, 用于密钥交换
                - CKM_DES_KEY_GEN: 对称密钥生成
                - CKM_MD5_HMAC: 摘要
                - CKM_SSL3_MASTER_KEY_DERIVE_DH: 主密钥派生
                - CKM_TLS_PRE_MASTER_KEY_GEN: 预主密钥生成
                - CKM_TLS_MASTER_KEY_DERIVE
                - CKM_WTLS_PRE_MASTER_KEY_GEN:
                -  CKM_WTLS_MASTER_KEY_DERIVE: 
    - 核心接口: 这些接口都是抽象接口, 还没有实现, 需要具体的工具箱
        - C_OpenSession()
        - C_CreateObject()
        - C_SignInit()
        - C_VerifyInit()
        - C_DigesttInit()
        - C_DigestUpdate()
    - 核心文件
        - core_pkcs11.c
        - core_pki_utils.c
        - core_pkcs11_mbedtls.c
        - mbedtls_utils.c
    - 从这个组成来看, PKCS#11向外提供了一个统一使用加密工具的标准, 屏蔽了不同类型的加密工具差异性
        
2. **mbedtls**
    - 密码学原语功能
        - 对称加密
        - 非对称加密
        - 散列
        - 消息认证码
    - X.509证书处理
        - 证书解析与验证
        - 证书签发与写入
    - 其他
        - ASN.1解析器
        - PEM格式支持
        - PKCS#5/PKCS#12支持

### PKCS其他标准
1. **PKCS#1**: RSA加密标准
    - 定义RSA公钥和私钥的语法
    - 定义RSA加密、签名和验签的流程
2. **PKCS#3**: 定义使用DH算法进行安全秘钥交换的协议
3. **PKCS#5**: __基于密码的加密标准__. 规范如何从用户密码(口令)安全地派生加密密钥
4. **PKCS#7**: 定义被加密或签名后的数据的结构格式
5. **PKCS#8**: 定义 __私钥的存储格式__
6. **PKCS#10**: 定义向证书办法机构申请数字证书时发送的数据格式(CSR-Certificate Signing Request)
7. **PKCS#11**: 密码令牌接口标准. 提供一套独立与设备的编程接口, 用于访问TPM(Trust Platform Module), HSM(Hardware Sercure Module)等硬件安全设备
8. **PKCS#12**: 个人信息交换语法标准. 定义一种文件格式, __用于将用户的私钥、证书和其他相关材料打包在一个受密码保护的文件中__





# 遇到的问题

##　中断优先级配置问题：FreeRTOSConfig.h
```
configKERNEL_INTERRUPT_PRIORITY和configMAX_SYSCALL_INTERRUPT_PRIORITY配置不当导致程序无法运行
```
1. **作用**
    - configKERNEL_INTERRUPT_PRIORITY: 配置内核相关中断的优先级, 确保优先级不会影响(抢占)重要的用户中断
    - configMAX_SYSCALL_INTERRUPT_PRIORITY: 可以屏蔽的最大中断优先级, 屏蔽后, 可以安全调用FreeRTOS API(其中存在临界区访问, 比如ISR函数)
    - 由于NVIC的特性, 存在高优先级抢占低优先级CPU使用权执行, 所以太高优先级的中断不能调用FreeRTOS API, 因为无法屏蔽, 会导致和低优先级的中断中FreeRTOS API调用发生冲突(比如访问临界区)
2. **CM3和CM4**
    - CM3和CM4的NVIC可配置优先级为4位, 也就是支持16个不同的优先级
    - 由于这4位位于高4位, 且低4位置始终为1, configKERNEL_INTERRUPT_PRIORITY和configMAX_SYSCALL_INTERRUPT_PRIORITY可配置的优先级是255-32


## 驱动层中断优先级配置问题: 与configMAX_SYSCALL_INTERRUPT_PRIORITY
```
如果配置的中断优先级比configMAX_SYSCALL_INTERRUPT_PRIORITY要高, 之后在中断中调用了FreeRTOS接口, 则会报错
```
1. **作用**
    - configMAX_SYSCALL_INTERRUPT_PRIORITY的作用是建立临界区, 当进入中断时, 屏蔽比其他低优先级中断, 避免被打断
    - 
## 定时器操作队列问题: configTIMER_QUEUE_LENGTH
```
问题: 在启动调度器之前创建了超过configTIMER_QUEUE_LENGTH数量的定时器并启动了它, 导致系统死等
```
1. **作用**
    - 定义定时器操作队列的大小
        - 定时器操作: 停止, 启动, 重置等
2. **使用方法**
    - 由于调度器启动后, 系统才会从队列中接收操作, 所以如果在调度器启动后再去添加定时器, 就不存在队列插入死等的问题
    - 当需要启动定时器时, 增加一个"调度器是否正在运行"的判断


## 任务优先级问题
```
当一个任务的优先级比较低的时候, 它被执行的机会将会很低, 所以各个任务的优先级需要合理分配
```
1. **命令行交互任务的优先级要其他任务低, 这样才能看到其他任务的交互信息**


## 优先级继承: 低优先级阻塞高优先级, 低优先任务被暂时拉高到同等水平, 直到资源释放



## 任务通知: xTaskNotifyWait
```
xTaskNotifyWait(), 当且仅当任务未处于接收到通知时, 通知值才会被进入清除标志给清除
```

## 系统堆和栈的分配
```
编译成功, 无法运行: 编译后可执行文件为27KB, 但是Code + Data的总大小超过了192KB
```
1. **启动文件和分散加载文件**
```
Stack_Size      EQU     0x00002800
                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size
__initial_sp


Heap_Size       EQU     0x0001C500
                AREA    HEAP, NOINIT, READWRITE, ALIGN=3
__heap_base
Heap_Mem        SPACE   Heap_Size
__heap_limit
```
2. **分散加载文件**
```
LR_IROM1 0x08000000 0x00010000  {   ; 加载区域（Flash）：起始地址0x08000000，大小64KB
  ER_IROM1 0x08000000 0x00010000  { ; 执行区域（Flash）
   *.o (RESET, +First)             ; 中断向量表放在最前面
   *(InRoot$$Sections)             ; 库相关的段
   .ANY (+RO)                      ; 所有代码和只读数据
  }

  RW_IRAM1 0x20000000 0x00005000  { ; 执行区域（RAM）：起始地址0x20000000，大小20KB
   .ANY (+RW +ZI)                  ; 所有读写数据、零初始化数据（包括堆栈）
  }
}
```
3. **启动文件定义堆和栈的大小**: 启动文件预留空间
    - Stack_Size      EQU     0x00000400: Stack_Size: 用来定义栈的大小
    - Heap_Size       EQU     0x00000200: Heap_Size: 用来定义堆的大小
    - 分配栈空间
        - Stack_Mem       SPACE   Stack_Size  ; 为栈分配空间: SPACE指令: 在目标内存中保留指定大小的字节空间
        - __initial_sp: 一个标号, 表示栈顶的初始地址(栈向下增长, 所以这是栈的最高地址)
    - 分配堆空间
        - Heap_Mem        SPACE   Heap_Size: SPACE指令: 在目标内存中保留指定大小的字节空间
        - __heap_base和__heap_limit：标号，分别表示堆的起始地址和结束地址

4. **分散加载文件定义堆和栈的起始地址**: 分散加载文件决定这些空间被放置在内存的哪个地址
    - LR_IROM1 0x08000000 0x00010000: 加载区域是Flash, 起始地址0x08000000, 大小64KB
        - ER_IROM1 0x08000000 0x00010000: 执行区域
        -  *.o (RESET, +First) : 中断向量表, RESET是上电启动要跳转的位置
        - *(InRoot$$Sections): 库相关的段
        - .ANY (+RO): 所有代码段和只读数据
    - RW_IRAM1 0x20000000 0x00005000: 执行区域RAM, 起始地址0x20000000, 大小20KB
        - .ANY (+RW +ZI): 所有读写数据, 零初始化数据(包括堆栈)

5. **启动文件和分散加载文件的关系**
    - 启动文件中定义的"堆"和"栈"属于ZI(Zero-Initialized)数据
    - 栈的起始地址(__initila_sp): 通常放在RW_IRAM1区域的最高地址
    - 堆的起始地址(__heap_base): 紧跟着其他RW/ZI数据之后
    - 堆的结束地址(__heap_limit): 起始地址 + 堆大小

6. **FreeRTOS中堆和栈的管理**
    - FreeRTOS中的"堆"
        - 从启动文件定义的堆中通过malloc分配而来, 或者是一个静态数组
    - FreeRTOS中的"栈"
        - 创建任务时通过参数指定, 这个参数可能是一个RW, 也可能是一个ZI

7. **编译和执行**
    - 编译后生成的可执行文件大小, 是不包含未初始化的数据
    - 当总的数据和代码大小超过了芯片的内存大小时, 程序是无法运行的





