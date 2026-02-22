
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


/*********************************************************************
 *                              宏定义
 * *******************************************************************/
#define recmuRECURSIVE_MUTEX_TEST_TASK_STACK_SIZE    configMINIMAL_STACK_SIZE


#define recmuCONTROLLING_TASK_PRIORITY               (tskIDLE_PRIORITY + 2)
#define recmuBLOCKING_TASK_PRIORITY                      (tskIDLE_PRIORITY + 1)
#define recmuPOLLING_TASK_PRIORITY                       (tskIDLE_PRIORITY + 0)


#define recmuMAX_COUNT                                   (10)

#define recmu15ms_DELAY                                  (pdMS_TO_TICKS(15))
#define recmuSHORT_DELAY                                 (pdMS_TO_TICKS(20))
#define recmuNO_DELAY                                    ((TickType_t )0)

/*********************************************************************
 *                              局部函数声明
 * *******************************************************************/
static void prvRecursiveMutexControllingTask(void * pvParameters);
static void prvRecursiveMutexBlockingTask(void * pvParameters);
static void prvRecursiveMutexPollingTask(void * pvParameters);

/*********************************************************************
 *                              局部变量声明
 * *******************************************************************/
static TaskHandle_t xControllingTaskHandle, xBlockingTaskHandle;

static volatile UBaseType_t uxControllingCycles = 0, uxBlockingCycles = 0, uxPollingCycles = 0;
static volatile BaseType_t xErrorOccurred = pdFALSE, xControllingIsSuspended = pdFALSE, xBlockingIsSuspended = pdFALSE;
/*
递归, 需要记录递归的深度, 这样在解锁的时候可以知道是第几层调用
*/
static SemaphoreHandle_t xMutex;

/*********************************************************************
 * Controlling任务
 *          测试递归锁, 递归的深度 : 同一个任务, 上锁的深度要和解锁的深度匹配
 *          解锁完毕后, 把自己挂起, 等待唤醒
 *          由于它的优先级最高, 先执行; 先执行, 先拿到锁, 因此就算xTaskDelay, 其他任务也得不到执行
 * Blocking任务
 *          获取锁, 确认Controlling被阻塞了, 释放锁, 挂起自己, 让Polling线程去获取互斥量
 * Polling任务
 *          Controlling和Blocking都不在占有锁, 同时都被挂起了
 *          先唤醒Blocking, Blocking优先级高恢复执行, 但是在获取锁的时候, 发现已经被Polling获取了, 被阻塞
 *          Polling获取了锁, 阻塞了Blocking, 这个时候Polling的优先级提升到Blocking, 获取CPU控制权, 继续执行
 *          Polling唤醒Controlling, Controlling恢复执行, 但是在获取锁的时候, 发现已经被Polling获取了, 被阻塞
 *          Polling占用锁, 阻塞了Controlling, 优先级提升到Controlling, 释放锁后, 恢复之前的有限, 然后Controlling再次获取控制权
 * 启用打印
 *      Blocking resumed by Polling
 *      Polling will resume Controlling
 *      Take the mutex, Polling task priority = 1
 *      Controlling resumed by Polling
 *      Take the mutex, Polling task priority = 2
 *      Give the mutex, Polling task priority = 0
 * *******************************************************************/
void vStartRecursiveMutexTasks(void)
{
    xMutex = xSemaphoreCreateRecursiveMutex();

    if (xMutex != NULL)
    {
        vQueueAddToRegistry((QueueHandle_t)xMutex, "Recursive_Mutex");

        xTaskCreate(prvRecursiveMutexControllingTask, 
                "Rec1", 
                recmuRECURSIVE_MUTEX_TEST_TASK_STACK_SIZE, 
                NULL, recmuCONTROLLING_TASK_PRIORITY, &xControllingTaskHandle);
        
        xTaskCreate(prvRecursiveMutexBlockingTask,
            "Rec2", 
            recmuRECURSIVE_MUTEX_TEST_TASK_STACK_SIZE, 
            NULL, recmuBLOCKING_TASK_PRIORITY, &xBlockingTaskHandle);

        xTaskCreate( prvRecursiveMutexPollingTask, "Rec3", 
            recmuRECURSIVE_MUTEX_TEST_TASK_STACK_SIZE, 
            NULL, recmuPOLLING_TASK_PRIORITY, NULL );
    }
}

static void prvRecursiveMutexControllingTask(void * pvParameters)
{
    UBaseType_t ux;
    for (;;)
    {
        /*解锁: 一开始就有资源, 解锁必然失败*/
        if (xSemaphoreGiveRecursive(xMutex) == pdPASS)
        {
            printf("OS error, recursive mutex Give test failed\r\n", ux);
        }
        for (ux = 0; ux < recmuMAX_COUNT; ux++)
        {
            /*这里我要上锁, 指导递归深度*/
            if (xSemaphoreTakeRecursive(xMutex, recmu15ms_DELAY) != pdPASS)
            {
                printf("The %dth xSemaphoreTakeRecursive failed\r\n", ux);
            }
            vTaskDelay(recmuSHORT_DELAY);
        }
        for (ux = 0; ux < recmuMAX_COUNT; ux++)
        {
            vTaskDelay(recmuSHORT_DELAY);
            if( xSemaphoreGiveRecursive(xMutex ) != pdPASS)
            {
                printf("The %dth xSemaphoreGiveRecursive failed\r\n", ux);
            }
        }

        /*都解锁完了, 再解锁会失败*/
        if(xSemaphoreGiveRecursive(xMutex) == pdPASS)
        {
            printf("OS error, recursive mutex Take test failed\r\n", ux);
        }
        uxControllingCycles++;

        xControllingIsSuspended = pdTRUE;
        /*把自己挂起来, 等待唤醒*/
        vTaskSuspend(NULL);
        xControllingIsSuspended = pdFALSE;
        //printf("Controlling resumed by Polling\r\n");
    }
}

static void prvRecursiveMutexBlockingTask(void * pvParameters)
{
    /*Blocking是如何阻塞的*/
    for (;;)
    {
        if (xSemaphoreTakeRecursive(xMutex, (portMAX_DELAY - 1)) == pdPASS)
        {
            if (xControllingIsSuspended != pdTRUE)
            {
                /*高优先级优先执行, 除非被低优先级的任务阻塞, 否则会一直执行, 直到高优先级任务被挂起
                所有只有Control被挂起之后, Blocking才有机会执行
                */
            }
            else
            {
                /*控制线程被挂起了*/
                if (xSemaphoreGiveRecursive( xMutex ) != pdPASS)
                {
                    printf("Blocking: The xSemaphoreGiveRecursive failed\r\n");
                }
                xBlockingIsSuspended = pdTRUE;
                /*Blocking也把自己挂起来, 让polling线程去获取互斥量*/
                vTaskSuspend(NULL);
                xBlockingIsSuspended = pdFALSE;
                //printf("Blocking resumed by Polling\r\n");
            }
        }

        if (uxControllingCycles != (uxBlockingCycles + 1))
        {

        }
        uxBlockingCycles++;
    }
}

/*优先级最低的线程, 需要等待其他高优先执行完或阻塞才能执行*/
static void prvRecursiveMutexPollingTask(void * pvParameters)
{
    for(;;)
    {
        if (xSemaphoreTakeRecursive(xMutex, recmuNO_DELAY) == pdPASS)
        {
            if ((xBlockingIsSuspended != pdTRUE) || (xControllingIsSuspended != pdTRUE))
            {
                /*必须两个都被挂起, 然后polling可以去唤醒他们*/
            }
            else
            {
                /*polling已经获得了互斥量, 这个时候唤醒了某个任务, 就有了那个任务的优先级*/
                uxPollingCycles++;

                /*首先唤醒阻塞任务*/
                vTaskResume(xBlockingTaskHandle);
                //printf("Polling will resume Controlling\r\n");
                //printf("Take the mutex, Polling task priority = %d\r\n", uxTaskPriorityGet(NULL));
                vTaskResume(xControllingTaskHandle);
                /**/
                /*唤醒之后阻塞任务开始执行*/
                if ((xBlockingIsSuspended == pdTRUE) || (xControllingIsSuspended == pdTRUE))
                {
                    /*唤醒开始执行, 但是由于polling已经获取互斥量, 所有blocking要被阻塞在take那里*/
                    printf("OS error, recursive mutex Take test failed\r\n");
                }
                /*由于polling阻塞了blocking, 同时也阻塞了controlling, 它的优先级被临时提升为被阻塞的最高的任务的优先级*/
                //printf("Take the mutex, Polling task priority = %d\r\n", uxTaskPriorityGet(NULL));

                /*释放互斥量*/
                if (xSemaphoreGiveRecursive( xMutex ) != pdPASS)
                {

                }
                //printf("Give the mutex, Polling task priority = %d\r\n", uxTaskPriorityGet(NULL));
            }
        }
    }
}




