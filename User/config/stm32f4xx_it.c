
#include <stdio.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "drv_time.h"

#include "timer_manager.h"

/*********************
 * 
 * SysTick定时器回调
 * 
 *********************/
void SysTick_Handler()
{
	//timer_manager_tick_handler();
	drv_time_sys_tick_increment();
	timer_manager_tick_decrement_handler();
}

void HardFault_Handler_C(void *arg)
{
    printf("\r\n================= HardFault Detected =================\r\n");
    // 读取故障状态寄存器
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t dfsr = SCB->DFSR;
    uint32_t mmfar = SCB->MMFAR;
    uint32_t bfar = SCB->BFAR;
    
    // 打印故障状态寄存器

    // 打印堆栈内容（PC、LR等）
    printf("\r\n--- Stack Frame arg = 0x%08X---\r\n", arg);
    uint32_t* stack_ptr = (uint32_t*)(arg);
    
    // 堆栈帧包含: R0-R3, R12, LR, PC, xPSR
    printf("0:  0x%08X\r\n", stack_ptr[0]);
    printf("1:  0x%08X\r\n", stack_ptr[1]);
    printf("2:  0x%08X\r\n", stack_ptr[2]);
    printf("3:  0x%08X\r\n", stack_ptr[3]);
    printf("4:  0x%08X\r\n", stack_ptr[4]);
    printf("5:  0x%08X\r\n", stack_ptr[5]);
    printf("6:  0x%08X\r\n", stack_ptr[6]);
    printf("7:  0x%08X\r\n", stack_ptr[7]);
    printf("8:  0x%08X\r\n", stack_ptr[8]);
    printf("9:  0x%08X\r\n", stack_ptr[9]);
    printf("10: 0x%08X\r\n", stack_ptr[10]);
    printf("11: 0x%08X\r\n", stack_ptr[11]);
    printf("12: 0x%08X\r\n", stack_ptr[12]);
    printf("13: 0x%08X\r\n", stack_ptr[13]);
    printf("14: 0x%08X\r\n", stack_ptr[14]);
    printf("15: 0x%08X\r\n", stack_ptr[15]);
    printf("16: 0x%08X\r\n", stack_ptr[16]);

    // 尝试打印函数调用链
    printf("\r\n--- Call Chain Analysis ---\r\n");
    printf("Return Address (PC): 0x%08lX\r\n", stack_ptr[6]);
    printf("Link Register (LR):  0x%08lX\r\n", stack_ptr[5]);

    if (stack_ptr[5] & 0x1) {
        printf("LR indicates Thumb mode\r\n");
    } else {
        printf("LR indicates ARM mode (unexpected for Cortex-M)\r\n");
    }
    
    printf("\r\n==================================================\r\n");
    
    while(1);
}

__asm void HardFault_Handler(void)
{
	IMPORT HardFault_Handler_C
    /* 保存寄存器到堆栈 */
    tst lr, #4
    ite eq
    mrseq r0, msp
    mrsne r0, psp
    
    /* 现在r0里面是sp指针的值了 */
    /* 获取堆栈指针到r0 */
    /* 首先这个函数是没有参数, 所有R0-R3, 都是可以用的 */
    /* 移动msp或psp的值到了r0里面, 接下来就可以使用这个值了 */
    /* 这个时候由于sp指针的值并没有改变, 所以直接打印也没有什么问题 */
    /* 最后退出的时候, 我要打印一些指针的值, 所以直接打印栈中的值 */
    mov r0, r13
    
    /* 获取PC寄存器值 */
    //ldr r2, [r0, #24]  /* PC在堆栈中的偏移量为24字节 */
    
    /* 获取LR寄存器值 */
    //ldr r3, [r0, #20]  /* LR在堆栈中的偏移量为20字节 */
    
    /* 跳转到C函数处理 */
    b HardFault_Handler_C
}

#if 0
// 实际的HardFault中断服务例程
void HardFault_Handler(void) {
    
	printf("=================HardFault\r\n");
	while(1);
}
#endif

// 实际的HardFault中断服务例程
void BusFault_Handler(void) {
    printf("=================BusFault\r\n");
	while(1);
}

// 实际的HardFault中断服务例程
void UsageFault_Handler(void) {
    printf("=================UsageFault\r\n");
	while(1);
}