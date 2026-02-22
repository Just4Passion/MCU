

#include <string.h>

#include "FreeRTOS.h"

#include "me_rtos_cli.h"

/************************************************************************
 * 
 *      首先编写一个命令节点, 命令节点是什么样的
 * 命令名称
 * 命令帮助信息
 * 命令处理函数
 * 命令参数个数
 * 命令输出结果
 * 
 * note:
 *      在这个重构命令行中, 我只修改命令行的注册接口, 其他逻辑不做修改
 * 
 ***********************************************************************/


/*针对ARMCC编译器, 使用sectionName$$Base定义起始地址, 使用sectionName$$Limit定义结束地址*/
extern const int cmd_table$$Base;
extern const int cmd_table$$Limit;
#define __cmd_table_start ((cmd_node *) &cmd_table$$Base)
#define __cmd_table_end ((cmd_node *) &cmd_table$$Limit)

static char cCmdOutBuffer[1024] = {0};
BaseType_t help_handle(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    static const cmd_node *pxCmdNode;
    BaseType_t xReturn = pdFALSE;
    /*
    可能会进来很多次, 一次仅仅赋值一次
    */
    if (pxCmdNode == NULL)
    {
        pxCmdNode = __cmd_table_start;
    }
    strncpy(pcWriteBuffer, pxCmdNode->desc, xWriteBufferLen);
    pxCmdNode++;

    if (pxCmdNode == __cmd_table_end)
    {
        pxCmdNode = NULL;
        xReturn = pdFALSE;
    }
    else 
    {
        xReturn = pdTRUE;
    }

    return xReturn;
}
CMD_REGISTER(help, help: displays the supported commands, 0);

static int8_t get_number_of_parameters(const char * pcCommandString)
{
    int8_t cParameters = 0;
    BaseType_t xLastCharacterWasSpace = pdFALSE;

    while(*pcCommandString != 0x00)
    {
        if((*pcCommandString ) == ' ')
        {
            /*如果上一次字符不是空格, 记为一个有效参数*/
            if(xLastCharacterWasSpace != pdTRUE)
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
    if(xLastCharacterWasSpace == pdTRUE)
    {
        cParameters--;
    }

    return cParameters;
}

BaseType_t cmd_process(const char * const pcCommandString, char *pcWriteBuffer, size_t xWriteBufferLen)
{
    BaseType_t xReturn = pdTRUE;

    /*存在多线程问题, 但是其本身就是root独占, 所以目前不考虑*/
    static const cmd_node *pxCmdNode = NULL;
    const char * pcRegisteredCommandString;
    size_t xCommandStringLength;

    if (pxCmdNode == NULL)
    {
        pxCmdNode = __cmd_table_start;
        for (; pxCmdNode != __cmd_table_end; pxCmdNode++)
        {
            pcRegisteredCommandString = pxCmdNode->cmd;
            xCommandStringLength = strlen(pcRegisteredCommandString);
            if (strncmp(pxCmdNode->cmd, pcCommandString, xCommandStringLength) == 0)
            {
                /*判断下一个字符是不是' '或者'\0'*/
                /*命令匹配上了*/
                if ((pcCommandString[xCommandStringLength] == ' ') || (pcCommandString[xCommandStringLength] == '\0'))
                {
                    if (pxCmdNode->expectd_param_num >= 0)
                    {
                        if (get_number_of_parameters(pcCommandString) != pxCmdNode->expectd_param_num)
                        {
                            xReturn = pdFALSE;
                        }
                    }
                }
                break;
            }
        }
    }

    if((pxCmdNode != __cmd_table_end) && (xReturn == pdFALSE))
    {
        strncpy( pcWriteBuffer, "Incorrect command parameter(s).  Enter \"help\" to view a list of available commands.\r\n\r\n", xWriteBufferLen );
        pxCmdNode = NULL;
    }
    else if (pxCmdNode != __cmd_table_end)
    {
        xReturn = pxCmdNode->handler(pcWriteBuffer, xWriteBufferLen, pcCommandString);
        if(xReturn == pdFALSE)
        {
            pxCmdNode = NULL;
        }
    }
    else
    {
        strncpy( pcWriteBuffer, "Command not found\r\n", xWriteBufferLen);
        pxCmdNode = NULL;
        xReturn = pdFALSE;
    }
    return xReturn;
}

char *cmd_get_out_buffer()
{
    return cCmdOutBuffer;
}

const char *cmd_get_parameter(const char* pcCommandString, UBaseType_t uxWantedParameter, BaseType_t * pxParameterStringLength)
{
    /*
    想要第几个参数, 找到了赋值出来
    和get_number_of_parameters有异曲同工之妙
    */
    UBaseType_t uxParametersFound = 0;
    const char * pcReturn = NULL;

    *pxParameterStringLength = 0;
    while( uxParametersFound < uxWantedParameter)
    {
        /*找到第一个有效字符串的末尾*/
        while(((*pcCommandString) != 0x00) && ((*pcCommandString) != ' '))
        {
            pcCommandString++;
        }

        /*略过字符串后面的空格*/
        while(((*pcCommandString) != 0x00) && ((*pcCommandString) == ' '))
        {
            pcCommandString++;
        }

        /*指向了一个参数*/
        if(*pcCommandString != 0x00)
        {
            /*找到一个参数*/
            uxParametersFound++;

            /*是不是想要的第N个参数*/
            if(uxParametersFound == uxWantedParameter)
            {
                /*获取到了参数的起始位置*/
                pcReturn = pcCommandString;
                /*计算参数的长度*/
                while(((*pcCommandString) != 0x00) && ((*pcCommandString ) != ' '))
                {
                    (*pxParameterStringLength)++;
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



