

#ifndef ME_RTOS_CLI_H
#define ME_RTOS_CLI_H

#ifdef __cplusplus
extern "C" {
#endif


#define CMD_REGISTER(command, help, param_num) \
    const cmd_node _##command##_cmd_node __attribute__((section("cmd_table"))) = {    \
        .cmd = #command,                        \
        .desc = "\r\n"#help,                          \
        .handler = command##_handle,            \
        .expectd_param_num = param_num          \
    };

#define CMD_REGISTER_BASE(command, help) CMD_REGISTER(command, help, 0)
#define CMD_REGISTER_DYNAMIC(command, help) CMD_REGISTER(command, help, -1)


typedef BaseType_t (*cmd_handler)(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
struct CMD_NODE
{
    char *cmd;
    char *desc;
    cmd_handler handler;
    int8_t expectd_param_num; /*-1表示不限个数; 0表示不需要参数*/
};
typedef struct CMD_NODE cmd_node;

const char *cmd_get_parameter(const char* pcCommandString, 
    UBaseType_t uxWantedParameter, BaseType_t * pxParameterStringLength);


#ifdef __cplusplus
}
#endif

#endif



