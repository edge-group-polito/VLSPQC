/*
 * main_mldsa.c
 *
 * KAT test runner for ML-DSA.
 * Uses explicit seed parameters:
 *   crypto_sign_keypair(pk, sk, keypair_seed)
 *   crypto_sign(sm, &smlen, m, mlen, NULL, 0, sk, sign_rnd)
 *   crypto_sign_open(m, &mlen, sm, smlen, NULL, 0, pk)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "api.h"
#include "params.h"
#include "sign.h"
#include "kat_parser.h"

#ifndef KATNUM
#define KATNUM  10000
#endif
/* runtime override: KATNUM env var takes precedence */
static int katnum = KATNUM;
static void __attribute__((constructor)) katnum_init(void) {
    const char *s = getenv("KATNUM"); if (s) katnum = atoi(s); }

#ifndef RUN_KEYGEN
#define RUN_KEYGEN  1
#endif
#ifndef RUN_SIGN
#define RUN_SIGN    1
#endif
#ifndef RUN_VERIFY
#define RUN_VERIFY  1
#endif
#ifndef DEBUG
#define DEBUG       0
#endif
#ifndef RSP_PATH
#error "RSP_PATH must be defined"
#endif

#if DEBUG
static void print_hex(const char *label, const uint8_t *buf, size_t len)
{
    printf("  %s = ", label);
    for (size_t i = 0; i < len; i++) printf("%02X", buf[i]);
    printf("\n");
}
#define DBGHEX(l, b, n) print_hex(l, b, n)
#else
#define DBGHEX(l, b, n) ((void)0)
#endif

int main(void)
{
    kat_parser_t parser;
    kat_entry_t  entry;

    uint8_t keypair_seed[SEEDBYTES];
    uint8_t sign_rnd[RNDBYTES];
    uint8_t msg[1024];
    long    mlen_rsp = 0;
    uint8_t pk_rsp[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk_rsp[CRYPTO_SECRETKEYBYTES];
    long    smlen_rsp = 0;

    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t *sm    = NULL;
    uint8_t *m_out = NULL;
    size_t  smlen, mlen_out;

    int pass_key = 0, fail_key = 0;
    int pass_sign = 0, fail_sign = 0;
    int pass_ver = 0, fail_ver = 0;
    int count = 0, ret;

    printf("[%s] Opening .rsp: %s\n", CRYPTO_ALGNAME, RSP_PATH);
    if (kat_parser_open(&parser, RSP_PATH) != 0) return 1;

    printf("Testing: ");
#if RUN_KEYGEN
    printf("keygen ");
#endif
#if RUN_SIGN
    printf("sign ");
#endif
#if RUN_VERIFY
    printf("verify ");
#endif
    printf("\n");

    while (count < katnum) {
        ret = kat_parser_next(&parser, &entry);
        if (ret == 1) break;
        if (ret != 0) { fprintf(stderr, "parse error\n"); break; }

        if (kat_get_bytes(&entry, "keypair_seed", keypair_seed, SEEDBYTES) != 0) goto next;
        if (kat_get_bytes(&entry, "sign_rnd",     sign_rnd,     RNDBYTES)  != 0) goto next;
        if (kat_get_long(&entry,  "mlen",          &mlen_rsp) != 0) goto next;
        if (kat_get_bytes(&entry, "msg",  msg, (size_t)mlen_rsp) != 0) goto next;
        if (kat_get_bytes(&entry, "pk", pk_rsp, CRYPTO_PUBLICKEYBYTES) != 0) goto next;
        if (kat_get_bytes(&entry, "sk", sk_rsp, CRYPTO_SECRETKEYBYTES) != 0) goto next;
        if (kat_get_long(&entry,  "smlen", &smlen_rsp) != 0) goto next;
        kat_field_t *sm_f = kat_entry_get(&entry, "sm");
        if (!sm_f || !sm_f->data) goto next;

#if RUN_KEYGEN
        ret = crypto_sign_keypair(pk, sk, keypair_seed);
        if (ret != 0 || memcmp(pk, pk_rsp, CRYPTO_PUBLICKEYBYTES) != 0
                     || memcmp(sk, sk_rsp, CRYPTO_SECRETKEYBYTES) != 0) {
            printf("[%d] FAIL: keygen\n", entry.count);
            DBGHEX("pk_got", pk, CRYPTO_PUBLICKEYBYTES);
            fail_key++;
        } else {
            printf("[%d] PASS: keygen\n", entry.count);
            pass_key++;
        }
#else
        memcpy(pk, pk_rsp, CRYPTO_PUBLICKEYBYTES);
        memcpy(sk, sk_rsp, CRYPTO_SECRETKEYBYTES);
#endif

#if RUN_SIGN
        {
            size_t sm_max = (size_t)mlen_rsp + CRYPTO_BYTES;
            sm = (uint8_t *)malloc(sm_max);
            if (!sm) { perror("malloc"); goto next; }
            ret = crypto_sign(sm, &smlen, msg, (size_t)mlen_rsp, NULL, 0, sk, sign_rnd);
            if (ret != 0 || smlen != (size_t)smlen_rsp
                         || memcmp(sm, sm_f->data, (size_t)smlen_rsp) != 0) {
                printf("[%d] FAIL: sign\n", entry.count);
                DBGHEX("sm_got", sm, smlen);
                fail_sign++;
            } else {
                printf("[%d] PASS: sign\n", entry.count);
                pass_sign++;
            }
        }
#else
        sm = (uint8_t *)malloc(sm_f->len);
        if (!sm) { perror("malloc"); goto next; }
        memcpy(sm, sm_f->data, sm_f->len);
        smlen = (size_t)smlen_rsp;
#endif

#if RUN_VERIFY
        m_out = (uint8_t *)malloc(smlen + 1);
        if (!m_out) { perror("malloc"); goto next; }
        ret = crypto_sign_open(m_out, &mlen_out, sm, smlen, NULL, 0, pk);
        if (ret != 0 || mlen_out != (size_t)mlen_rsp
                     || memcmp(m_out, msg, (size_t)mlen_rsp) != 0) {
            printf("[%d] FAIL: verify\n", entry.count);
            fail_ver++;
        } else {
            printf("[%d] PASS: verify\n", entry.count);
            pass_ver++;
        }
#endif

next:
        free(sm); sm = NULL; free(m_out); m_out = NULL;
        kat_entry_free(&entry);
        count++;
    }

    kat_parser_close(&parser);
    printf("\n=== Results for %s ===\n", CRYPTO_ALGNAME);
#if RUN_KEYGEN
    printf("  Keygen : %d pass, %d fail\n", pass_key,  fail_key);
#endif
#if RUN_SIGN
    printf("  Sign   : %d pass, %d fail\n", pass_sign, fail_sign);
#endif
#if RUN_VERIFY
    printf("  Verify : %d pass, %d fail\n", pass_ver,  fail_ver);
#endif
    int total_fail = fail_key + fail_sign + fail_ver;
    printf("  Total  : %d vectors, %d failures\n", count, total_fail);
    return (total_fail > 0) ? 1 : 0;
}
