
#include <stdio.h>
#include "gd32f4xx.h" 
#include "drv_conf.h"

#include "FreeRTOS.h"
#include "task.h"

void my_delay(uint32_t count)
{
	while(count--);
}

/*
延时, 也是一个定时器
首先需要一个加载值 Load 
然后可以获取当前值 Val
然后一个中断函数不断地递减Load, 用来模拟硬件递减
*/

void xTask_debug_deal(void *pvParameters)
{
	while(1)
	{
		debug_deal();
	}
}

void xTask_serial_deal(void *pvParameters)
{
	printf("\r\nroot>");
	while(1)
	{
		/*这里在不断的向串口打印字符串*/
		serial_recv_cmd_deal();
	}
}

void xTask_start_scheduler(void *pvParameters)
{
	vTaskStartScheduler();
}


void xTask_led(void *pvParameters)
{
	while (1)
	{
		//led_on();
		my_delay(0xFFFFFFF);
		//led_off();
		my_delay(0xFFFFFFF);
	}
}

void xTask_check(void *pvParameters)
{
	while(1)
    {
        if (xAreSemaphoreTasksStillRunning() != pdTRUE)
		{
			printf("Error IN Semaphore\r\n");
		}
		vTaskDelay(500);
    }
}

void xTask_RTC_showtime(void *pvParameters)
{
	DATE_TIME date_time;
	date_time.year = 2025;
	date_time.month = 10;
    date_time.day = 9;
    date_time.hour = 20;
    date_time.minute = 46;
    date_time.second = 50;
	drv_rtc_init(&date_time);
	while(1)
    {
        vTaskDelay(1000);
		drv_rtc_get_time(&date_time);
		printf("===================%02d-%02d %02d:%02d:%02d\r\n",
			date_time.year, date_time.month, date_time.day, 
			date_time.hour, date_time.minute, date_time.second);
    }
}

int main(void)
{
	/*看门狗*/
    //drv_fwdgt_init(10);
	/*串口初始化*/
	TaskHandle_t xTCBDebugDeal;
	{
		//serial_init();

		/*创建一个任务, 启用调度器*/
		//xTaskCreate(xTask_debug_deal, "DebugDeal", configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 3), &xTCBDebugDeal);
		//xTaskCreate(xTask_serial_deal, "SerialDeal", configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 3), &xTCBDebugDeal);
		//xTaskCreate(xTask_check, "check", configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 3), &xTCBDebugDeal);
	}
	
	{
		vUARTCommandConsoleStart(configMINIMAL_STACK_SIZE * 3, (tskIDLE_PRIORITY + 1));
		//vStartTimerDemoTask(1000);
		//xRegisterSampleCLICommands();
		//xStartSemaphoreTasks((tskIDLE_PRIORITY + 4));
	}
	{
		//vStartLedTimers(1);
	}
	{
		//vStartTaskNotifyTask();
	}
	{
		//vStartRecursiveMutexTasks();
	}
	{
		//vCreateSuicidalTask((tskIDLE_PRIORITY+3));
	}
	{
		//vStartInterruptQueueTasks();
	}
	{
		//xTaskCreate(xTask_RTC_showtime, "RTC", configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY), &xTCBDebugDeal);
	}
	vTaskStartScheduler();
	return 0;
}


