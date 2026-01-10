
#include <stdio.h>

#include "kservice.h"

#include "drv_rtc.h"

#define RTC_BASE_YEAR   (1970)

/*rtc时间*/
typedef struct
{
    /**/
    uint32_t hour_format;   //时间格式, 24小时/12小时
    uint32_t async_predivider;  //异步分频因子
    uint32_t sync_predivider;   //同步分频因子
}rtc_cfg_t;

typedef struct
{
    dy_device_t device;
    rtc_cfg_t cfg;

}rtc_dev_t;

rtc_dev_t g_rtc_dev;


/**
 * @brief 初始化接口
 */
static int rtc_init(dy_device_t *dev)
{
    rtc_dev_t *rtc_dev = (rtc_dev_t *)dev;

    RTC_InitTypeDef stRTCInit = {0};

    /*RTC与低功耗模式以及备份寄存器有关*/
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    /* PWR_CR:DBF置1，使能RTC、RTC备份寄存器和备份SRAM的访问 */
    PWR_BackupAccessCmd(ENABLE);

    /*LSI和LSE*/
#define RTC_CLOCK_SOURCE_LSI
    /*选择时钟源*/
#if defined (RTC_CLOCK_SOURCE_LSI)
    /* 使用LSI作为RTC时钟源会有误差,仅仅是为了实验方便 */
    /* 使能LSI */
    RCC_LSICmd(ENABLE);
    /* 等待LSI稳定 */
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
    {
    }
    /* 选择LSI做为RTC的时钟源 */
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
#elif defined (RTC_CLOCK_SOURCE_LSE)
    /* 使能LSE */
    RCC_LSEConfig(RCC_LSE_ON);
    /* 等待LSE稳定 */
    while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET) 
    {
    }
    /* 选择LSE做为RTC的时钟源 */
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);

#endif /* RTC_CLOCK_SOURCE_LSI */

    /* 使能RTC时钟 */
    RCC_RTCCLKCmd(ENABLE);

    /* 等待 RTC APB 寄存器同步 */
    RTC_WaitForSynchro();

    /* 设置异步预分频器的值*/
    stRTCInit.RTC_AsynchPrediv = rtc_dev->cfg.async_predivider;
    /* 设置同步预分频器的值 */
    stRTCInit.RTC_SynchPrediv = rtc_dev->cfg.sync_predivider;
    stRTCInit.RTC_HourFormat = rtc_dev->cfg.hour_format;
    /* 用RTC_InitStructure的内容初始化RTC寄存器 */
    if (RTC_Init(&stRTCInit) == ERROR)
    {
        printf("rtc init failed\r\n");
        return DY_ERROR;
    }
    return DY_EOK;
}

/**
 * @brief 设置时间
 */
static void rtc_set_date_time(dy_device_t *dev, const rtc_date_time *date_time)
{
    rtc_dev_t *rtc_dev = (rtc_dev_t *)dev;
    RTC_TimeTypeDef stRTCTime;
    RTC_DateTypeDef stRTCDate;

    /*初始化日期*/
    stRTCDate.RTC_WeekDay = date_time->weekday; 
    stRTCDate.RTC_Date = date_time->date;           //这里需要一个1-31的数据
    stRTCDate.RTC_Month = date_time->month;         
    stRTCDate.RTC_Year = (date_time->year - RTC_BASE_YEAR);   //这里需要一个0-99的数据
    RTC_SetDate(RTC_Format_BIN, &stRTCDate);
    RTC_WriteBackupRegister(RTC_BKP_DR0, 0x32F2);     //在备份寄存器中写入一个数据, 用于标记RTC时间是否配置过

    /*初始化时间*/
    //stRTCTime.RTC_H12 = date_time->RTC_H12;
    stRTCTime.RTC_Hours = date_time->hours;     //需要根据rtc_dev->cfg.hour_format做一些判断
    stRTCTime.RTC_Minutes = date_time->minutes;
    stRTCTime.RTC_Seconds = date_time->seconds;
    RTC_SetTime(RTC_Format_BIN, &stRTCTime);
    RTC_WriteBackupRegister(RTC_BKP_DR0, 0x32F2);
}

/**
 * @brief 获取时间
 */
static void rtc_get_date_time(dy_device_t *dev, rtc_date_time *date_time)
{
    RTC_TimeTypeDef stRTCTime;
    RTC_DateTypeDef stRTCDate;

    RTC_GetTime(RTC_Format_BIN, &stRTCTime);
    RTC_GetDate(RTC_Format_BIN, &stRTCDate);

    /*时间*/
    date_time->hours = stRTCTime.RTC_Hours;
    date_time->minutes = stRTCTime.RTC_Minutes;
    date_time->seconds = stRTCTime.RTC_Seconds;

    /*日期*/
    date_time->weekday = stRTCDate.RTC_WeekDay;
    date_time->date = stRTCDate.RTC_Date;
    date_time->month = stRTCDate.RTC_Month;
    date_time->year = (stRTCDate.RTC_Year + RTC_BASE_YEAR);
}

/**
 * @brief 控制接口
 */
static int rtc_control(dy_device_t *dev, int cmd, void *arg)
{
    int ret = DY_EOK;
    switch(cmd)
    {
        case RTC_SET_DATE_TIME:
            rtc_set_date_time(dev, (const rtc_date_time *)arg);
            break;
        case RTC_GET_DATE_TIME:
            rtc_get_date_time(dev, (rtc_date_time *)arg);
            break;
        default:
            ret = dy_device_control(dev, cmd, arg);
            break;
    }
    return ret;
}

device_ops_t g_rtc_ops = {
    .init = rtc_init,
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .control = rtc_control,
    .callback = NULL
};

/**
 * @brief RTC初始化
 */
int drv_rtc_init()
{
    int ret = 0;

    strcpy(g_rtc_dev.device.name, "rtc");
    g_rtc_dev.device.ops = &g_rtc_ops;

    g_rtc_dev.cfg.hour_format = RTC_HourFormat_24;
    g_rtc_dev.cfg.async_predivider = 0x7F;
    g_rtc_dev.cfg.sync_predivider = 0xFF;

    /*注册*/
    if (DY_EOK != dy_device_register("rtc", &g_rtc_dev.device))
    {
        return DY_ERROR;
    }
    /*初始化*/
    ret = g_rtc_dev.device.ops->init(&g_rtc_dev.device);
    if (DY_EOK != ret)
    {
        return DY_ERROR;
    }
    return DY_EOK;
}

