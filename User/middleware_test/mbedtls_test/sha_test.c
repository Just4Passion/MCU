
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "me_rtos_cli.h"


#include "mbedtls/sha1.h"
#include "mbedtls/sha3.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"

typedef enum
{
    MBEDTLS_SHA2_NONE,
    MBEDTLS_SHA2_256,
    MBEDTLS_SHA2_512
}mbedtls_sha2_id;

#ifdef MBEDTLS_SHA1_C
void mbedtls_sha1_to_hexstr_test(const unsigned char *pcContext, size_t xLen, char *pcOutputStr)
{
    int dwIndex = 0;
    unsigned char cDegist[20] = {0};
    mbedtls_sha1(pcContext, xLen, cDegist);

    for (dwIndex = 0; dwIndex < 40; ++dwIndex)
    {
        pcOutputStr[dwIndex] = upper_hex_char((cDegist[dwIndex >> 1] >> ((dwIndex & 0x01) ? 0 : 4)) & 0x0F);
    }
    pcOutputStr[40] = '\0';
}

BaseType_t sha1_handle(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
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
    mbedtls_sha1_to_hexstr_test(pcParameter, xParameterLen, pcWriteBuffer);
    return xReturn;
}
CMD_REGISTER(sha1, sha1: caculate inputstr sha1 value, 1)
#endif

#ifdef MBEDTLS_SHA3_C
void mbedtls_sha3_to_hexstr_test(mbedtls_sha3_id eSha3Id, const unsigned char *pcContext, size_t xLen, char *pcOutputStr)
{
    int dwIndex = 0;
    unsigned char cDegist[64] = {0};
    size_t xOutputLen = 0;
    switch(eSha3Id)
    {
        case MBEDTLS_SHA3_224:
            xOutputLen = 56;
            break;
        case MBEDTLS_SHA3_256:
            xOutputLen = 64;
            break;
        case MBEDTLS_SHA3_384:
            xOutputLen = 96;
            break;
        case MBEDTLS_SHA3_512:
            xOutputLen = 128;
            break;
        default: 
            xOutputLen = 0;
            break;
    }
    mbedtls_sha3(eSha3Id, pcContext, xLen, cDegist, sizeof(cDegist));

    for (dwIndex = 0; dwIndex < xOutputLen; ++dwIndex)
    {
        pcOutputStr[dwIndex] = upper_hex_char((cDegist[dwIndex >> 1] >> ((dwIndex & 0x01) ? 0 : 4)) & 0x0F);
    }
    pcOutputStr[xOutputLen] = '\0';
}

BaseType_t sha3_handle(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    BaseType_t xReturn = pdFALSE;
    size_t xParameterLen = 0;
    const char *pcParameter;
    mbedtls_sha3_id eSha3Id = MBEDTLS_SHA3_NONE;
    /*获取第一个参数: 224, 256, 384, 512*/
    pcParameter = cmd_get_parameter(pcCommandString, 1, &xParameterLen);
    if (pcParameter == NULL)
    {
        printf("have no parameter\n");
        return pdFALSE;
    }
    if (strncmp(pcParameter, "224", xParameterLen) == 0)
    {
        eSha3Id = MBEDTLS_SHA3_224;
    }
    else if (strncmp(pcParameter, "256", xParameterLen) == 0)
    {
        eSha3Id = MBEDTLS_SHA3_256;
    }
    else if (strncmp(pcParameter, "384", xParameterLen) == 0)
    {
        eSha3Id = MBEDTLS_SHA3_384;
    }
    else if (strncmp(pcParameter, "512", xParameterLen) == 0)
    {
        eSha3Id = MBEDTLS_SHA3_512;
    }
    else
    {
        printf("sha3 id is not support\n");
        return pdFALSE;
    }
    /*获取第2个参数和参数的长度*/
    pcParameter = cmd_get_parameter(pcCommandString, 2, &xParameterLen);
    if (pcParameter == NULL)
    {
        printf("have no parameter\n");
        return pdFALSE;
    }
    printf("eSha3Id = %d, xParameterLen = %d, pcParameter = %s\n", eSha3Id, xParameterLen, pcParameter);
    mbedtls_sha3_to_hexstr_test(eSha3Id, pcParameter, xParameterLen, pcWriteBuffer);
    return xReturn;
}
CMD_REGISTER(sha3, sha3: caculate inputstr sha3 value, 2)
#endif


void mbedtls_sha2_to_hexstr_test(mbedtls_sha2_id eSha2Id, const unsigned char *pcContext, size_t xLen, char *pcOutputStr)
{
    int dwIndex = 0;
    unsigned char cDegist[64] = {0};
    size_t xOutputLen = 0;
    switch(eSha2Id)
    {
        case MBEDTLS_SHA2_256:
            xOutputLen = 64;
            mbedtls_sha256(pcContext, xLen, cDegist, 0);
            break;
        case MBEDTLS_SHA2_512:
            #ifdef MBEDTLS_SHA512_C
            mbedtls_sha512(pcContext, xLen, cDegist, 0);
            #endif
            xOutputLen = 128;
            break;
        default: 
            xOutputLen = 0;
            break;
    }

    for (dwIndex = 0; dwIndex < xOutputLen; ++dwIndex)
    {
        pcOutputStr[dwIndex] = upper_hex_char((cDegist[dwIndex >> 1] >> ((dwIndex & 0x01) ? 0 : 4)) & 0x0F);
    }
    pcOutputStr[xOutputLen] = '\0';
}

BaseType_t sha2_handle(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    BaseType_t xReturn = pdFALSE;
    size_t xParameterLen = 0;
    const char *pcParameter;
    mbedtls_sha2_id eSha2Id = MBEDTLS_SHA2_NONE;
    /*获取第一个参数: 224, 256, 384, 512*/
    pcParameter = cmd_get_parameter(pcCommandString, 1, &xParameterLen);
    if (pcParameter == NULL)
    {
        printf("have no parameter\n");
        return pdFALSE;
    }
    if (strncmp(pcParameter, "256", xParameterLen) == 0)
    {
        eSha2Id = MBEDTLS_SHA2_256;
    }
    else if (strncmp(pcParameter, "512", xParameterLen) == 0)
    {
        eSha2Id = MBEDTLS_SHA2_512;
    }
    else
    {
        printf("sha2 id is not support\n");
        return pdFALSE;
    }
    /*获取第2个参数和参数的长度*/
    pcParameter = cmd_get_parameter(pcCommandString, 2, &xParameterLen);
    if (pcParameter == NULL)
    {
        printf("have no parameter\n");
        return pdFALSE;
    }
    printf("eSha2Id = %d, xParameterLen = %d, pcParameter = %s\n", eSha2Id, xParameterLen, pcParameter);
    mbedtls_sha2_to_hexstr_test(eSha2Id, pcParameter, xParameterLen, pcWriteBuffer);
    return xReturn;
}
CMD_REGISTER(sha2, sha2: caculate inputstr sha2 value, 2)