
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define semtstBLOCKING_EXPECTED_VALUE        (( uint32_t )0xfff)
#define semtstNON_BLOCKING_EXPECTED_VALUE    (( uint32_t )0xff)

#define semtstSTACK_SIZE                     configMINIMAL_STACK_SIZE

#define semtstNUM_TASKS                      (4)

#define semtstDELAY_FACTOR                   ((TickType_t)10)

typedef struct SEMPHORE_PARAMETERS
{
    SemaphoreHandle_t xSemaphore;               //信号量句柄
    volatile uint32_t * pulSharedVariable;      //共享变量
    TickType_t xBlockTime;                      //阻塞时间
}xSemaphoreParameters;

static volatile short sCheckVariables[semtstNUM_TASKS] = { 0 };
static volatile short sNextCheckVariable = 0;
static void prvSemaphoreTest(void * pvParameters)
{
    /*获取信号量, 访问共享变量*/
    xSemaphoreParameters *pxParameters;
    volatile uint32_t *pulSharedVariable, ulExpectedValue;

    uint32_t ulCounter;
    short sError = pdFALSE, sCheckVariableToUse;

    portENTER_CRITICAL();
    sCheckVariableToUse = sNextCheckVariable;
    sNextCheckVariable++;
    portEXIT_CRITICAL();

    pxParameters = (xSemaphoreParameters *)pvParameters;
    pulSharedVariable = pxParameters->pulSharedVariable;

    if(pxParameters->xBlockTime > (TickType_t)0)
    {
        ulExpectedValue = semtstBLOCKING_EXPECTED_VALUE;
    }
    else
    {
        ulExpectedValue = semtstNON_BLOCKING_EXPECTED_VALUE;
    }

    while(1)
    {
        if (xSemaphoreTake(pxParameters->xSemaphore, pxParameters->xBlockTime) == pdPASS)
        {
            /*检查共享变量的值是否为预期值*/
            if (*pulSharedVariable != ulExpectedValue)
            {
                sError = pdTRUE;
            }
            /*修改共享变量的值, 期望这个过程中发生任务切换: 时间片用完了
            如果发生了任务切换, 这个时候共享变量的值可能为预期值也可能是其他值*/
            for (ulCounter = 0; ulCounter <= ulExpectedValue; ulCounter++)
            {
                *pulSharedVariable = ulCounter;
                if(*pulSharedVariable != ulCounter)
                {
                    sError = pdTRUE;
                }
            }

            /*释放信号量*/
            if(xSemaphoreGive(pxParameters->xSemaphore) == pdFALSE)
            {
                sError = pdTRUE;
            }

            /*是pdFALSE说明是没有问题*/
            //if (sError == pdFALSE)
            if (sError == pdTRUE)
            {
                if (sCheckVariableToUse < semtstNUM_TASKS)
                {
                    sCheckVariables[sCheckVariableToUse]++;
                }
            }

            /*通过延时, 让其他低优先级任务得到执行*/
            if (pxParameters->xBlockTime != (TickType_t)0)
            {
                vTaskDelay(pxParameters->xBlockTime * semtstDELAY_FACTOR);
            }
        }
        else
        {
            if (pxParameters->xBlockTime == (TickType_t)0)
            {
                /*切换上下文*/
                taskYIELD();
            }
        }
    }
}


void xStartSemaphoreTasks(UBaseType_t uxPriority)
{
    xSemaphoreParameters * pxFirstSemaphoreParameters, * pxSecondSemaphoreParameters;
    const TickType_t xBlockTime = (TickType_t)100;

    /*阻塞时间为0, 无法获取则立马返回*/
    pxFirstSemaphoreParameters = (xSemaphoreParameters *)pvPortMalloc(sizeof(xSemaphoreParameters));
    if (pxFirstSemaphoreParameters != NULL)
    {
        pxFirstSemaphoreParameters->xSemaphore = xSemaphoreCreateBinary();  //创建一个二值信号量, 可用于实现互斥量, 同步等
        if (pxFirstSemaphoreParameters->xSemaphore != NULL)
        {
            xSemaphoreGive(pxFirstSemaphoreParameters->xSemaphore);  //释放信号量, 让其他任务可以访问共享资源
            pxFirstSemaphoreParameters->pulSharedVariable = (uint32_t *) pvPortMalloc(sizeof( uint32_t));
            *(pxFirstSemaphoreParameters->pulSharedVariable) = semtstNON_BLOCKING_EXPECTED_VALUE;
            pxFirstSemaphoreParameters->xBlockTime = (TickType_t)0;

            /*创建任务*/
            xTaskCreate(prvSemaphoreTest, "PolSEM1", semtstSTACK_SIZE, (void*) pxFirstSemaphoreParameters, tskIDLE_PRIORITY, (TaskHandle_t *)NULL);
            xTaskCreate(prvSemaphoreTest, "PolSEM2", semtstSTACK_SIZE, (void*) pxFirstSemaphoreParameters, tskIDLE_PRIORITY, (TaskHandle_t *) NULL);

            /*用于调试*/
            vQueueAddToRegistry((QueueHandle_t)pxFirstSemaphoreParameters->xSemaphore, "Counting_Sem_1");
        }
    }

    pxSecondSemaphoreParameters = (xSemaphoreParameters *) pvPortMalloc(sizeof(xSemaphoreParameters));
    if (pxSecondSemaphoreParameters != NULL)
    {
        pxSecondSemaphoreParameters->xSemaphore = xSemaphoreCreateBinary();  //创建一个二值信号量, 可用于实现互斥量, 同步等
        if (pxSecondSemaphoreParameters->xSemaphore != NULL)
        {
            xSemaphoreGive(pxSecondSemaphoreParameters->xSemaphore);  //释放信号量, 让其他任务可以访问共享资源
            pxSecondSemaphoreParameters->pulSharedVariable = (uint32_t *) pvPortMalloc(sizeof( uint32_t));
            *(pxSecondSemaphoreParameters->pulSharedVariable) = semtstBLOCKING_EXPECTED_VALUE;
            pxSecondSemaphoreParameters->xBlockTime = xBlockTime / portTICK_PERIOD_MS;

            /*创建任务*/
            xTaskCreate(prvSemaphoreTest, "BlkSEM1", semtstSTACK_SIZE, (void*)pxSecondSemaphoreParameters, uxPriority, (TaskHandle_t *)NULL);
            xTaskCreate(prvSemaphoreTest, "BlkSEM2", semtstSTACK_SIZE, (void*)pxSecondSemaphoreParameters, uxPriority, (TaskHandle_t *)NULL);

            /*用于调试*/
            vQueueAddToRegistry((QueueHandle_t)pxSecondSemaphoreParameters->xSemaphore, "Counting_Sem_2");
        }
    }
}

BaseType_t xAreSemaphoreTasksStillRunning(void)
{
    static short sLastCheckVariables[semtstNUM_TASKS] = { 0 };
    BaseType_t xTask, xReturn = pdTRUE;

    for(xTask = 0; xTask < semtstNUM_TASKS; xTask++)
    {
        if(sLastCheckVariables[xTask] != sCheckVariables[xTask])
        {
            xReturn = pdFALSE;
        }
        sLastCheckVariables[xTask] = sCheckVariables[xTask];
    }

    return xReturn;
}





