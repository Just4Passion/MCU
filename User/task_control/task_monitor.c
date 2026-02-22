

#include "FreeRTOS.h"
#include "task.h"


/*********************************************************************
 *                          宏定义
 *********************************************************************/
#define deathSTACK_SIZE    (configMINIMAL_STACK_SIZE + 60)


/*********************************************************************
 *                          局部函数声明
 *********************************************************************/
static void vCreateTasks(void *pvParameters);
static void vSuicidalTask(void *pvParameters);

/*********************************************************************
 * 这个模块的作用是测试"任务管理"
 *      一个后台任务, 创建多个前台任务, 测试任务挂起和恢复
 *      一个被创建的任务, 可以杀死另一个任务
 *********************************************************************/
static volatile UBaseType_t uxTasksRunningAtStart = 0;
static volatile uint16_t usCreationCount = 0;

TaskHandle_t xCreatedTask;


void vCreateSuicidalTask(UBaseType_t uxPriority)
{
    xTaskCreate(vCreateTasks, "TaskCreator", deathSTACK_SIZE, (void*)NULL, uxPriority, NULL);
}

static void vSuicidalTask(void *pvParameters)
{
    volatile long l1, l2;
    TaskHandle_t xTaskToKill;
    const TickType_t xDelay = pdMS_TO_TICKS((TickType_t)200);

    if (pvParameters != NULL)
    {
        xTaskToKill = *(TaskHandle_t*)pvParameters;
    }
    else
    {
        xTaskToKill = NULL;
    }

    for(;;)
    {
        l1 = 2;
        l2 = 89;
        l2 *= l1;

        vTaskDelay(xDelay);

        if (xTaskToKill != NULL)
        {
            vTaskDelay((TickType_t)0);
            vTaskDelete(xTaskToKill);   /*把目标任务杀死*/
            /*自杀后, TCB就被回收了, 再也不会被调用到了, 不用考虑再被执行的问题了*/
            vTaskDelete(NULL);
        }
    }
}

static void vCreateTasks(void *pvParameters)
{
    const TickType_t xDelay = pdMS_TO_TICKS((TickType_t)1000);
    UBaseType_t uxPriority;

    /*确保其他任务都跑起来了*/
    vTaskDelay(xDelay);

    uxTasksRunningAtStart = (UBaseType_t)uxTaskGetNumberOfTasks();
    uxPriority = uxTaskPriorityGet(NULL);

    for (;;)
    {
        /***************************************************
        作为创建者, 挂起的时间很久, 是1000
        两个子任务, 挂起时间是2000
        也就是说, 每次我创建新任务的时候, 两个子任务就已经死光了
        任务队列中不能同时出现4个任务
        如果同时出现的任务超过2个, 很可能会因为访问xCreatedTask而导致崩溃: 可以尝试一下, 比如把xDelay缩短
        
        我把xDelay缩短到20, 这个时候创建了很多Suicidal1, 但是奇迹的是他们居然没有崩溃
        这么多访问同一个xCreatedTask, 居然没有崩溃, 运气好
        ****************************************************/
        vTaskDelay(xDelay);

        xCreatedTask = NULL;
        xTaskCreate( vSuicidalTask, "Suicidal1", configMINIMAL_STACK_SIZE, NULL, uxPriority, &xCreatedTask);
        xTaskCreate( vSuicidalTask, "Suicidal2", configMINIMAL_STACK_SIZE, &xCreatedTask, uxPriority, NULL);
        
        ++usCreationCount;
    }
    
}


