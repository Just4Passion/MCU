
#include "gd32f4xx.h"
#include "drv_fwdgt.h"


/*
控制寄存器：写入0xCCCC可以开启独立看门狗定时器; 写入0xAAAA可以重装载计时器
重载寄存器(FWDGT_RLD)
预分频寄存器(FWDGT_PSC)和FWDGT_RLD都有写保护功能, 在写数据到这些寄存器之前, 都要写0x5555到FWDGT_CTL中
看门狗看门多长时间, 当重启时, 需要看堆栈信息
没有中断号
*/
void drv_fwdgt_init(uint32_t timemout_s)
{
    /*
    首先向FWDGT_RLD中写入0x5555, 然后配置FWDGT_PSC和FWDGT_RLD
    */
    fwdgt_write_enable();
    /*
    配置预分频寄存器和重载寄存器
    从时钟树可以看到，FWDGT的时钟由IRC32K提供: 32000 / 256分频 * s
    12位计数器最大可计数值是 4095， 最大超时时间是32.76s, 与手册一致
    */
    fwdgt_config((timemout_s * 32000) / 256, FWDGT_PSC_DIV256);
    /*启用看门狗*/
    fwdgt_enable();
    fwdgt_counter_reload();
}

void drv_fwdgt_feed(void)
{
    fwdgt_counter_reload();
}




