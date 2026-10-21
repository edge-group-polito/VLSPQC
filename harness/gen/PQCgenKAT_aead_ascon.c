/*
 * PQCgenKAT_aead_ascon.c
 *
 * KAT generator for ASCON's AEAD variants (ascon128v12, ascon128av12,
 * ascon80pqv12). Unlike the PQC KEM/DS families, ASCON's AEAD API is not
 * derand/coin-based -- crypto_aead_encrypt()/crypto_aead_decrypt() already
 * take key/nonce/ad/plaintext directly and are deterministic by
 * construction, so there is no reseed-desync caveat and no coins to store.
 *
 * RSP fields written:
 *   count, seed, key, nonce, ad, pt, ct
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
#include "crypto_aead.h"

#ifndef KATNUM
#define KATNUM 10
#endif

#ifndef MLEN
#define MLEN 33
#endif

#ifndef ADLEN
#define ADLEN 16
#endif

#ifndef RSP_PATH
#define RSP_PATH "PQCaeadKAT.rsp"
#endif

#ifndef ALGO_NAME
#define ALGO_NAME "ASCON-AEAD"
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
    uint8_t key[CRYPTO_KEYBYTES];
    uint8_t nonce[CRYPTO_NPUBBYTES];
    uint8_t ad[ADLEN];
    uint8_t pt[MLEN];
    uint8_t ct[MLEN + CRYPTO_ABYTES];
    uint8_t pt2[MLEN];
    unsigned long long clen, mlen2;
    int ret;

    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(fp, "# %s\n\n", ALGO_NAME);

    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;

    for (int count = 0; count < KATNUM; count++) {

        randombytes_init(entropy_input, NULL, 256);
        randombytes(seed, 48);
        randombytes_init(seed, NULL, 256);

        randombytes(key,   CRYPTO_KEYBYTES);
        randombytes(nonce, CRYPTO_NPUBBYTES);
        randombytes(ad,    ADLEN);
        randombytes(pt,    MLEN);

        ret = crypto_aead_encrypt(ct, &clen, pt, MLEN, ad, ADLEN, NULL, nonce, key);
        if (ret != 0 || clen != (unsigned long long)(MLEN + CRYPTO_ABYTES)) {
            fprintf(stderr, "count=%d: crypto_aead_encrypt failed\n", count);
            fclose(fp); return 1;
        }

        ret = crypto_aead_decrypt(pt2, &mlen2, NULL, ct, clen, ad, ADLEN, nonce, key);
        if (ret != 0 || mlen2 != (unsigned long long)MLEN || memcmp(pt, pt2, MLEN) != 0) {
            fprintf(stderr, "count=%d: crypto_aead_decrypt roundtrip mismatch\n", count);
            fclose(fp); return 1;
        }

        fprintf(fp, "count = %d\n", count);
        fprint_hex(fp, "seed",  seed,  48);
        fprint_hex(fp, "key",   key,   CRYPTO_KEYBYTES);
        fprint_hex(fp, "nonce", nonce, CRYPTO_NPUBBYTES);
        fprint_hex(fp, "ad",    ad,    ADLEN);
        fprint_hex(fp, "pt",    pt,    MLEN);
        fprint_hex(fp, "ct",    ct,    (size_t)clen);
        fprintf(fp, "\n");

        for (int i = 47; i >= 0; i--) { if (++entropy_input[i]) break; }
    }

    fclose(fp);
    printf("Generated %d KAT vectors -> %s\n", KATNUM, RSP_PATH);
    return 0;
}
