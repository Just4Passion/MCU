
#ifndef RTOS_CLI_H
#define RTOS_CLI_H

#ifdef __cplusplus
extern "C" {
#endif


/*处理命令行命令*/
typedef BaseType_t (*pdCOMMAND_LINE_CALLBACK)(char *pcWriteBuffer, 
                                              size_t xWriteBufferLen, 
                                              const char * pcCommandString);

/*这是一个命令行命令的定义*/
typedef struct xCOMMAND_LINE_INPUT
{
    const char * const pcCommand;           /*命令字符串*/
    const char * const pcHelpString;        /*帮助字符串*/
    const pdCOMMAND_LINE_CALLBACK pxCommandInterpreter; /*命令解释函数*/
    int8_t cExpectedNumberOfParameters;     /*期望的命令个数*/
}CLI_Command_Definition_t;

/*命令行列表, 这是一个链表的节点*/
typedef struct xCOMMAND_INPUT_LIST
{
    const CLI_Command_Definition_t *pxCommandLineDefinition;
    struct xCOMMAND_INPUT_LIST *pxNext;
}CLI_Definition_List_Item_t;

#define xCommandLineInput    CLI_Command_Definition_t


/*往命令链表中注册命令*/
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
    BaseType_t FreeRTOS_CLIRegisterCommand(const CLI_Command_Definition_t * const pxCommandToRegister);
#endif

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    BaseType_t FreeRTOS_CLIRegisterCommandStatic(const CLI_Command_Definition_t * const pxCommandToRegister,
                                                CLI_Definition_List_Item_t * pxCliDefinitionListItemBuffer);
#endif

/*处理命令: 当命令进来时, 从命令列表中找匹配的命令*/
BaseType_t FreeRTOS_CLIProcessCommand( const char * const pcCommandInput,
                                        char * pcWriteBuffer,
                                        size_t xWriteBufferLen );

/*返回输出buffer*/
char * FreeRTOS_CLIGetOutputBuffer( void );

/*返回命令中参数字符串的指针*/
const char * FreeRTOS_CLIGetParameter(const char * pcCommandString,
                                      UBaseType_t uxWantedParameter,
                                      BaseType_t * pxParameterStringLength);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif

