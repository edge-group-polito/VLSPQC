/*
 * PQCgenKAT_hash_ascon.c
 *
 * KAT generator for ASCON's hash/XOF variants (asconhashv12, asconhashav12,
 * asconxofv12, asconxofav12). Single-call API, no key/nonce/AD, no
 * randomness needed by the algorithm itself -- the message is the only
 * input, derived here from the NIST DRBG purely to get varied KAT vectors.
 *
 * RSP fields written:
 *   count, seed, msg, md
 *
 * Compile with the variant's src/ sources and harness/gen/rng.c.
 * Needs: -I <ascon-variant>/include/
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rng.h"
#include "api.h"
#include "crypto_hash.h"

#ifndef KATNUM
#define KATNUM 10
#endif

#ifndef MLEN
#define MLEN 33
#endif

#ifndef RSP_PATH
#define RSP_PATH "PQChashKAT.rsp"
#endif

#ifndef ALGO_NAME
#define ALGO_NAME "ASCON-HASH"
#endif

static void fprint_hex(FILE *fp, const char *label, const unsigned char *buf, size_t len)
{
    fprintf(fp, "%s = ", label);
    for (size_t i = 0; i < len; i++) fprintf(fp, "%02X", buf[i]);
    fprintf(fp, "\n");
}

int main(void)
{
    uint8_t entropy_input[48];
    uint8_t seed[48];
    uint8_t msg[MLEN];
    uint8_t md[crypto_hash_BYTES];
    int ret;

    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(fp, "# %s\n\n", ALGO_NAME);

    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;

    for (int count = 0; count < KATNUM; count++) {

        randombytes_init(entropy_input, NULL, 256);
        randombytes(seed, 48);
        randombytes_init(seed, NULL, 256);

        randombytes(msg, MLEN);

        ret = crypto_hash(md, msg, MLEN);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_hash failed\n", count);
            fclose(fp); return 1;
        }

        fprintf(fp, "count = %d\n", count);
        fprint_hex(fp, "seed", seed, 48);
        fprint_hex(fp, "msg",  msg,  MLEN);
        fprint_hex(fp, "md",   md,   crypto_hash_BYTES);
        fprintf(fp, "\n");

        for (int i = 47; i >= 0; i--) { if (++entropy_input[i]) break; }
    }

    fclose(fp);
    printf("Generated %d KAT vectors -> %s\n", KATNUM, RSP_PATH);
    return 0;
}
