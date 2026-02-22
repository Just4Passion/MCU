
#include <string.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "portmacro.h"


#include "rtos_cli.h"

/*
FreeRTOS Plus CLI的设计

Command Process Fuction -> Map Command Function to CLI Definition 
-> Register Maped(Function, CLI) -> Run Command Interpreter

*/

/*
从CLI理解CLI的设计
    首先需要命令处理函数 - 设计者要明确命令处理函数的原型
    然后需要命令表 - 设计者需要把命令处理函数和命令关联起来
    然后注册命令表
    最后接收命令, 解释命令

命令处理节点: 命令, 帮助信息, 处理函数, 输出数据
设计一个数据结构用来管理命令, 映射表(数组-可以使用section的方式注册到某个代码区域)
注册接口
查找接口
*/

#ifdef RTOS_CLI

/*****************************************************************
 * 
 *              FreeRTOS Plus CLI的整体设计源码
 * 
 * 1、实现命令
 *      为了降低RAM的使用率, 当实现命令行为的函数返回pdTRUE时, 继续调用该函数(递归); 返回pdFALSE时, 表示命令处理完毕
 * 
 *****************************************************************/

#define configCOMMAND_INT_MAX_OUTPUT_SIZE 512

#ifndef configAPPLICATION_PROVIDES_cOutputBuffer //如果应用提供输出缓冲区, 则CLI不提供
    #define configAPPLICATION_PROVIDES_cOutputBuffer    0
#endif

static void prvRegisterCommand(const CLI_Command_Definition_t * const pxCommandToRegister,
                               CLI_Definition_List_Item_t * pxCliDefinitionListItemBuffer);

/*帮助命令处理函数： help*/
static BaseType_t prvHelpCommand(char * pcWriteBuffer,
                                size_t xWriteBufferLen,
                                const char * pcCommandString );
/*跟随在命令后的命令参数个数*/
static int8_t prvGetNumberOfParameters(const char * pcCommandString );

/*help命令节点*/
static const CLI_Command_Definition_t xHelpCommand =
{
    "help",
    "\r\nhelp:\r\n Lists all the registered commands\r\n\r\n",
    prvHelpCommand,
    0
};

/*命令链表*/
static CLI_Definition_List_Item_t xRegisteredCommands =
{
    &xHelpCommand,
    NULL
};


#if ( configAPPLICATION_PROVIDES_cOutputBuffer == 0 )
    static char cOutputBuffer[ configCOMMAND_INT_MAX_OUTPUT_SIZE ];
#else
    extern char cOutputBuffer[ configCOMMAND_INT_MAX_OUTPUT_SIZE ];
#endif


#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

    BaseType_t FreeRTOS_CLIRegisterCommand(const CLI_Command_Definition_t * const pxCommandToRegister)
    {
        BaseType_t xReturn = pdFAIL;
        CLI_Definition_List_Item_t * pxNewListItem;

        
        configASSERT(pxCommandToRegister != NULL);

        /*申请内存*/
        pxNewListItem = (CLI_Definition_List_Item_t *) pvPortMalloc(sizeof(CLI_Definition_List_Item_t));
        configASSERT( pxNewListItem != NULL );
        if( pxNewListItem != NULL )
        {
            prvRegisterCommand(pxCommandToRegister, pxNewListItem );
            xReturn = pdPASS;
        }

        return xReturn;
    }

#endif /* #if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) */

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

    BaseType_t FreeRTOS_CLIRegisterCommandStatic( const CLI_Command_Definition_t * const pxCommandToRegister,
                                                  CLI_Definition_List_Item_t * pxCliDefinitionListItemBuffer )
    {
        configASSERT( pxCommandToRegister != NULL );
        configASSERT( pxCliDefinitionListItemBuffer != NULL );

        prvRegisterCommand(pxCommandToRegister, pxCliDefinitionListItemBuffer);

        return pdPASS;
    }

#endif /* #if ( configSUPPORT_STATIC_ALLOCATION == 1 ) */


static void prvRegisterCommand(const CLI_Command_Definition_t * const pxCommandToRegister,
    CLI_Definition_List_Item_t * pxCliDefinitionListItemBuffer )
{
    /*静态变量: 始终指向末尾节点*/
    static CLI_Definition_List_Item_t * pxLastCommandInList = &xRegisteredCommands;

    configASSERT( pxCommandToRegister != NULL );
    configASSERT( pxCliDefinitionListItemBuffer != NULL );

    taskENTER_CRITICAL();
    {
        /*参数1是具体的命令参数节点, 参数2是即将被赋值并插入命令链表的节点*/
        pxCliDefinitionListItemBuffer->pxCommandLineDefinition = pxCommandToRegister;
        pxCliDefinitionListItemBuffer->pxNext = NULL;

        /*将新节点接末尾*/
        pxLastCommandInList->pxNext = pxCliDefinitionListItemBuffer;
        /*指向末尾节点*/
        pxLastCommandInList = pxCliDefinitionListItemBuffer;
    }
    taskEXIT_CRITICAL();
}

static BaseType_t prvHelpCommand( char * pcWriteBuffer,
    size_t xWriteBufferLen,
    const char * pcCommandString )
{
    static const CLI_Definition_List_Item_t * pxCommand = NULL;
    BaseType_t xReturn;

    ( void ) pcCommandString;

    if( pxCommand == NULL )
    {
        /*每次到末尾, pxCommand都会重新进入开头
        所以帮助信息就是遍历整个链表, 然后输出命令的help信息
        */
        pxCommand = &xRegisteredCommands;
    }

    /*把指令的帮助信息复制到缓冲区*/
    strncpy( pcWriteBuffer, pxCommand->pxCommandLineDefinition->pcHelpString, xWriteBufferLen );
    pxCommand = pxCommand->pxNext;

    if( pxCommand == NULL )
    {
        xReturn = pdFALSE;
    }
    else
    {
        xReturn = pdTRUE;
    }
    return xReturn;
}

static int8_t prvGetNumberOfParameters( const char * pcCommandString )
{
    int8_t cParameters = 0;
    BaseType_t xLastCharacterWasSpace = pdFALSE;

    while( *pcCommandString != 0x00 )
    {
        if( ( *pcCommandString ) == ' ' )
        {
            /*如果上一次字符不是空格, 记为一个有效参数*/
            if( xLastCharacterWasSpace != pdTRUE )
            {
                cParameters++;
                xLastCharacterWasSpace = pdTRUE;
            }
        }
        else
        {
            xLastCharacterWasSpace = pdFALSE;
        }

        pcCommandString++;
    }

    /*如果命令以空格结尾, 则不计入参数个数*/
    if( xLastCharacterWasSpace == pdTRUE )
    {
        cParameters--;
    }

    return cParameters;
}



/*********************************外部接口*********************************/
char * FreeRTOS_CLIGetOutputBuffer( void )
{
    return cOutputBuffer;
}

/*
*/
const char * FreeRTOS_CLIGetParameter(const char * pcCommandString,
                                      UBaseType_t uxWantedParameter,
                                      BaseType_t * pxParameterStringLength)
{
    UBaseType_t uxParametersFound = 0;
    const char * pcReturn = NULL;

    *pxParameterStringLength = 0;

    while( uxParametersFound < uxWantedParameter )
    {
        /*找到第一个有效字符串的末尾*/
        while( ( ( *pcCommandString ) != 0x00 ) && ( ( *pcCommandString ) != ' ' ) )
        {
            pcCommandString++;
        }

        /*略过字符串后面的空格*/
        while( ( ( *pcCommandString ) != 0x00 ) && ( ( *pcCommandString ) == ' ' ) )
        {
            pcCommandString++;
        }

        /*指向了一个参数*/
        if( *pcCommandString != 0x00 )
        {
            /*找到一个参数*/
            uxParametersFound++;

            if( uxParametersFound == uxWantedParameter )
            {
                /*参数有多长*/
                pcReturn = pcCommandString;

                while(((*pcCommandString) != 0x00) && ((*pcCommandString ) != ' '))
                {
                    ( *pxParameterStringLength )++;
                    pcCommandString++;
                }

                if( *pxParameterStringLength == 0 )
                {
                    pcReturn = NULL;
                }

                break;
            }
        }
        else
        {
            break;
        }
    }

    return pcReturn;
}

BaseType_t FreeRTOS_CLIProcessCommand(const char * const pcCommandInput,
                                     char * pcWriteBuffer,
                                     size_t xWriteBufferLen)
{
    static const CLI_Definition_List_Item_t * pxCommand = NULL;
    BaseType_t xReturn = pdTRUE;
    const char * pcRegisteredCommandString;
    size_t xCommandStringLength;

    /**/
    if( pxCommand == NULL )
    {
        /*找到匹配的命令*/
        for( pxCommand = &xRegisteredCommands; pxCommand != NULL; pxCommand = pxCommand->pxNext )
        {
            pcRegisteredCommandString = pxCommand->pxCommandLineDefinition->pcCommand;
            xCommandStringLength = strlen(pcRegisteredCommandString);

            /*将命令与输入进行比较, 如果有前缀匹配怎么办, 考虑使用空格来区分*/
            if(strncmp( pcCommandInput, pcRegisteredCommandString, xCommandStringLength) == 0)
            {
                if(( pcCommandInput[ xCommandStringLength ] == ' ')
                    || ( pcCommandInput[ xCommandStringLength ] == 0x00))
                {
                    /*不存在前缀匹配问题, 直接使用空格区分开*/
                    if( pxCommand->pxCommandLineDefinition->cExpectedNumberOfParameters >= 0 )
                    {
                        if( prvGetNumberOfParameters(pcCommandInput) != pxCommand->pxCommandLineDefinition->cExpectedNumberOfParameters)
                        {
                            xReturn = pdFALSE;
                        }
                    }
                    break;
                }
            }
        }
    }

    if( ( pxCommand != NULL ) && (xReturn == pdFALSE))
    {
        strncpy( pcWriteBuffer, "Incorrect command parameter(s).  Enter \"help\" to view a list of available commands.\r\n\r\n", xWriteBufferLen );
        pxCommand = NULL;
    }
    else if( pxCommand != NULL )
    {
        xReturn = pxCommand->pxCommandLineDefinition->pxCommandInterpreter(pcWriteBuffer, xWriteBufferLen, pcCommandInput);

        /*关注返回pdTRUE和pdFALSE有什么区别*/
        if( xReturn == pdFALSE )
        {
            pxCommand = NULL;
        }
    }
    else
    {
        strncpy( pcWriteBuffer, "Command not recognised.  Enter 'help' to view a list of available commands.\r\n\r\n", xWriteBufferLen );
        xReturn = pdFALSE;
    }

    return xReturn;
}

/*****************************************************************
 * 
 *                      MyCLI 设计源码
 * 
 *****************************************************************/

BaseType_t xCommandFunctionName(char *pcWriteBuffer, 
    size_t xWriteBufferLen, 
    const char *pcCommandString,
    const char *pcCommandParameters);


#endif




