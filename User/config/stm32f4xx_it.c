
#include <stdio.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "drv_time.h"
#include "drv_timer.h"

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


/*********************
 * 
 * timer6定时器中断
 * 
 *********************/
void TIM6_DAC_IRQHandler()
{
    /*获取timer6, 检查是否使能中断, 检查中断函数是否为空*/
    int ret = DY_EOK;
	dy_device_t *timer6 = dy_find_device("timer6");
	if (NULL == timer6)
	{
		return;
	}
	timer6->ops->control(timer6, BTIMER_IRQ_EXE, NULL);
}

/*********************
 * 
 * timer1和timer10的更新中断
 * 
 *********************/
void TIM1_UP_TIM10_IRQHandler()
{
    int ret = DY_EOK;
    //dy_device_t *timer1 = dy_find_device("timer1");
	dy_device_t *timer = dy_find_device("timer10");
	if (NULL == timer)
	{
		return;
	}
    //timer->ops->control(timer, BTIMER_IRQ_EXE, NULL);
    timer->ops->control(timer, OCTIMER_IRQ_EXE, NULL);
}

/*********************
 * 
 * timer1和timer10的更新中断
 * 
 *********************/
void TIM1_BRK_TIM9_IRQHandler()
{
    int ret = DY_EOK;
    //dy_device_t *timer1 = dy_find_device("timer1");
	dy_device_t *timer = dy_find_device("timer9");
	if (NULL == timer)
	{
		return;
	}
    timer->ops->control(timer, BTIMER_IRQ_EXE, NULL);
}



/*********************
 * 
 *  DMA中断
 * 
 *********************/
void DMA1_Stream1_IRQHandler()
{
    if(DMA_GetITStatus(DMA1_Stream1, DMA_IT_TCIF1) != RESET)
    {
        printf("======================DMA come 1\r\n");
        DMA_ClearITPendingBit(DMA1_Stream1, DMA_IT_TCIF1);
    }
    if(DMA_GetITStatus(DMA1_Stream1, DMA_IT_HTIF1) != RESET)
    {
        printf("======================DMA come 2\r\n");
        DMA_ClearITPendingBit(DMA1_Stream1, DMA_IT_HTIF1);
    }
    if(DMA_GetITStatus(DMA1_Stream1, DMA_IT_TEIF1) != RESET)
    {
        printf("======================DMA come 3\r\n");
        DMA_ClearITPendingBit(DMA1_Stream1, DMA_IT_TEIF1);
    }
    if(DMA_GetITStatus(DMA1_Stream1, DMA_IT_DMEIF1) != RESET)
    {
        printf("======================DMA come 4\r\n");
        DMA_ClearITPendingBit(DMA1_Stream1, DMA_IT_DMEIF1);
    }
    DMA_ClearITPendingBit(DMA1_Stream1, DMA_IT_TCIF1 | DMA_IT_HTIF1 | DMA_IT_TEIF1 | DMA_IT_DMEIF1);
    /*如果是传输完成中断*/

    /*如果是其他中断*/
    printf("======================DMA come in\r\n");
}


void TIM8_CC_IRQHandler() 
{
    float frequency = 0;
    float duty_cycle = 0;
    uint32_t ic1_value = 0;
    uint32_t ic2_value = 0;
    /*清除定时器捕获/比较1中断*/
    TIM_ClearITPendingBit(TIM8, TIM_IT_CC1);

    /* 获取输入捕获值 */
    ic1_value = TIM_GetCapture1(TIM8);
    ic2_value = TIM_GetCapture2(TIM8);
    //printf("ic1_value = %d  ic2_value = %d ", ic1_value, ic2_value);

    /*注意：捕获寄存器CCR1和CCR2的值在计算占空比和频率的时候必须加1*/
    if (ic1_value != 0)
    { 
        /*占空比计算*/
        duty_cycle = (float)((ic2_value+1) * 100) / (ic1_value+1);

        /*频率计算*/
        frequency = 168000000 / (168) / (float)(ic1_value+1);
        printf("占空比：%0.2f%%   频率：%0.2fHz\n", duty_cycle, frequency);
    }
}


/*
WWDG_IRQHandler                                                       
PVD_IRQHandler                                      
TAMP_STAMP_IRQHandler                  
RTC_WKUP_IRQHandler                                
FLASH_IRQHandler                                                       
RCC_IRQHandler                                                            
EXTI0_IRQHandler                                                          
EXTI1_IRQHandler                                                           
EXTI2_IRQHandler                                                          
EXTI3_IRQHandler                                                         
EXTI4_IRQHandler                                                          
DMA1_Stream0_IRQHandler                                       
DMA1_Stream1_IRQHandler                                          
DMA1_Stream2_IRQHandler                                          
DMA1_Stream3_IRQHandler                                          
DMA1_Stream4_IRQHandler                                          
DMA1_Stream5_IRQHandler                                          
DMA1_Stream6_IRQHandler                                          
ADC_IRQHandler                                         
CAN1_TX_IRQHandler                                                            
CAN1_RX0_IRQHandler                                                          
CAN1_RX1_IRQHandler                                                           
CAN1_SCE_IRQHandler                                                           
EXTI9_5_IRQHandler                                                
TIM1_BRK_TIM9_IRQHandler                        
TIM1_UP_TIM10_IRQHandler                      
TIM1_TRG_COM_TIM11_IRQHandler  
TIM1_CC_IRQHandler                                               
TIM2_IRQHandler                                                           
TIM3_IRQHandler                                                           
TIM4_IRQHandler                                                           
I2C1_EV_IRQHandler                                                         
I2C1_ER_IRQHandler                                                         
I2C2_EV_IRQHandler                                                        
I2C2_ER_IRQHandler                                                           
SPI1_IRQHandler                                                          
SPI2_IRQHandler                                                           
USART1_IRQHandler                                                       
USART2_IRQHandler                                                       
USART3_IRQHandler                                                      
EXTI15_10_IRQHandler                                            
RTC_Alarm_IRQHandler                            
OTG_FS_WKUP_IRQHandler                                
TIM8_BRK_TIM12_IRQHandler                      
TIM8_UP_TIM13_IRQHandler                       
TIM8_TRG_COM_TIM14_IRQHandler  
TIM8_CC_IRQHandler                                               
DMA1_Stream7_IRQHandler                                                 
FSMC_IRQHandler                                                            
SDIO_IRQHandler                                                            
TIM5_IRQHandler                                                            
SPI3_IRQHandler                                                            
UART4_IRQHandler                                                          
UART5_IRQHandler                                                          
TIM6_DAC_IRQHandler                            
TIM7_IRQHandler                              
DMA2_Stream0_IRQHandler                                         
DMA2_Stream1_IRQHandler                                          
DMA2_Stream2_IRQHandler                                           
DMA2_Stream3_IRQHandler                                           
DMA2_Stream4_IRQHandler                                        
ETH_IRQHandler                                                         
ETH_WKUP_IRQHandler                                
CAN2_TX_IRQHandler                                                           
CAN2_RX0_IRQHandler                                                          
CAN2_RX1_IRQHandler                                                          
CAN2_SCE_IRQHandler                                                          
OTG_FS_IRQHandler                                                    
DMA2_Stream5_IRQHandler                                          
DMA2_Stream6_IRQHandler                                          
DMA2_Stream7_IRQHandler                                          
USART6_IRQHandler                                                        
I2C3_EV_IRQHandler                                                          
I2C3_ER_IRQHandler                                                          
OTG_HS_EP1_OUT_IRQHandler                           
OTG_HS_EP1_IN_IRQHandler                            
OTG_HS_WKUP_IRQHandler                                
OTG_HS_IRQHandler                                                   
DCMI_IRQHandler                                                            
CRYP_IRQHandler                                                    
HASH_RNG_IRQHandler
FPU_IRQHandler
*/
