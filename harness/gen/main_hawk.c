/*
 * main_hawk.c
 *
 * KAT test runner for HAWK (and any DS using NIST DRBG + standard API):
 *   crypto_sign_keypair(pk, sk)
 *   crypto_sign(sm, &smlen, m, mlen, sk)
 *   crypto_sign_open(m, &mlen, sm, smlen, pk)
 *
 * Reads a .rsp file produced by PQCgenKAT_sign_hawk.c, re-seeds the DRBG
 * with the stored seed, re-runs the algorithm, and compares all outputs.
 *
 * Compile-time flags (set via -D in Makefile):
 *   KATNUM        – max test vectors to run (default: all in file)
 *   RUN_KEYGEN    – 1 = test keygen (default 1)
 *   RUN_SIGN      – 1 = test sign   (default 1)
 *   RUN_VERIFY    – 1 = test verify (default 1)
 *   DEBUG         – 1 = print intermediate values (default 0)
 *   RSP_PATH      – path to the .rsp file (required; set by Makefile)
 *
 * HAWK has no explicit-randomness entry point, so with RUN_KEYGEN=0 the
 * real crypto_sign_keypair() is still called (output discarded in favor of
 * the .rsp values) purely to advance the DRBG to the position sign()
 * expects -- same fix already applied to CROSS/LESS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "rng.h"
#include "api.h"

/* Pull in the KAT parser from common/ */
#include "kat_parser.h"

#ifndef KATNUM
#define KATNUM  10000   /* large default → run everything in file */
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
#error "RSP_PATH must be defined (path to the .rsp file)"
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

    uint8_t seed[48];
    uint8_t msg[1024];
    long    mlen_rsp = 0;
    uint8_t pk_rsp[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk_rsp[CRYPTO_SECRETKEYBYTES];
    long    smlen_rsp = 0;

    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t *sm    = NULL;
    uint8_t *m_out = NULL;
    unsigned long long smlen, mlen_out;

    int pass_key = 0, fail_key = 0;
    int pass_sign = 0, fail_sign = 0;
    int pass_ver = 0, fail_ver = 0;
    int count = 0;
    int ret;

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
        if (ret == 1) break;   /* EOF */
        if (ret != 0) { fprintf(stderr, "parse error\n"); break; }

        if (kat_get_bytes(&entry, "seed", seed, 48)  != 0) goto next;
        if (kat_get_long(&entry, "mlen", &mlen_rsp)  != 0) goto next;
        if (kat_get_bytes(&entry, "msg", msg, (size_t)mlen_rsp) != 0) goto next;
        if (kat_get_bytes(&entry, "pk", pk_rsp, CRYPTO_PUBLICKEYBYTES)  != 0) goto next;
        if (kat_get_bytes(&entry, "sk", sk_rsp, CRYPTO_SECRETKEYBYTES)  != 0) goto next;
        if (kat_get_long(&entry, "smlen", &smlen_rsp) != 0) goto next;

        kat_field_t *sm_f = kat_entry_get(&entry, "sm");
        if (!sm_f || !sm_f->data) goto next;

        /* Re-seed DRBG exactly as generator did, then advance past
         * the randombytes(msg, mlen) call the generator made before keygen. */
        randombytes_init(seed, NULL, 256);
        {
            uint8_t _skip[1024];
            randombytes(_skip, (unsigned long long)mlen_rsp);
        }

        /* ------- KEYGEN ------- */
#if RUN_KEYGEN
        ret = crypto_sign_keypair(pk, sk);
        if (ret != 0) {
            printf("[%d] FAIL: crypto_sign_keypair returned %d\n", entry.count, ret);
            fail_key++; goto skip_sign;
        }
        if (memcmp(pk, pk_rsp, CRYPTO_PUBLICKEYBYTES) != 0) {
            printf("[%d] FAIL: PK mismatch\n", entry.count);
            DBGHEX("pk_got", pk, CRYPTO_PUBLICKEYBYTES);
            DBGHEX("pk_exp", pk_rsp, CRYPTO_PUBLICKEYBYTES);
            fail_key++;
        } else if (memcmp(sk, sk_rsp, CRYPTO_SECRETKEYBYTES) != 0) {
            printf("[%d] FAIL: SK mismatch\n", entry.count);
            fail_key++;
        } else {
            printf("[%d] PASS: keygen\n", entry.count);
            pass_key++;
        }
#else
        /* Still call the real keypair -- HAWK's keygen consumes a variable
         * amount of randomness (internal rejection-sampling retry loop), so
         * only calling the real function reproduces the DRBG's exact
         * position for sign() below; a hand-replicated "throwaway read of N
         * bytes" (as CROSS's fixed-size keygen allows) can't be done safely
         * here. Output is discarded in favor of the .rsp values. */
        ret = crypto_sign_keypair(pk, sk);
        memcpy(pk, pk_rsp, CRYPTO_PUBLICKEYBYTES);
        memcpy(sk, sk_rsp, CRYPTO_SECRETKEYBYTES);
#endif

skip_sign:
        /* ------- SIGN ------- */
#if RUN_SIGN
        {
            size_t sm_max = (size_t)mlen_rsp + CRYPTO_BYTES;
            sm = (uint8_t *)malloc(sm_max);
            if (!sm) { perror("malloc"); goto next; }
            ret = crypto_sign(sm, &smlen, msg, (size_t)mlen_rsp, sk);
            if (ret != 0) {
                printf("[%d] FAIL: crypto_sign returned %d\n", entry.count, ret);
                fail_sign++;
            } else if (smlen != (unsigned long long)smlen_rsp) {
                printf("[%d] FAIL: smlen mismatch (%llu != %ld)\n",
                       entry.count, smlen, smlen_rsp);
                fail_sign++;
            } else if (memcmp(sm, sm_f->data, (size_t)smlen_rsp) != 0) {
                printf("[%d] FAIL: SM mismatch\n", entry.count);
                DBGHEX("sm_got", sm, smlen);
                DBGHEX("sm_exp", sm_f->data, sm_f->len);
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
        smlen = (unsigned long long)smlen_rsp;
#endif

        /* ------- VERIFY ------- */
#if RUN_VERIFY
        {
            m_out = (uint8_t *)malloc(smlen + 1);
            if (!m_out) { perror("malloc"); goto next; }
            ret = crypto_sign_open(m_out, &mlen_out, sm, smlen, pk);
            if (ret != 0) {
                printf("[%d] FAIL: crypto_sign_open returned %d\n", entry.count, ret);
                fail_ver++;
            } else if (mlen_out != (unsigned long long)mlen_rsp ||
                       memcmp(m_out, msg, (size_t)mlen_rsp) != 0) {
                printf("[%d] FAIL: recovered message mismatch\n", entry.count);
                DBGHEX("msg_got", m_out, mlen_out);
                DBGHEX("msg_exp", msg, (size_t)mlen_rsp);
                fail_ver++;
            } else {
                printf("[%d] PASS: verify\n", entry.count);
                pass_ver++;
            }
        }
#endif

next:
        free(sm);    sm    = NULL;
        free(m_out); m_out = NULL;
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
