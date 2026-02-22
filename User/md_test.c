/**
 * \file md.c
 *
 * \brief Generic message digest wrapper for Mbed TLS
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

 /*
  * Availability of functions in this module is controlled by two
  * feature macros:
  * - MBEDTLS_MD_C enables the whole module;
  * - MBEDTLS_MD_LIGHT enables only functions for hashing and accessing
  * most hash metadata (everything except string names); is it
  * automatically set whenever MBEDTLS_MD_C is defined.
  *
  * In this file, functions from MD_LIGHT are at the top, MD_C at the end.
  *
  * In the future we may want to change the contract of some functions
  * (behaviour with NULL arguments) depending on whether MD_C is defined or
  * only MD_LIGHT. Also, the exact scope of MD_LIGHT might vary.
  *
  * For these reasons, we're keeping MD_LIGHT internal for now.
  */
 #if defined(MBEDTLS_MD_LIGHT)
 
 #include "mbedtls/md.h"
 #include "md_wrap.h"
 #include "mbedtls/platform_util.h"
 #include "mbedtls/error.h"
 
 #include "mbedtls/md5.h"
 #include "mbedtls/ripemd160.h"
 #include "mbedtls/sha1.h"
 #include "mbedtls/sha256.h"
 #include "mbedtls/sha512.h"
 #include "mbedtls/sha3.h"
 
 #if defined(MBEDTLS_PSA_CRYPTO_C)
 #include <psa/crypto.h>
 #include "md_psa.h"
 #include "psa_util_internal.h"
 #endif
 
 #if defined(MBEDTLS_MD_SOME_PSA)
 #include "psa_crypto_core.h"
 #endif
 
 #include "mbedtls/platform.h"
 
 #include <string.h>
 
 #if defined(MBEDTLS_FS_IO)
 #include <stdio.h>
 #endif
 
 /* See comment above MBEDTLS_MD_MAX_SIZE in md.h */
 #if defined(MBEDTLS_PSA_CRYPTO_C) && MBEDTLS_MD_MAX_SIZE < PSA_HASH_MAX_SIZE
 #error "Internal error: MBEDTLS_MD_MAX_SIZE < PSA_HASH_MAX_SIZE"
 #endif

 inline void mbedtls_xor(unsigned char *r, const unsigned char *a, const unsigned char *b, size_t n)
{
    size_t i = 0;
#if defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS)
#if defined(__ARM_NEON)
    for (; (i + 16) <= n; i += 16) {
        uint8x16_t v1 = vld1q_u8(a + i);
        uint8x16_t v2 = vld1q_u8(b + i);
        uint8x16_t x = veorq_u8(v1, v2);
        vst1q_u8(r + i, x);
    }
#elif defined(__amd64__) || defined(__x86_64__) || defined(__aarch64__)
    /* This codepath probably only makes sense on architectures with 64-bit registers */
    for (; (i + 8) <= n; i += 8) {
        uint64_t x = mbedtls_get_unaligned_uint64(a + i) ^ mbedtls_get_unaligned_uint64(b + i);
        mbedtls_put_unaligned_uint64(r + i, x);
    }
#else
    for (; (i + 4) <= n; i += 4) {
        uint32_t x = mbedtls_get_unaligned_uint32(a + i) ^ mbedtls_get_unaligned_uint32(b + i);
        mbedtls_put_unaligned_uint32(r + i, x);
    }
#endif
#endif
    for (; i < n; i++) {
        r[i] = a[i] ^ b[i];
    }
}
 
 #if defined(MBEDTLS_MD_C)
 #define MD_INFO(type, out_size, block_size) type, out_size, block_size,
 #else
 #define MD_INFO(type, out_size, block_size) type, out_size,
 #endif
 
 #if defined(MBEDTLS_MD_CAN_MD5)
 static const mbedtls_md_info_t mbedtls_md5_info = {
     MD_INFO(MBEDTLS_MD_MD5, 16, 64)
 };
 #endif

 
 #if defined(MBEDTLS_MD_CAN_SHA256)
 static const mbedtls_md_info_t mbedtls_sha256_info = {
     MD_INFO(MBEDTLS_MD_SHA256, 32, 64)
 };
 #endif

 const mbedtls_md_info_t *mbedtls_md_info_from_type(mbedtls_md_type_t md_type)
 {
     switch (md_type) {
 #if defined(MBEDTLS_MD_CAN_MD5)
         case MBEDTLS_MD_MD5:
             return &mbedtls_md5_info;
 #endif
 
 #if defined(MBEDTLS_MD_CAN_SHA256)
         case MBEDTLS_MD_SHA256:
             return &mbedtls_sha256_info;
 #endif
         default:
             return NULL;
     }
 }
 void mbedtls_md_init(mbedtls_md_context_t *ctx)
 {
     /* Note: this sets engine (if present) to MBEDTLS_MD_ENGINE_LEGACY */
     memset(ctx, 0, sizeof(mbedtls_md_context_t));
 }
 
 void mbedtls_md_free(mbedtls_md_context_t *ctx)
 {
     if (ctx == NULL || ctx->md_info == NULL) {
         return;
     }
 
     if (ctx->md_ctx != NULL) {

         switch (ctx->md_info->type) {
 #if defined(MBEDTLS_MD5_C)
             case MBEDTLS_MD_MD5:
                 mbedtls_md5_free(ctx->md_ctx);
                 break;
 #endif
 #if defined(MBEDTLS_SHA256_C)
             case MBEDTLS_MD_SHA256:
                 mbedtls_sha256_free(ctx->md_ctx);
                 break;
 #endif
             default:
                 /* Shouldn't happen */
                 break;
         }
         mbedtls_free(ctx->md_ctx);
     }
 
 #if defined(MBEDTLS_MD_C)
     if (ctx->hmac_ctx != NULL) {
         mbedtls_zeroize_and_free(ctx->hmac_ctx,
                                  2 * ctx->md_info->block_size);
     }
 #endif
 
     mbedtls_platform_zeroize(ctx, sizeof(mbedtls_md_context_t));
 }
 
 int mbedtls_md_clone(mbedtls_md_context_t *dst,
                      const mbedtls_md_context_t *src)
 {
     if (dst == NULL || dst->md_info == NULL ||
         src == NULL || src->md_info == NULL ||
         dst->md_info != src->md_info) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 
     switch (src->md_info->type) {
 #if defined(MBEDTLS_MD5_C)
         case MBEDTLS_MD_MD5:
             mbedtls_md5_clone(dst->md_ctx, src->md_ctx);
             break;
 #endif

 #if defined(MBEDTLS_SHA256_C)
         case MBEDTLS_MD_SHA256:
             mbedtls_sha256_clone(dst->md_ctx, src->md_ctx);
             break;
 #endif
         default:
             return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 
     return 0;
 }
 
 #define ALLOC(type)                                                   \
     do {                                                                \
         ctx->md_ctx = mbedtls_calloc(1, sizeof(mbedtls_##type##_context)); \
         if (ctx->md_ctx == NULL)                                       \
         return MBEDTLS_ERR_MD_ALLOC_FAILED;                      \
         mbedtls_##type##_init(ctx->md_ctx);                           \
     }                                                                   \
     while (0)
 
 int mbedtls_md_setup(mbedtls_md_context_t *ctx, const mbedtls_md_info_t *md_info, int hmac)
 {
 #if defined(MBEDTLS_MD_C)
     if (ctx == NULL) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 #endif
     if (md_info == NULL) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 
     ctx->md_info = md_info;
     ctx->md_ctx = NULL;
 #if defined(MBEDTLS_MD_C)
     ctx->hmac_ctx = NULL;
 #else
     if (hmac != 0) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 #endif

     switch (md_info->type) {
 #if defined(MBEDTLS_MD5_C)
         case MBEDTLS_MD_MD5:
             ALLOC(md5);
             break;
 #endif
 #if defined(MBEDTLS_SHA256_C)
         case MBEDTLS_MD_SHA256:
             ALLOC(sha256);
             break;
 #endif
         default:
             return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 
 #if defined(MBEDTLS_MD_C)
     if (hmac != 0) {
         ctx->hmac_ctx = mbedtls_calloc(2, md_info->block_size);
         if (ctx->hmac_ctx == NULL) {
             mbedtls_md_free(ctx);
             return MBEDTLS_ERR_MD_ALLOC_FAILED;
         }
     }
 #endif
 
     return 0;
 }
 #undef ALLOC
 
 int mbedtls_md_starts(mbedtls_md_context_t *ctx)
 {
 #if defined(MBEDTLS_MD_C)
     if (ctx == NULL || ctx->md_info == NULL) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 #endif
 
 
     switch (ctx->md_info->type) {
 #if defined(MBEDTLS_MD5_C)
         case MBEDTLS_MD_MD5:
             return mbedtls_md5_starts(ctx->md_ctx);
 #endif
 #if defined(MBEDTLS_RIPEMD160_C)
         case MBEDTLS_MD_RIPEMD160:
             return mbedtls_ripemd160_starts(ctx->md_ctx);
 #endif
 #if defined(MBEDTLS_SHA256_C)
         case MBEDTLS_MD_SHA256:
             return mbedtls_sha256_starts(ctx->md_ctx, 0);
 #endif
         default:
             return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 }
 
 int mbedtls_md_update(mbedtls_md_context_t *ctx, const unsigned char *input, size_t ilen)
 {
 #if defined(MBEDTLS_MD_C)
     if (ctx == NULL || ctx->md_info == NULL) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 #endif
 
     switch (ctx->md_info->type) {
 #if defined(MBEDTLS_MD5_C)
         case MBEDTLS_MD_MD5:
             return mbedtls_md5_update(ctx->md_ctx, input, ilen);
 #endif
 #if defined(MBEDTLS_SHA256_C)
         case MBEDTLS_MD_SHA256:
             return mbedtls_sha256_update(ctx->md_ctx, input, ilen);
 #endif
 #if defined(MBEDTLS_SHA384_C)
         case MBEDTLS_MD_SHA384:
             return mbedtls_sha512_update(ctx->md_ctx, input, ilen);
 #endif
         default:
             return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 }
 
 int mbedtls_md_finish(mbedtls_md_context_t *ctx, unsigned char *output)
 {
 #if defined(MBEDTLS_MD_C)
     if (ctx == NULL || ctx->md_info == NULL) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 #endif

     switch (ctx->md_info->type) {
 #if defined(MBEDTLS_MD5_C)
         case MBEDTLS_MD_MD5:
             return mbedtls_md5_finish(ctx->md_ctx, output);
 #endif
 #if defined(MBEDTLS_SHA256_C)
         case MBEDTLS_MD_SHA256:
             return mbedtls_sha256_finish(ctx->md_ctx, output);
 #endif
         default:
             return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 }
 
 int mbedtls_md(const mbedtls_md_info_t *md_info, const unsigned char *input, size_t ilen,
                unsigned char *output)
 {
     if (md_info == NULL) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 
     switch (md_info->type) {
 #if defined(MBEDTLS_MD5_C)
         case MBEDTLS_MD_MD5:
             return mbedtls_md5(input, ilen, output);
 #endif
 #if defined(MBEDTLS_RIPEMD160_C)
         case MBEDTLS_MD_RIPEMD160:
             return mbedtls_ripemd160(input, ilen, output);
 #endif
 #if defined(MBEDTLS_SHA256_C)
         case MBEDTLS_MD_SHA256:
             return mbedtls_sha256(input, ilen, output, 0);
 #endif
         default:
             return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 }
 
 unsigned char mbedtls_md_get_size(const mbedtls_md_info_t *md_info)
 {
     if (md_info == NULL) {
         return 0;
     }
 
     return md_info->size;
 }
 
 mbedtls_md_type_t mbedtls_md_get_type(const mbedtls_md_info_t *md_info)
 {
     if (md_info == NULL) {
         return MBEDTLS_MD_NONE;
     }
 
     return md_info->type;
 }
 
 #if defined(MBEDTLS_PSA_CRYPTO_C)
 int mbedtls_md_error_from_psa(psa_status_t status)
 {
     return PSA_TO_MBEDTLS_ERR_LIST(status, psa_to_md_errors,
                                    psa_generic_status_to_mbedtls);
 }
 #endif /* MBEDTLS_PSA_CRYPTO_C */
 
 
 /************************************************************************
  * Functions above this separator are part of MBEDTLS_MD_LIGHT,         *
  * functions below are only available when MBEDTLS_MD_C is set.         *
  ************************************************************************/
 #if defined(MBEDTLS_MD_C)
 
 /*
  * Reminder: update profiles in x509_crt.c when adding a new hash!
  */
 static const int supported_digests[] = {

 #if defined(MBEDTLS_MD_CAN_SHA256)
     MBEDTLS_MD_SHA256,
 #endif

 #if defined(MBEDTLS_MD_CAN_MD5)
     MBEDTLS_MD_MD5,
 #endif
 
     MBEDTLS_MD_NONE
 };
 
 const int *mbedtls_md_list(void)
 {
     return supported_digests;
 }
 
 typedef struct {
     const char *md_name;
     mbedtls_md_type_t md_type;
 } md_name_entry;
 
 static const md_name_entry md_names[] = {
 #if defined(MBEDTLS_MD_CAN_MD5)
     { "MD5", MBEDTLS_MD_MD5 },
 #endif
 #if defined(MBEDTLS_MD_CAN_SHA256)
     { "SHA256", MBEDTLS_MD_SHA256 },
 #endif
     { NULL, MBEDTLS_MD_NONE },
 };
 
 const mbedtls_md_info_t *mbedtls_md_info_from_string(const char *md_name)
 {
     if (NULL == md_name) {
         return NULL;
     }
 
     const md_name_entry *entry = md_names;
     while (entry->md_name != NULL &&
            strcmp(entry->md_name, md_name) != 0) {
         ++entry;
     }
 
     return mbedtls_md_info_from_type(entry->md_type);
 }
 
 const char *mbedtls_md_get_name(const mbedtls_md_info_t *md_info)
 {
     if (md_info == NULL) {
         return NULL;
     }
 
     const md_name_entry *entry = md_names;
     while (entry->md_type != MBEDTLS_MD_NONE &&
            entry->md_type != md_info->type) {
         ++entry;
     }
 
     return entry->md_name;
 }
 
 const mbedtls_md_info_t *mbedtls_md_info_from_ctx(
     const mbedtls_md_context_t *ctx)
 {
     if (ctx == NULL) {
         return NULL;
     }
 
     return ctx->MBEDTLS_PRIVATE(md_info);
 }

 int mbedtls_md_hmac_starts(mbedtls_md_context_t *ctx, const unsigned char *key, size_t keylen)
 {
     int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
     unsigned char sum[MBEDTLS_MD_MAX_SIZE];
     unsigned char *ipad, *opad;
 
     if (ctx == NULL || ctx->md_info == NULL || ctx->hmac_ctx == NULL) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 
     if (keylen > (size_t) ctx->md_info->block_size) {
         if ((ret = mbedtls_md_starts(ctx)) != 0) {
             goto cleanup;
         }
         if ((ret = mbedtls_md_update(ctx, key, keylen)) != 0) {
             goto cleanup;
         }
         if ((ret = mbedtls_md_finish(ctx, sum)) != 0) {
             goto cleanup;
         }
 
         keylen = ctx->md_info->size;
         key = sum;
     }
 
     ipad = (unsigned char *) ctx->hmac_ctx;
     opad = (unsigned char *) ctx->hmac_ctx + ctx->md_info->block_size;
 
     memset(ipad, 0x36, ctx->md_info->block_size);
     memset(opad, 0x5C, ctx->md_info->block_size);
 
     mbedtls_xor(ipad, ipad, key, keylen);
     mbedtls_xor(opad, opad, key, keylen);
 
     if ((ret = mbedtls_md_starts(ctx)) != 0) {
         goto cleanup;
     }
     if ((ret = mbedtls_md_update(ctx, ipad,
                                  ctx->md_info->block_size)) != 0) {
         goto cleanup;
     }
 
 cleanup:
     mbedtls_platform_zeroize(sum, sizeof(sum));
 
     return ret;
 }
 
 int mbedtls_md_hmac_update(mbedtls_md_context_t *ctx, const unsigned char *input, size_t ilen)
 {
     if (ctx == NULL || ctx->md_info == NULL || ctx->hmac_ctx == NULL) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 
     return mbedtls_md_update(ctx, input, ilen);
 }
 
 int mbedtls_md_hmac_finish(mbedtls_md_context_t *ctx, unsigned char *output)
 {
     int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
     unsigned char tmp[MBEDTLS_MD_MAX_SIZE];
     unsigned char *opad;
 
     if (ctx == NULL || ctx->md_info == NULL || ctx->hmac_ctx == NULL) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 
     opad = (unsigned char *) ctx->hmac_ctx + ctx->md_info->block_size;
 
     if ((ret = mbedtls_md_finish(ctx, tmp)) != 0) {
         return ret;
     }
     if ((ret = mbedtls_md_starts(ctx)) != 0) {
         return ret;
     }
     if ((ret = mbedtls_md_update(ctx, opad,
                                  ctx->md_info->block_size)) != 0) {
         return ret;
     }
     if ((ret = mbedtls_md_update(ctx, tmp,
                                  ctx->md_info->size)) != 0) {
         return ret;
     }
     return mbedtls_md_finish(ctx, output);
 }
 
 int mbedtls_md_hmac_reset(mbedtls_md_context_t *ctx)
 {
     int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
     unsigned char *ipad;
 
     if (ctx == NULL || ctx->md_info == NULL || ctx->hmac_ctx == NULL) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 
     ipad = (unsigned char *) ctx->hmac_ctx;
 
     if ((ret = mbedtls_md_starts(ctx)) != 0) {
         return ret;
     }
     return mbedtls_md_update(ctx, ipad, ctx->md_info->block_size);
 }
 
 int mbedtls_md_hmac(const mbedtls_md_info_t *md_info,
                     const unsigned char *key, size_t keylen,
                     const unsigned char *input, size_t ilen,
                     unsigned char *output)
 {
     mbedtls_md_context_t ctx;
     int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
 
     if (md_info == NULL) {
         return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
     }
 
     mbedtls_md_init(&ctx);
 
     if ((ret = mbedtls_md_setup(&ctx, md_info, 1)) != 0) {
         goto cleanup;
     }
 
     if ((ret = mbedtls_md_hmac_starts(&ctx, key, keylen)) != 0) {
         goto cleanup;
     }
     if ((ret = mbedtls_md_hmac_update(&ctx, input, ilen)) != 0) {
         goto cleanup;
     }
     if ((ret = mbedtls_md_hmac_finish(&ctx, output)) != 0) {
         goto cleanup;
     }
 
 cleanup:
     mbedtls_md_free(&ctx);
 
     return ret;
 }
 
 #endif /* MBEDTLS_MD_C */
 
 #endif /* MBEDTLS_MD_LIGHT */
 