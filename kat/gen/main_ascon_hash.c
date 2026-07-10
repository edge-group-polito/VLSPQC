/*
 * main_ascon_hash.c
 *
 * KAT test runner for ASCON's hash/XOF variants (asconhashv12,
 * asconhashav12, asconxofv12, asconxofav12). Single-stage: recompute
 * crypto_hash(msg) and compare against the stored digest. No RUN_* flag --
 * there is only one operation to gate.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "crypto_hash.h"
#include "kat_parser.h"

#ifndef MLEN
#define MLEN 33
#endif

#ifndef KATNUM
#define KATNUM  10000
#endif
/* runtime override: KATNUM env var takes precedence */
static int katnum = KATNUM;
static void __attribute__((constructor)) katnum_init(void) {
    const char *s = getenv("KATNUM"); if (s) katnum = atoi(s); }

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

    uint8_t msg_rsp[MLEN];
    uint8_t md_rsp[crypto_hash_BYTES];
    uint8_t md[crypto_hash_BYTES];

    int pass = 0, fail = 0;
    int count = 0, ret;

    printf("[ASCON-HASH] Opening .rsp: %s\n", RSP_PATH);
    if (kat_parser_open(&parser, RSP_PATH) != 0) return 1;

    printf("Testing: hash\n");

    while (count < katnum) {
        ret = kat_parser_next(&parser, &entry);
        if (ret == 1) break;
        if (ret != 0) { fprintf(stderr, "parse error\n"); break; }

        if (kat_get_bytes(&entry, "msg", msg_rsp, MLEN)             != 0) goto next;
        if (kat_get_bytes(&entry, "md",  md_rsp,  crypto_hash_BYTES) != 0) goto next;

        ret = crypto_hash(md, msg_rsp, MLEN);
        if (ret != 0 || memcmp(md, md_rsp, crypto_hash_BYTES) != 0) {
            printf("[%d] FAIL: hash\n", entry.count);
            DBGHEX("md_got", md,     crypto_hash_BYTES);
            DBGHEX("md_exp", md_rsp, crypto_hash_BYTES);
            fail++;
        } else {
            printf("[%d] PASS: hash\n", entry.count);
            pass++;
        }

next:
        kat_entry_free(&entry);
        count++;
    }

    kat_parser_close(&parser);
    printf("\n=== Results for %s ===\n", "ASCON-HASH");
    printf("  Hash   : %d pass, %d fail\n", pass, fail);
    printf("  Total  : %d vectors, %d failures\n", count, fail);
    return (fail > 0) ? 1 : 0;
}
