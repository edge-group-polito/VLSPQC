/*
 * main_ascon_aead.c
 *
 * KAT test runner for ASCON's AEAD variants (ascon128v12, ascon128av12,
 * ascon80pqv12). Uses the direct (non-derand) API:
 *   crypto_aead_encrypt(c, &clen, m, mlen, ad, adlen, NULL, npub, k)
 *   crypto_aead_decrypt(m, &mlen, NULL, c, clen, ad, adlen, npub, k)
 *
 * RUN_ENCAPS / RUN_DECAPS gate encrypt / decrypt respectively -- reusing the
 * same flag names KEM families use for encaps/decaps, since ASCON has no
 * keygen and the two stages map onto the same slots.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "crypto_aead.h"
#include "kat_parser.h"

#ifndef MLEN
#define MLEN 33
#endif

#ifndef ADLEN
#define ADLEN 16
#endif

#ifndef KATNUM
#define KATNUM  10000
#endif
/* runtime override: KATNUM env var takes precedence */
static int katnum = KATNUM;
static void __attribute__((constructor)) katnum_init(void) {
    const char *s = getenv("KATNUM"); if (s) katnum = atoi(s); }

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

    uint8_t key_rsp[CRYPTO_KEYBYTES];
    uint8_t nonce_rsp[CRYPTO_NPUBBYTES];
    uint8_t ad_rsp[ADLEN];
    uint8_t pt_rsp[MLEN];
    uint8_t ct_rsp[MLEN + CRYPTO_ABYTES];

    uint8_t ct[MLEN + CRYPTO_ABYTES];
    uint8_t pt[MLEN];
    unsigned long long clen, mlen2;

    int pass_enc = 0, fail_enc = 0;
    int pass_dec = 0, fail_dec = 0;
    int count = 0, ret;

    printf("[ASCON-AEAD] Opening .rsp: %s\n", RSP_PATH);
    if (kat_parser_open(&parser, RSP_PATH) != 0) return 1;

    printf("Testing: ");
#if RUN_ENCAPS
    printf("encrypt ");
#endif
#if RUN_DECAPS
    printf("decrypt ");
#endif
    printf("\n");

    while (count < katnum) {
        ret = kat_parser_next(&parser, &entry);
        if (ret == 1) break;
        if (ret != 0) { fprintf(stderr, "parse error\n"); break; }

        if (kat_get_bytes(&entry, "key",   key_rsp,   CRYPTO_KEYBYTES)  != 0) goto next;
        if (kat_get_bytes(&entry, "nonce", nonce_rsp, CRYPTO_NPUBBYTES) != 0) goto next;
        if (kat_get_bytes(&entry, "ad",    ad_rsp,    ADLEN)            != 0) goto next;
        if (kat_get_bytes(&entry, "pt",    pt_rsp,    MLEN)             != 0) goto next;
        if (kat_get_bytes(&entry, "ct",    ct_rsp,    MLEN + CRYPTO_ABYTES) != 0) goto next;

#if RUN_ENCAPS
        ret = crypto_aead_encrypt(ct, &clen, pt_rsp, MLEN, ad_rsp, ADLEN, NULL, nonce_rsp, key_rsp);
        if (ret != 0 || clen != (unsigned long long)(MLEN + CRYPTO_ABYTES)
                     || memcmp(ct, ct_rsp, MLEN + CRYPTO_ABYTES) != 0) {
            printf("[%d] FAIL: encrypt\n", entry.count);
            DBGHEX("ct_got", ct,     MLEN + CRYPTO_ABYTES);
            DBGHEX("ct_exp", ct_rsp, MLEN + CRYPTO_ABYTES);
            fail_enc++;
        } else {
            printf("[%d] PASS: encrypt\n", entry.count);
            pass_enc++;
        }
#else
        memcpy(ct, ct_rsp, MLEN + CRYPTO_ABYTES);
        clen = MLEN + CRYPTO_ABYTES;
#endif

#if RUN_DECAPS
        ret = crypto_aead_decrypt(pt, &mlen2, NULL, ct, clen, ad_rsp, ADLEN, nonce_rsp, key_rsp);
        if (ret != 0 || mlen2 != (unsigned long long)MLEN || memcmp(pt, pt_rsp, MLEN) != 0) {
            printf("[%d] FAIL: decrypt\n", entry.count);
            DBGHEX("pt_got", pt,     MLEN);
            DBGHEX("pt_exp", pt_rsp, MLEN);
            fail_dec++;
        } else {
            printf("[%d] PASS: decrypt\n", entry.count);
            pass_dec++;
        }
#endif

next:
        kat_entry_free(&entry);
        count++;
    }

    kat_parser_close(&parser);
    printf("\n=== Results for %s ===\n", "ASCON-AEAD");
#if RUN_ENCAPS
    printf("  Encrypt: %d pass, %d fail\n", pass_enc, fail_enc);
#endif
#if RUN_DECAPS
    printf("  Decrypt: %d pass, %d fail\n", pass_dec, fail_dec);
#endif
    int total_fail = fail_enc + fail_dec;
    printf("  Total  : %d vectors, %d failures\n", count, total_fail);
    return (total_fail > 0) ? 1 : 0;
}
