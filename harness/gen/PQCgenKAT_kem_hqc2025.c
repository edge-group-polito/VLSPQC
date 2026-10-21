/*
 * PQCgenKAT_kem_hqc2025.c
 *
 * KAT generator for HQC-2025.
 * Uses the standard 3-arg NIST KEM API:
 *   crypto_kem_keypair(pk, sk)
 *   crypto_kem_enc(ct, ss, pk)
 *   crypto_kem_dec(ss, ct, sk)
 *
 * Randomness is generated internally via HQC's own SHAKE-256 PRNG
 * (prng_get_bytes), which must be seeded with prng_init() before each call.
 *
 * Seed derivation: NIST AES-256-CTR DRBG (rng.h/rng.c) produces a 48-byte
 * seed that is stored in the .rsp and fed to prng_init() to reproduce the
 * exact same PRNG sequence during testing.
 *
 * RSP fields written:
 *   count, seed (48 B), pk, sk, ct, ss
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "rng.h"        /* NIST DRBG – seed derivation only */
#include "api.h"
#include "symmetric.h"  /* prng_init, prng_get_bytes */

#ifndef KATNUM
#define KATNUM 10
#endif
#ifndef RSP_PATH
#define RSP_PATH "PQCkemKAT.rsp"
#endif
#ifndef ALGO_NAME
#define ALGO_NAME CRYPTO_ALGNAME
#endif

int main(void)
{
    uint8_t entropy_input[48];
    uint8_t seed[48];
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
    uint8_t ss_enc[CRYPTO_BYTES];
    uint8_t ss_dec[CRYPTO_BYTES];
    int ret;

    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(fp, "# %s\n\n", ALGO_NAME);

    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;

    for (int count = 0; count < KATNUM; count++) {

        /* Derive a deterministic seed from the NIST DRBG */
        randombytes_init(entropy_input, NULL, 256);
        randombytes(seed, 48);

        /* Seed HQC-2025's SHAKE-256 PRNG with that seed */
        prng_init(seed, NULL, 48, 0);

        ret = crypto_kem_keypair(pk, sk);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_kem_keypair failed\n", count);
            fclose(fp); return 1;
        }

        ret = crypto_kem_enc(ct, ss_enc, pk);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_kem_enc failed\n", count);
            fclose(fp); return 1;
        }

        ret = crypto_kem_dec(ss_dec, ct, sk);
        if (ret != 0 || memcmp(ss_enc, ss_dec, CRYPTO_BYTES) != 0) {
            fprintf(stderr, "count=%d: decapsulation mismatch\n", count);
            fclose(fp); return 1;
        }

        fprintf(fp, "count = %d\n", count);
        fprintf(fp, "seed = ");
        for (int i = 0; i < 48; i++) fprintf(fp, "%02X", seed[i]);
        fprintf(fp, "\n");
        fprintf(fp, "pk = ");
        for (int i = 0; i < CRYPTO_PUBLICKEYBYTES; i++) fprintf(fp, "%02X", pk[i]);
        fprintf(fp, "\n");
        fprintf(fp, "sk = ");
        for (int i = 0; i < CRYPTO_SECRETKEYBYTES; i++) fprintf(fp, "%02X", sk[i]);
        fprintf(fp, "\n");
        fprintf(fp, "ct = ");
        for (int i = 0; i < CRYPTO_CIPHERTEXTBYTES; i++) fprintf(fp, "%02X", ct[i]);
        fprintf(fp, "\n");
        fprintf(fp, "ss = ");
        for (int i = 0; i < CRYPTO_BYTES; i++) fprintf(fp, "%02X", ss_enc[i]);
        fprintf(fp, "\n\n");

        for (int i = 47; i >= 0; i--) { if (++entropy_input[i]) break; }
    }

    fclose(fp);
    printf("Generated %d KAT vectors -> %s\n", KATNUM, RSP_PATH);
    return 0;
}
