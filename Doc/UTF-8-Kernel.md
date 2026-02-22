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

3. **销毁队列**


## task: 任务
```
任务创建, 主要包括多核任务创建, 动态内存分配和静态内存分配
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
    - vTaskDelay: INCLUDE_vTaskDelay

3. **任务属性获取**
    - eTaskGetState: INCLUDE_eTaskGetState
    - uxTaskPriorityGet: INCLUDE_uxTaskPriorityGet

3. **任务删除**
    - vTaskDelete: INCLUDE_vTaskDelete


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
