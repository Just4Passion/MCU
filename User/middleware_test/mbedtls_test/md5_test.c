
#include <stdio.h>

#include "FreeRTOS.h"
#include "me_rtos_cli.h"
#include "mbedtls/md5.h"

void mbedtls_md5_to_hexstr_test(const unsigned char *pcContext, size_t xLen, char *pcOutputStr)
{
    int dwIndex = 0;
    unsigned char cDegist[16] = {0};
    mbedtls_md5(pcContext, xLen, cDegist);

    for (dwIndex = 0; dwIndex < 32; ++dwIndex)
    {
        pcOutputStr[dwIndex] = upper_hex_char((cDegist[dwIndex >> 1] >> ((dwIndex & 0x01) ? 0 : 4)) & 0x0F);
    }
    pcOutputStr[32] = '\0';
}

BaseType_t md5_handle(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    BaseType_t xReturn = pdFALSE;
    size_t xParameterLen = 0;
    const char *pcParameter;
    /*获取第一个参数和参数的长度*/
    pcParameter = cmd_get_parameter(pcCommandString, 1, &xParameterLen);
    if (pcParameter == NULL)
    {
        printf("have no parameter\n");
        return pdFALSE;
    }
    mbedtls_md5_to_hexstr_test(pcParameter, xParameterLen, pcWriteBuffer);
    return xReturn;
}
CMD_REGISTER(md5, md5: caculate inputstr md5 value, 1);




