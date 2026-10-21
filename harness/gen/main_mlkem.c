/*
 * main_mlkem.c
 *
 * KAT test runner for ML-KEM (Kyber).
 * Uses derand functions:
 *   crypto_kem_keypair_derand(pk, sk, keypair_coins)  -- 64 bytes
 *   crypto_kem_enc_derand(ct, ss, pk, enc_coins)      -- 32 bytes
 *   crypto_kem_dec(ss, ct, sk)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kem.h"
#include "api.h"
#include "params.h"
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
#ifndef RUN_ENCAPS
#define RUN_ENCAPS  1
#endif
#ifndef RUN_DECAPS
#define RUN_DECAPS  1
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

    uint8_t keypair_coins[KYBER_SYMBYTES * 2];
    uint8_t enc_coins[KYBER_SYMBYTES];
    uint8_t pk_rsp[KYBER_PUBLICKEYBYTES];
    uint8_t sk_rsp[KYBER_SECRETKEYBYTES];
    uint8_t ct_rsp[KYBER_CIPHERTEXTBYTES];
    uint8_t ss_rsp[KYBER_SSBYTES];

    uint8_t pk[KYBER_PUBLICKEYBYTES];
    uint8_t sk[KYBER_SECRETKEYBYTES];
    uint8_t ct[KYBER_CIPHERTEXTBYTES];
    uint8_t ss_enc[KYBER_SSBYTES];
    uint8_t ss_dec[KYBER_SSBYTES];

    int pass_key = 0, fail_key = 0;
    int pass_enc = 0, fail_enc = 0;
    int pass_dec = 0, fail_dec = 0;
    int count = 0, ret;

    printf("[ML-KEM] Opening .rsp: %s\n", RSP_PATH);
    if (kat_parser_open(&parser, RSP_PATH) != 0) return 1;

    printf("Testing: ");
#if RUN_KEYGEN
    printf("keygen ");
#endif
#if RUN_ENCAPS
    printf("encaps ");
#endif
#if RUN_DECAPS
    printf("decaps ");
#endif
    printf("\n");

    while (count < katnum) {
        ret = kat_parser_next(&parser, &entry);
        if (ret == 1) break;
        if (ret != 0) { fprintf(stderr, "parse error\n"); break; }

        if (kat_get_bytes(&entry, "keypair_coins", keypair_coins, KYBER_SYMBYTES*2) != 0) goto next;
        if (kat_get_bytes(&entry, "enc_coins",     enc_coins,     KYBER_SYMBYTES)   != 0) goto next;
        if (kat_get_bytes(&entry, "pk", pk_rsp, KYBER_PUBLICKEYBYTES)  != 0) goto next;
        if (kat_get_bytes(&entry, "sk", sk_rsp, KYBER_SECRETKEYBYTES)  != 0) goto next;
        if (kat_get_bytes(&entry, "ct", ct_rsp, KYBER_CIPHERTEXTBYTES) != 0) goto next;
        if (kat_get_bytes(&entry, "ss", ss_rsp, KYBER_SSBYTES)         != 0) goto next;

#if RUN_KEYGEN
        ret = crypto_kem_keypair_derand(pk, sk, keypair_coins);
        if (ret != 0 || memcmp(pk, pk_rsp, KYBER_PUBLICKEYBYTES) != 0
                     || memcmp(sk, sk_rsp, KYBER_SECRETKEYBYTES) != 0) {
            printf("[%d] FAIL: keygen\n", entry.count);
            DBGHEX("pk_got", pk, KYBER_PUBLICKEYBYTES);
            DBGHEX("pk_exp", pk_rsp, KYBER_PUBLICKEYBYTES);
            fail_key++;
        } else {
            printf("[%d] PASS: keygen\n", entry.count);
            pass_key++;
        }
#else
        memcpy(pk, pk_rsp, KYBER_PUBLICKEYBYTES);
        memcpy(sk, sk_rsp, KYBER_SECRETKEYBYTES);
#endif

#if RUN_ENCAPS
        ret = crypto_kem_enc_derand(ct, ss_enc, pk, enc_coins);
        if (ret != 0 || memcmp(ct,     ct_rsp, KYBER_CIPHERTEXTBYTES) != 0
                     || memcmp(ss_enc, ss_rsp, KYBER_SSBYTES)         != 0) {
            printf("[%d] FAIL: encaps\n", entry.count);
            DBGHEX("ct_got",  ct,     KYBER_CIPHERTEXTBYTES);
            DBGHEX("ss_got",  ss_enc, KYBER_SSBYTES);
            DBGHEX("ss_exp",  ss_rsp, KYBER_SSBYTES);
            fail_enc++;
        } else {
            printf("[%d] PASS: encaps\n", entry.count);
            pass_enc++;
        }
#else
        memcpy(ct, ct_rsp, KYBER_CIPHERTEXTBYTES);
#endif

#if RUN_DECAPS
        ret = crypto_kem_dec(ss_dec, ct, sk);
        if (ret != 0 || memcmp(ss_dec, ss_rsp, KYBER_SSBYTES) != 0) {
            printf("[%d] FAIL: decaps\n", entry.count);
            DBGHEX("ss_got", ss_dec, KYBER_SSBYTES);
            DBGHEX("ss_exp", ss_rsp, KYBER_SSBYTES);
            fail_dec++;
        } else {
            printf("[%d] PASS: decaps\n", entry.count);
            pass_dec++;
        }
#endif

next:
        kat_entry_free(&entry);
        count++;
    }

    kat_parser_close(&parser);
    printf("\n=== Results for ML-KEM ===\n");
#if RUN_KEYGEN
    printf("  Keygen : %d pass, %d fail\n", pass_key, fail_key);
#endif
#if RUN_ENCAPS
    printf("  Encaps : %d pass, %d fail\n", pass_enc, fail_enc);
#endif
#if RUN_DECAPS
    printf("  Decaps : %d pass, %d fail\n", pass_dec, fail_dec);
#endif
    int total_fail = fail_key + fail_enc + fail_dec;
    printf("  Total  : %d vectors, %d failures\n", count, total_fail);
    return (total_fail > 0) ? 1 : 0;
}
