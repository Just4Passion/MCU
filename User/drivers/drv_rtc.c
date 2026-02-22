
#include "drv_rtc.h"

/*********************************************************************
 * 
 * 
 *                      提供包含年/月/日/时/分/秒/亚秒的日历功能
 *                      时间和日期使用BCD码形式显示(4bit表示10进制数 12=0001 0010, 59=0101 1001)
 *                      亚秒使用二进制码显示
 * 
 * 两个模式可配置的独立的侵入检测
 * 可编程的日历和两个位域可屏蔽的闹钟
 * 可屏蔽的中断源
 *          闹钟 0 和闹钟 1
 *          时间戳检测
 *          侵入检测
 *          自动唤醒事件
 **********************************************************************/

static uint8_t bcd_to_dec(uint8_t val)
{
    uint8_t high = (val >> 4) & 0x0F;  // 右移4位并掩码，得到十位数（0-9）
    uint8_t low = val & 0x0F;          // 直接掩码，得到个位数（0-9）
    return (high * 10) + low;
}

static uint8_t dec_to_bcd(uint8_t val)
{
    // 提取十位数和个位数
    uint8_t tens = val / 10;  // 十位数（例如12 / 10 = 1）
    uint8_t units = val % 10; // 个位数（例如12 % 10 = 2）

    // 组合为BCD码：十位数左移4位，然后与个位数位或
    return (tens << 4) | units;
}


void drv_rtc_init(DATE_TIME *date)
{
    rtc_parameter_struct rtc_initpara;

    // 1. 启用备份域访问
    rcu_periph_clock_enable(RCU_BKPSRAM);
    rcu_periph_clock_enable(RCU_PMU);
    rcu_periph_clock_enable(RCU_RTC);
    pmu_backup_write_enable();

    /*配置时钟源和分频值*/
    rcu_rtc_clock_config(RCU_RTCSRC_HXTAL_DIV_RTCDIV); // 选择LXTAL作为RTC时钟源
    rcu_rtc_div_config(31);
    /*初始化RTC*/
    rtc_register_sync_wait();             // 等待寄存器同步
    
    rtc_deinit();
    rtc_initpara.year = dec_to_bcd(date->year - 2000);
    rtc_initpara.month = dec_to_bcd(date->month);
    rtc_initpara.date = dec_to_bcd(date->day);
    rtc_initpara.hour = dec_to_bcd(date->hour);
    rtc_initpara.minute = dec_to_bcd(date->minute);
    rtc_initpara.second = dec_to_bcd(date->second);
    rtc_initpara.factor_asyn = 0x7F;
    rtc_initpara.factor_syn = 0xFF;
    rtc_initpara.am_pm = RTC_AM;
    rtc_initpara.display_format = RTC_24HOUR;
    rtc_init(&rtc_initpara);
}


void drv_rtc_set_time()
{

}

void drv_rtc_get_time(DATE_TIME *date)
{
    rtc_parameter_struct rtc_cur_time;
    rtc_current_time_get(&rtc_cur_time);

    date->year = 2000 + bcd_to_dec(rtc_cur_time.year);
    date->month = bcd_to_dec(rtc_cur_time.month);
    date->day = bcd_to_dec(rtc_cur_time.date);
    date->hour = bcd_to_dec(rtc_cur_time.hour);
    date->minute = bcd_to_dec(rtc_cur_time.minute);
    date->second = bcd_to_dec(rtc_cur_time.second);
}





