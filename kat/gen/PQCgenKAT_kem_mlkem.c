/*
 * PQCgenKAT_kem_mlkem.c
 *
 * KAT generator for ML-KEM (Kyber).
 * ML-KEM provides deterministic ("derand") functions:
 *   crypto_kem_keypair_derand(pk, sk, coins)   -- coins: 64 bytes
 *   crypto_kem_enc_derand(ct, ss, pk, coins)   -- coins: 32 bytes
 *   crypto_kem_dec(ss, ct, sk)
 *
 * RSP fields written:
 *   count, seed, keypair_coins, enc_coins, pk, sk, ct, ss
 *
 * Compile with ML-KEM sources (without randombytes.c) and rng.c.
 * Needs: -I <ml-kem-variant>/include/
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rng.h"
#include "kem.h"
#include "api.h"
#include "params.h"

#ifndef KATNUM
#define KATNUM 10
#endif

#ifndef RSP_PATH
#define RSP_PATH "PQCkemKAT.rsp"
#endif

#ifndef ALGO_NAME
#define ALGO_NAME "ML-KEM"
#endif

int main(void)
{
    uint8_t entropy_input[48];
    uint8_t seed[48];
    uint8_t keypair_coins[KYBER_SYMBYTES * 2];   /* 64 bytes */
    uint8_t enc_coins[KYBER_SYMBYTES];             /* 32 bytes */
    uint8_t pk[KYBER_PUBLICKEYBYTES];
    uint8_t sk[KYBER_SECRETKEYBYTES];
    uint8_t ct[KYBER_CIPHERTEXTBYTES];
    uint8_t ss_enc[KYBER_SSBYTES];
    uint8_t ss_dec[KYBER_SSBYTES];
    int ret;

    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(fp, "# %s\n\n", ALGO_NAME);

    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;

    for (int count = 0; count < KATNUM; count++) {

        randombytes_init(entropy_input, NULL, 256);
        randombytes(seed, 48);
        randombytes_init(seed, NULL, 256);

        randombytes(keypair_coins, KYBER_SYMBYTES * 2);
        randombytes(enc_coins,     KYBER_SYMBYTES);

        ret = crypto_kem_keypair_derand(pk, sk, keypair_coins);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_kem_keypair_derand failed\n", count);
            fclose(fp); return 1;
        }

        ret = crypto_kem_enc_derand(ct, ss_enc, pk, enc_coins);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_kem_enc_derand failed\n", count);
            fclose(fp); return 1;
        }

        ret = crypto_kem_dec(ss_dec, ct, sk);
        if (ret != 0 || memcmp(ss_enc, ss_dec, KYBER_SSBYTES) != 0) {
            fprintf(stderr, "count=%d: decapsulation mismatch\n", count);
            fclose(fp); return 1;
        }

        fprintf(fp, "count = %d\n", count);
        fprintf(fp, "seed = ");
        for (int i = 0; i < 48; i++) fprintf(fp, "%02X", seed[i]);
        fprintf(fp, "\n");
        fprintf(fp, "keypair_coins = ");
        for (int i = 0; i < KYBER_SYMBYTES*2; i++) fprintf(fp, "%02X", keypair_coins[i]);
        fprintf(fp, "\n");
        fprintf(fp, "enc_coins = ");
        for (int i = 0; i < KYBER_SYMBYTES; i++) fprintf(fp, "%02X", enc_coins[i]);
        fprintf(fp, "\n");
        fprintf(fp, "pk = ");
        for (int i = 0; i < KYBER_PUBLICKEYBYTES; i++) fprintf(fp, "%02X", pk[i]);
        fprintf(fp, "\n");
        fprintf(fp, "sk = ");
        for (int i = 0; i < KYBER_SECRETKEYBYTES; i++) fprintf(fp, "%02X", sk[i]);
        fprintf(fp, "\n");
        fprintf(fp, "ct = ");
        for (int i = 0; i < KYBER_CIPHERTEXTBYTES; i++) fprintf(fp, "%02X", ct[i]);
        fprintf(fp, "\n");
        fprintf(fp, "ss = ");
        for (int i = 0; i < KYBER_SSBYTES; i++) fprintf(fp, "%02X", ss_enc[i]);
        fprintf(fp, "\n\n");

        for (int i = 47; i >= 0; i--) { if (++entropy_input[i]) break; }
    }

    fclose(fp);
    printf("Generated %d KAT vectors → %s\n", KATNUM, RSP_PATH);
    return 0;
}
