

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gd32f4xx.h"

/*FreeRTOS相关头文件*/
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


/*serial.c相关头文件*/
#include "serial.h"

/*命令行头文件*/
#include "rtos_cli.h"

/****************************************************************************
 * 
 *     串口命令接收和回显, 考虑到很多线程都要使用串口, 这里使用了互斥量进行访问
 *     需要互斥访问的是串口发送接口, 由于很多线程都需要往发送队列中添加数据
 *     在访问发送队列的时候, 使用的是同一个互斥量, 进行互斥访问
 * 
 ***************************************************************************/
typedef void * xComPortHandle;

/*队列长度*/
#define cmdMAX_INPUT_SIZE               50        /* 最大输入长度 */
#define cmdQUEUE_LENGTH                 25

#define cmdASCII_DEL                    ( 0x7F )    /*ASCII: Del*/

/*用到了互斥量, 超时等待时间*/
#define cmdMAX_MUTEX_WAIT          pdMS_TO_TICKS( 300 )
/*波特率*/
#define configCLI_BAUD_RATE    115200

static void prvUARTCommandConsoleTask( void * pvParameters );
void vUARTCommandConsoleStart( uint16_t usStackSize,
                               UBaseType_t uxPriority );
void vOutputString( const char * const pcMessage );

static void prvUARTCommandConsoleTask_MeCLI(void * pvParameters);

/*固定的输出*/
static const char * const pcWelcomeMessage = "FreeRTOS command server.\r\nType Help to view a list of registered commands.\r\n\r\n>";
static const char * const pcEndOfOutputMessage = "\r\n[Press ENTER to execute the previous command again]\r\n>";
static const char * const pcNewLine = "\r\n";


/*信号量, 用来互斥访问的, 如果是多线程, 信号量减1后, 其他的得等待
等待就会被挂起, 直到被通知可以执行 - 切换成就绪态就行, 或者被超时唤醒
这里是主动唤醒, 还是等待下一次被调度??*/
static SemaphoreHandle_t xTxMutex = NULL;

/*控制台句柄*/
static xComPortHandle xPort = 0;

static TaskHandle_t xTCBDebugDeal;



void vUARTCommandConsoleStart(uint16_t usStackSize, UBaseType_t uxPriority)
{
    xTxMutex = xSemaphoreCreateMutex();
    configASSERT(xTxMutex);

    xTaskCreate(prvUARTCommandConsoleTask_MeCLI, "CLI", usStackSize, NULL, uxPriority, &xTCBDebugDeal);
}

#ifdef RTOS_CLI
static void prvUARTCommandConsoleTask(void * pvParameters)
{
    signed char cRxedChar;      //接收到的字符
    uint8_t ucInputIndex = 0;   //输入的字符索引
    char * pcOutputString;      //
    static char cInputString[ cmdMAX_INPUT_SIZE ], cLastInputString[ cmdMAX_INPUT_SIZE ];
    BaseType_t xReturned;

    pcOutputString = FreeRTOS_CLIGetOutputBuffer();
    
    serial_init();

    serial_put_string(pcWelcomeMessage, (unsigned short)strlen(pcWelcomeMessage));
 
    for (;;)
    {
        while(serial_get_char(&cRxedChar, portMAX_DELAY) != pdPASS)
        {

        }

        if (xSemaphoreTake(xTxMutex, cmdMAX_MUTEX_WAIT) == pdPASS)
        {
            serial_put_char(cRxedChar, portMAX_DELAY);

            if ((cRxedChar == '\n') || (cRxedChar == '\r'))
            {
                serial_put_string(pcNewLine, (unsigned short)strlen(pcNewLine));

                if (ucInputIndex == 0)
                {
                    /*如果命令是空的, 把上一个命令复制到cInputString中
                    这是为了 ENTER 直接执行上一个命令*/
                    strcpy(cInputString, cLastInputString);
                }

                /*执行命令*/
                do
                {
                    xReturned = FreeRTOS_CLIProcessCommand(cInputString, pcOutputString, 256);
                    //xReturned = cmd_process(cInputString, pcOutputString, 512);

                    serial_put_string(pcOutputString, (unsigned short)strlen(pcOutputString));
                }while(xReturned != pdFALSE);   /*如果命令处理函数没有返回pdFALSE会一直执行*/

                strcpy(cLastInputString, cInputString );
                ucInputIndex = 0;
                memset( cInputString, 0x00, cmdMAX_INPUT_SIZE);
                serial_put_string(pcEndOfOutputMessage, (unsigned short)strlen(pcEndOfOutputMessage));
            }
            else
            {
                if (cRxedChar == '\r')
                {

                }
                else if ((cRxedChar == '\b') || (cRxedChar == cmdASCII_DEL))
                {
                    /*删除一个字符*/
                    if (ucInputIndex > 0)
                    {
                        ucInputIndex--;
                        cInputString[ucInputIndex] = 0;
                    }
                }
                else
                {
                    if((cRxedChar >= ' ') && (cRxedChar <= '~'))
                    {
                        if(ucInputIndex < cmdMAX_INPUT_SIZE)
                        {
                            cInputString[ucInputIndex] = cRxedChar;
                            ucInputIndex++;
                        }
                    }
                }
            }
            /*这里相当于一个解锁*/
            xSemaphoreGive(xTxMutex);
        }
    }
}

void vOutputString( const char * const pcMessage )
{
    if (xSemaphoreTake(xTxMutex, cmdMAX_MUTEX_WAIT) == pdPASS)
    {
        serial_put_string(pcMessage, (unsigned short)strlen(pcMessage));
        xSemaphoreGive(xTxMutex);
    }
}

/****************************************************************************
 * 
 *                          命令行接口测试
 *  注册命令
 *  执行命令
 * 
 ***************************************************************************/

#ifndef  configINCLUDE_TRACE_RELATED_CLI_COMMANDS
    #define configINCLUDE_TRACE_RELATED_CLI_COMMANDS    0
#endif

#ifndef configINCLUDE_QUERY_HEAP_COMMAND
    #define configINCLUDE_QUERY_HEAP_COMMAND    0
#endif

void vRegisterSampleCLICommands(void);

/*展示任务状态*/
static BaseType_t prvTaskStatsCommand( char * pcWriteBuffer,
    size_t xWriteBufferLen,
    const char * pcCommandString );

/*展示运行时间*/
#if ( configGENERATE_RUN_TIME_STATS == 1 )
static BaseType_t prvRunTimeStatsCommand( char * pcWriteBuffer,
                                            size_t xWriteBufferLen,
                                            const char * pcCommandString );
#endif /* configGENERATE_RUN_TIME_STATS */

/*3个参数的回显命令*/
static BaseType_t prvThreeParameterEchoCommand( char * pcWriteBuffer,
    size_t xWriteBufferLen,
    const char * pcCommandString );

static BaseType_t prvParameterEchoCommand( char * pcWriteBuffer,
    size_t xWriteBufferLen,
    const char * pcCommandString );

static BaseType_t prvRebootCommand(char * pcWriteBuffer,
    size_t xWriteBufferLen,
    const char * pcCommandString);

#if ( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
static BaseType_t prvQueryHeapCommand( char * pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char * pcCommandString );
#endif

#if ( configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1 )
static BaseType_t prvStartStopTraceCommand( char * pcWriteBuffer,
                                            size_t xWriteBufferLen,
                                            const char * pcCommandString );
#endif


static const CLI_Command_Definition_t xTaskStats =
{
    "task-stats",        /*命令名称*/
    "\r\ntask-stats:\r\n Displays a table showing the state of each FreeRTOS task\r\n",
    prvTaskStatsCommand, /*命令处理函数*/
    0                    /*无需参数*/
};

static const CLI_Command_Definition_t xThreeParameterEcho =
{
    "echo-3-parameters",
    "\r\necho-3-parameters <param1> <param2> <param3>:\r\n Expects three parameters, echos each in turn\r\n",
    prvThreeParameterEchoCommand, /*命令处理函数*/
    3                              /*需要3个入参参数*/
};

static const CLI_Command_Definition_t xParameterEcho =
{
    "echo-parameters",
    "\r\necho-parameters <...>:\r\n Take variable number of parameters, echos each in turn\r\n",
    prvParameterEchoCommand, /*命令处理函数*/
    -1                       /* 不限制数量 */
};

static const CLI_Command_Definition_t xReboot =
{
    "reboot",
    "\r\nreboot:\r\n reboot system\r\n",
    prvRebootCommand,
    0
};

#if ( configGENERATE_RUN_TIME_STATS == 1 )
    static const CLI_Command_Definition_t xRunTimeStats =
    {
        "run-time-stats",       /*主要关注它是如何实现的*/
        "\r\nrun-time-stats:\r\n Displays a table showing how much processing time each FreeRTOS task has used\r\n",
        prvRunTimeStatsCommand, /*命令处理函数*/
        0                       /*无需参数*/
    };
#endif

#if ( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
    static const CLI_Command_Definition_t xQueryHeap =
    {
        "query-heap",
        "\r\nquery-heap:\r\n Displays the free heap space, and minimum ever free heap space.\r\n",
        prvQueryHeapCommand, /*命令处理函数*/
        0                    /*无需参数*/
    };
#endif

#if configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1
    static const CLI_Command_Definition_t xStartStopTrace =
    {
        "trace",
        "\r\ntrace [start | stop]:\r\n Starts or stops a trace recording for viewing in FreeRTOS+Trace\r\n",
        prvStartStopTraceCommand, /*处理函数*/
        1                         /*参数是"start" and "stop". */
    };
#endif


void xRegisterSampleCLICommands(void)
{
    FreeRTOS_CLIRegisterCommand(&xTaskStats);
    FreeRTOS_CLIRegisterCommand(&xThreeParameterEcho);
    FreeRTOS_CLIRegisterCommand(&xParameterEcho);
    FreeRTOS_CLIRegisterCommand(&xReboot);
    #if ( configGENERATE_RUN_TIME_STATS == 1 )
    {
        FreeRTOS_CLIRegisterCommand(&xRunTimeStats);
    }
    #endif

    #if ( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
    {
        FreeRTOS_CLIRegisterCommand(&xQueryHeap);
    }
    #endif

    #if ( configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1 )
    {
        FreeRTOS_CLIRegisterCommand(&xStartStopTrace);
    }
    #endif
}


static BaseType_t prvTaskStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    const char * const pcHeader = "     State   Priority  Stack    #\r\n************************************************\r\n";
    BaseType_t xSpacePadding;

    /*遍历任务队列链表, 获取每个任务的状态*/
    configASSERT(pcWriteBuffer);
    strcpy(pcWriteBuffer, "Task");

    configASSERT(configMAX_TASK_NAME_LEN > 3);

    for (xSpacePadding = strlen("Task"); xSpacePadding <= (configMAX_TASK_NAME_LEN -3); xSpacePadding++)
    {
        *pcWriteBuffer = ' ';
        pcWriteBuffer++;
        *pcWriteBuffer = 0x00;
    }
    strcpy(pcWriteBuffer, pcHeader);

    vTaskList(pcWriteBuffer + strlen( pcHeader));

    return pdFALSE;
}

#if ( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
    static BaseType_t prvQueryHeapCommand( char * pcWriteBuffer,
                                           size_t xWriteBufferLen,
                                           const char * pcCommandString )
    {
        /* 避免编译器警告*/
        ( void ) pcCommandString;
        ( void ) xWriteBufferLen;
        configASSERT( pcWriteBuffer );

        sprintf(pcWriteBuffer, "Current free heap %d bytes, minimum ever free heap %d bytes\r\n", 
            (int) xPortGetFreeHeapSize(), (int) xPortGetMinimumEverFreeHeapSize() );

        
        return pdFALSE;
    }

#endif 


#if ( configGENERATE_RUN_TIME_STATS == 1 )
static BaseType_t prvRunTimeStatsCommand( char * pcWriteBuffer,
    size_t xWriteBufferLen,
    const char * pcCommandString )
{
    const char * const pcHeader = "  Abs Time      % Time\r\n****************************************\r\n";
    BaseType_t xSpacePadding;

    ( void ) pcCommandString;
    ( void ) xWriteBufferLen;
    configASSERT( pcWriteBuffer );

    strcpy( pcWriteBuffer, "Task" );
    pcWriteBuffer += strlen( pcWriteBuffer );

    for( xSpacePadding = strlen( "Task" ); xSpacePadding < ( configMAX_TASK_NAME_LEN - 3 ); xSpacePadding++ )
    {
        *pcWriteBuffer = ' ';
        pcWriteBuffer++;

        *pcWriteBuffer = 0x00;
    }

    strcpy( pcWriteBuffer, pcHeader );
    vTaskGetRunTimeStats( pcWriteBuffer + strlen( pcHeader ) );

    return pdFALSE;
}
#endif

static BaseType_t prvThreeParameterEchoCommand( char * pcWriteBuffer,
    size_t xWriteBufferLen,
    const char * pcCommandString )
{
    const char * pcParameter;
    BaseType_t xParameterStringLength, xReturn;
    static UBaseType_t uxParameterNumber = 0;

    ( void ) pcCommandString;
    ( void ) xWriteBufferLen;
    configASSERT( pcWriteBuffer );

    if( uxParameterNumber == 0 )
    {
        sprintf( pcWriteBuffer, "The three parameters were:\r\n" );
        uxParameterNumber = 1U;

        xReturn = pdPASS;
    }
    else
    {
        /*返回第几个参数*/
        pcParameter = FreeRTOS_CLIGetParameter(
            pcCommandString,        /* The command string itself. */
            uxParameterNumber,      /* Return the next parameter. */
            &xParameterStringLength /* Store the parameter string length. */
        );

        configASSERT(pcParameter);

        /* Return the parameter string. */
        memset( pcWriteBuffer, 0x00, xWriteBufferLen );
        sprintf( pcWriteBuffer, "%d: ", ( int ) uxParameterNumber );
        strncat( pcWriteBuffer, pcParameter, ( size_t ) xParameterStringLength );
        strncat( pcWriteBuffer, "\r\n", strlen( "\r\n" ) );

  
        if( uxParameterNumber == 3U )
        {
            xReturn = pdFALSE;
            uxParameterNumber = 0;
        }
        else
        {
            xReturn = pdTRUE;
            uxParameterNumber++;
        }
    }

    return xReturn;
}


static BaseType_t prvParameterEchoCommand(char * pcWriteBuffer,
    size_t xWriteBufferLen,
    const char * pcCommandString)
{
    const char * pcParameter;
    BaseType_t xParameterStringLength, xReturn;
    static UBaseType_t uxParameterNumber = 0;

    (void) pcCommandString;
    (void) xWriteBufferLen;
    configASSERT(pcWriteBuffer);

    if( uxParameterNumber == 0 )
    {
        sprintf( pcWriteBuffer, "The parameters were:\r\n" );

        uxParameterNumber = 1U;

        xReturn = pdPASS;
    }
    else
    {
        /* Obtain the parameter string. */
        pcParameter = FreeRTOS_CLIGetParameter
        (
        pcCommandString,        /* The command string itself. */
        uxParameterNumber,      /* Return the next parameter. */
        &xParameterStringLength /* Store the parameter string length. */
        );

        if( pcParameter != NULL )
        {
            /* Return the parameter string. */
            memset( pcWriteBuffer, 0x00, xWriteBufferLen );
            sprintf( pcWriteBuffer, "%d: ", ( int ) uxParameterNumber );
            strncat( pcWriteBuffer, ( char * ) pcParameter, ( size_t ) xParameterStringLength );
            strncat( pcWriteBuffer, "\r\n", strlen( "\r\n" ) );

            /* There might be more parameters to return after this one. */
            xReturn = pdTRUE;
            uxParameterNumber++;
        }
        else
        {
            /* No more parameters were found.  Make sure the write buffer does
            * not contain a valid string. */
            pcWriteBuffer[ 0 ] = 0x00;

            /* No more data to return. */
            xReturn = pdFALSE;

            /* Start over the next time this command is executed. */
            uxParameterNumber = 0;
        }
    }

    return xReturn;
}

static BaseType_t prvRebootCommand(char * pcWriteBuffer,
    size_t xWriteBufferLen,
    const char * pcCommandString)
{
    NVIC_SystemReset();
    return pdFALSE;
}

#endif



/****************************************************************************
 * 
 *                         定时器: 用于统计任务运行时间
 * 
 ***************************************************************************/

/*启动定时器*/
void vSetupTimerForRunTimeStats(void)
{
    /*定时器初始化*/
    /*定时器时钟来源*/
    /*定时器分频配置*/
    /*定时器重载值设置*/
    /*启动定时器*/

    /*
    TIMER5和TIMER6, 基本定时器, 16位, 内部时钟, 向上计数, 16预分频
    内部时钟: 16M RC; 48M RC; 32K RC
    TIMER5和TIMER6是挂在APB1总线上, 最大时钟42MHz
    初始化的时候使用的时钟是168M, APB1分频4, 为42M
    */
    #if 1
    timer_parameter_struct timer_initpara;
    rcu_periph_clock_enable(RCU_TIMER6);
	
    timer_deinit(TIMER6);
    timer_struct_para_init(&timer_initpara);
    timer_initpara.prescaler 		= 42;
    timer_initpara.alignedmode		= TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection	= TIMER_COUNTER_UP;
    timer_initpara.period			= 999;
    timer_initpara.clockdivision 	= TIMER_CKDIV_DIV1;
    timer_init(TIMER6, &timer_initpara);

    //timer_interrupt_enable(TIMER6, TIMER_INT_UP);
    timer_enable(TIMER6);
    //nvic_priority_group_set(NVIC_PRIGROUP_PRE1_SUB3);
    //nvic_irq_enable(TIMER6_IRQn, 15, 0);
    #endif
}
/*获取定时器计数值*/
uint32_t ulGetRunTimeCounterValue(void)
{
    //return 0;
    return timer_counter_read(TIMER6);
}


/****************************************************************************
 * 
 *                         me_rtos_cli.c测试
 * 
 ***************************************************************************/
extern char *cmd_get_out_buffer();
static void prvUARTCommandConsoleTask_MeCLI(void * pvParameters)
{
    signed char cRxedChar;      //接收到的字符
    uint8_t ucInputIndex = 0;   //输入的字符索引
    char * pcOutputString;      //
    static char cInputString[ cmdMAX_INPUT_SIZE ], cLastInputString[ cmdMAX_INPUT_SIZE ];
    BaseType_t xReturned;

    pcOutputString = cmd_get_out_buffer();
    
    serial_init();

    serial_put_string(pcWelcomeMessage, (unsigned short)strlen(pcWelcomeMessage));
 
    for (;;)
    {
        while(serial_get_char(&cRxedChar, portMAX_DELAY) != pdPASS)
        {

        }

        if (xSemaphoreTake(xTxMutex, cmdMAX_MUTEX_WAIT) == pdPASS)
        {
            serial_put_char(cRxedChar, portMAX_DELAY);

            if ((cRxedChar == '\n') || (cRxedChar == '\r'))
            {
                serial_put_string(pcNewLine, (unsigned short)strlen(pcNewLine));

                if (ucInputIndex == 0)
                {
                    /*如果命令是空的, 把上一个命令复制到cInputString中
                    这是为了 ENTER 直接执行上一个命令*/
                    strcpy(cInputString, cLastInputString);
                }

                /*执行命令*/
                do
                {
                    xReturned = cmd_process(cInputString, pcOutputString, 1024);

                    serial_put_string(pcOutputString, (unsigned short)strlen(pcOutputString));
                }while(xReturned != pdFALSE);   /*如果命令处理函数没有返回pdFALSE会一直执行*/

                strcpy(cLastInputString, cInputString );
                ucInputIndex = 0;
                memset(cInputString, 0x00, cmdMAX_INPUT_SIZE);
                serial_put_string(pcEndOfOutputMessage, (unsigned short)strlen(pcEndOfOutputMessage));
            }
            else
            {
                if (cRxedChar == '\r')
                {

                }
                else if ((cRxedChar == '\b') || (cRxedChar == cmdASCII_DEL))
                {
                    /*删除一个字符*/
                    if (ucInputIndex > 0)
                    {
                        ucInputIndex--;
                        cInputString[ucInputIndex] = 0;
                    }
                }
                else
                {
                    if((cRxedChar >= ' ') && (cRxedChar <= '~'))
                    {
                        if(ucInputIndex < cmdMAX_INPUT_SIZE)
                        {
                            cInputString[ucInputIndex] = cRxedChar;
                            ucInputIndex++;
                        }
                        else
                        {
                            
                        }
                    }
                }
            }
            /*这里相当于一个解锁*/
            xSemaphoreGive(xTxMutex);
        }
    }
}


/****************************************************************************
 * 
 *                         me_rtos_cli.c测试
 * 
 ***************************************************************************/
#include "me_rtos_cli.h"
BaseType_t taskinfo_handle(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    BaseType_t xReturn = pdFALSE;
    /*
    可能会进来很多次, 一次仅仅赋值一次
    */
    /*遍历task任务, 查看运行状态
    
    */
    const char * const pcHeader = "     State   Priority  Stack    #\r\n************************************************\r\n";
    BaseType_t xSpacePadding;
    configASSERT( configMAX_TASK_NAME_LEN > 3 );

    for( xSpacePadding = strlen( "Task" ); xSpacePadding < ( configMAX_TASK_NAME_LEN - 3 ); xSpacePadding++ )
    {
        /* Add a space to align columns after the task's name. */
        *pcWriteBuffer = ' ';
        pcWriteBuffer++;

        /* Ensure always terminated. */
        *pcWriteBuffer = 0x00;
    }

    strcpy( pcWriteBuffer, pcHeader );
    vTaskList( pcWriteBuffer + strlen( pcHeader ) );
    return xReturn;
}
CMD_REGISTER_BASE(taskinfo, taskinfo: displays all tasks information);

BaseType_t reboot_handle(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    BaseType_t xReturn = pdFALSE;
    NVIC_SystemReset();
    return xReturn;
}
CMD_REGISTER_BASE(reboot, reboot: displays all tasks information);


