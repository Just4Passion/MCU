

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "me_rtos_cli.h"


#include "mbedtls/aes.h"


void aes_en_decrypt_test(int mode, const unsigned char *pcContext, size_t xLen, char *pcOutputStr)
{
    BaseType_t xReturn = pdFALSE;
    int ret = 0;
    int offset = 0;
    size_t len = 0;
    unsigned char buf[64];
    unsigned char key[32] = "0123456789";
    unsigned char iv[16];
    unsigned char prv[16];

    unsigned char nonce_counter[16];
    unsigned char stream_block[16];
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    if (mode == MBEDTLS_AES_ENCRYPT)
    {
        ret = mbedtls_aes_setkey_enc(&ctx, key, 256);
    }
    else
    {
        ret = mbedtls_aes_setkey_dec(&ctx, key, 256);
    }

    memset(key, 0, 32);
    /*ECB模式*/
    /*相同的明文块会生成相同的密文块*/
    mbedtls_aes_crypt_ecb(&ctx, mode, buf, buf);    //密码本模式

    /*CBC模式*/
    /*需要一个初始向量启动，串行加密，一个块的错误会影响后续快*/
    /*以16字节块为单位进行加密*/
    mbedtls_aes_crypt_cbc(&ctx, mode, 16, iv, buf, buf);

    /*CFB模式*/
    /*流加密，错误传播有限*/
    /*以字节为单位进行加密*/
    ret = mbedtls_aes_setkey_enc(&ctx, key, 256);
    ret = mbedtls_aes_crypt_cfb128(&ctx, mode, 64, &offset, iv, buf, buf);  //iv是16, 不支持192
    /*OFB模式 - 不区分加密解密*/
    /*错误不可传播，生成一个密钥流，反复加密IV，然后与明文异或*/
    /*以字节为单位进行加密*/
    ret = mbedtls_aes_setkey_enc(&ctx, key, 256);
    mbedtls_aes_crypt_ofb(&ctx, 64, &offset, iv, buf, buf);
    /*CTR模式*/
    /*使用计数器生成密钥流: 高性能应用(如磁盘加密, 网络协议)*/
    /*以字节为单位进行加密*/
    len = 32;
    ret = mbedtls_aes_crypt_ctr(&ctx, len, &offset, nonce_counter,
        stream_block, buf, buf);
}

BaseType_t aes_handle(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    BaseType_t xReturn = pdFALSE;
    int mode = 0;
    size_t xParameterLen = 0;
    const char *pcParameter;
    /*获取第一个参数和参数的长度*/
    pcParameter = cmd_get_parameter(pcCommandString, 1, &xParameterLen);
    if (pcParameter == NULL)
    {
        printf("have no parameter\n");
        return pdFALSE;
    }
    if (pcParameter[0] == '1')
    {
        //加密
        mode = MBEDTLS_AES_ENCRYPT;
    }
    else
    {
        //解密
        mode = MBEDTLS_AES_DECRYPT;
    }
    pcParameter = cmd_get_parameter(pcCommandString, 2, &xParameterLen);
    if (pcParameter == NULL)
    {
        printf("have no parameter\n");
        return pdFALSE;
    }

    aes_en_decrypt_test(pcParameter, xParameterLen, pcWriteBuffer);
    return xReturn;
}
CMD_REGISTER(aes, aes: caculate inputstr aes encrypt[1] or decrypt[0], 2);


