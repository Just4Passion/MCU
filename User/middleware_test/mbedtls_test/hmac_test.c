
#include <stdio.h>

#include "FreeRTOS.h"
#include "me_rtos_cli.h"

#include "mbedtls/md.h"
/*
先测试md.c的接口
*/


void mbedtls_hamc_to_hexstr_test(mbedtls_md_type_t eMdType, 
    const unsigned char *pcSecret, size_t xSecretLen,
    const unsigned char *pcContext, size_t xLen, char *pcOutputStr)
{
    int dwIndex = 0;
    unsigned char cDegist[64] = {0};
    size_t xOutputLen = 0;
    const mbedtls_md_info_t *pxMdInfo = NULL;
    switch(eMdType)
    {
        case MBEDTLS_MD_NONE:
            xOutputLen = 0;
            break;
        case MBEDTLS_MD_MD5:
            xOutputLen = 32;
            break;
        case MBEDTLS_MD_RIPEMD160:
            xOutputLen = 40;
            break;
        case MBEDTLS_MD_SHA1:
            xOutputLen = 40;
            break;
        case MBEDTLS_MD_SHA224:
        case MBEDTLS_MD_SHA3_224:
            xOutputLen = 56;
            break;
        case MBEDTLS_MD_SHA256:
        case MBEDTLS_MD_SHA3_256:
            xOutputLen = 64;
            break;
        case MBEDTLS_MD_SHA384:
        case MBEDTLS_MD_SHA3_384:
            xOutputLen = 96;
            break;
        case MBEDTLS_MD_SHA512:
        case MBEDTLS_MD_SHA3_512:
            xOutputLen = 128;
            break;
        default: 
            xOutputLen = 0;
            break;
    }
    /*获取算法引擎*/
    pxMdInfo = mbedtls_md_info_from_type(eMdType);
    mbedtls_md_hmac(pxMdInfo, pcSecret, xSecretLen, pcContext, xLen, pcOutputStr);

    for (dwIndex = 0; dwIndex < xOutputLen; ++dwIndex)
    {
        pcOutputStr[dwIndex] = upper_hex_char((cDegist[dwIndex >> 1] >> ((dwIndex & 0x01) ? 0 : 4)) & 0x0F);
    }
    pcOutputStr[xOutputLen] = '\0';
}

BaseType_t hmac_handle(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    BaseType_t xReturn = pdFALSE;
    size_t xParameterLen = 0, xSecretLen = 0, xContentLen = 0;
    const char *pcParameter, *pcSecret, *pcContent;
    mbedtls_md_type_t eMdType = MBEDTLS_MD_NONE;
    /*获取第一个参数: 224, 256, 384, 512*/
    pcParameter = cmd_get_parameter(pcCommandString, 1, &xParameterLen);
    if (pcParameter == NULL)
    {
        printf("have no parameter\n");
        return pdFALSE;
    }
    if (strncmp(pcParameter, "md5", xParameterLen) == 0)
    {
        eMdType = MBEDTLS_MD_MD5;
    }
    else if (strncmp(pcParameter, "ripemd160", xParameterLen) == 0)
    {
        eMdType = MBEDTLS_MD_RIPEMD160;
    }
    else if (strncmp(pcParameter, "sha1", xParameterLen) == 0)
    {
        eMdType = MBEDTLS_MD_SHA1;
    }
    else if (strncmp(pcParameter, "sha-256", xParameterLen) == 0)
    {
        eMdType = MBEDTLS_MD_SHA256;
    }
    else if (strncmp(pcParameter, "sha-512", xParameterLen) == 0)
    {
        eMdType = MBEDTLS_MD_SHA512;
    }
    else if (strncmp(pcParameter, "sha3-224", xParameterLen) == 0)
    {
        eMdType = MBEDTLS_MD_SHA3_224;
    }
    else if (strncmp(pcParameter, "sha3-256", xParameterLen) == 0)
    {
        eMdType = MBEDTLS_MD_SHA3_256;
    }
    else if (strncmp(pcParameter, "sha3-384", xParameterLen) == 0)
    {
        eMdType = MBEDTLS_MD_SHA3_384;
    }
    else if (strncmp(pcParameter, "sha3-512", xParameterLen) == 0)
    {
        eMdType = MBEDTLS_MD_SHA3_512;
    }
    else
    {
        printf("md id is not support\n");
        return pdFALSE;
    }
    /*获取第2个参数和参数的长度*/
    pcSecret = cmd_get_parameter(pcCommandString, 2, &xSecretLen);
    if (pcSecret == NULL)
    {
        printf("have no 2th parameter\n");
        return pdFALSE;
    }
    /*获取第3个参数和参数的长度*/
    pcContent = cmd_get_parameter(pcCommandString, 3, &xContentLen);
    if (pcContent == NULL)
    {
        printf("have no 3th parameter\n");
        return pdFALSE;
    }
    printf("eMdType = %d, pcParameter = %s, pcSecret = %s, pcContent = %s\n", 
        eMdType, pcParameter, pcSecret, pcContent);
    mbedtls_hamc_to_hexstr_test(eMdType, pcSecret, xSecretLen,
        pcContent, xContentLen, pcWriteBuffer);
    return xReturn;
}
CMD_REGISTER(hmac, hmac: caculate inputstr hmac value. Example - hmac md_algorithm seceret string, 3);

