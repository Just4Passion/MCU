#include "gd32f30x.h"
#include "gd32f30x_it.h"
#include "system_mng.h"

/****************************************************
 *                  关于任务设计
 * 1. 寄存器组（上下文）
 *      必须记录的寄存器内容
 *          - R0~R3、R12、LR、PC、xPSR
 *          - R4~R11
 *      必须记录的仅存器内容扩展
 *          - R0~R3、R12、LR、PC、xPSR
 *          - R4~R11、S16~S31
 *      可以选择的其他寄存器
 * 
 *****************************************************/
typedef struct {
    uint32_t r0;        //栈顶
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
}register_context_auto;

typedef struct {
    
    uint32_t r4;
}register_context_manu;

