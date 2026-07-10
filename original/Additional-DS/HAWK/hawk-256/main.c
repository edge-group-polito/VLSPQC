///////////////////////////////////////////////////////////////////////////////////////
//                                                                                   //
// Auth: Alessandra Dolmeta, Valeria Piscopo                                         //
// @ EDGE group, at VLSI-LAB, Politecnico di Torino                                  //
// Date: July 2026                                                                   //
// Desc: hawk-256 KAT test entry point                                               //
//                                                                                   //
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api.h"
#include "rng.h"
#include "sha3.h"
#include "test_vectors_256.h"

#ifdef PERF_CNT_CYCLES
    #include "core_v_mini_mcu.h"
    #include "csr.h"
#endif

#ifndef TEST_KEY
#define TEST_KEY       1
#endif
#ifndef TEST_SIGN
#define TEST_SIGN      1
#endif
#ifndef TEST_SIGN_OPEN
#define TEST_SIGN_OPEN 1
#endif

#ifndef NTESTS
#define NTESTS 1
#else
#if (NTESTS > N_TVEC)
#error "NTESTS must be <= N_TVEC"
#endif
#endif

#if PRINT_VECT
void printVect(const char *name, const uint8_t *buf, size_t size) {
    printf("%s = ", name);
    for (size_t i = 0; i < size; i++)
        printf("%02X", buf[i]);
    printf("\n");
}
#endif

int main(void)
{
    unsigned char sk[CRYPTO_SECRETKEYBYTES];
    unsigned char pk[CRYPTO_PUBLICKEYBYTES];

    unsigned char m[MLEN_KAT];
    unsigned char sm[SMLEN_KAT];
    unsigned char m1[SMLEN_KAT];

    unsigned long long smlen, mlen1;

    unsigned cycles_keygen, cycles_sign, cycles_sign_open;
    (void)cycles_keygen; (void)cycles_sign; (void)cycles_sign_open;

    int r;

    printf("Started %d test(s) — " CRYPTO_ALGNAME "\n", NTESTS);

    char test_str[80] = "Testing";
#if TEST_KEY
    strcat(test_str, " keypair");
#endif
#if TEST_SIGN
    strcat(test_str, " sign");
#endif
#if TEST_SIGN_OPEN
    strcat(test_str, " sign_open");
#endif
    printf("%s\n", test_str);

    for (int i = 0; i < NTESTS; i++) {

        printf("Test %d: ", i);

        /*
         * Re-seed the NIST DRBG with the KAT seed.
         * Both keygen and sign draw from this shared DRBG state, exactly
         * as the reference PQCgenKAT_sign.c program does.
         */
        randombytes_init((unsigned char *)TVEC_SEED[i], NULL, 256);

        //*************************************************************
        // KEYGEN
        //*************************************************************
#if TEST_KEY
        #ifdef PERF_CNT_CYCLES
            CSR_WRITE(CSR_REG_MCYCLE, 0);
        #endif

        r = crypto_sign_keypair(pk, sk);
        if (r != 0) { printf("ERROR: crypto_sign_keypair returned %d\n", r); return -1; }

        #ifdef PERF_CNT_CYCLES
            CSR_READ(CSR_REG_MCYCLE, &cycles_keygen);
            printf("Keygen cycles: %u\n", cycles_keygen);
        #endif

        if (memcmp(pk, TVEC_PK[i], CRYPTO_PUBLICKEYBYTES)) { printf("ERROR: PK mismatch\n"); return -1; }
        if (memcmp(sk, TVEC_SK[i], CRYPTO_SECRETKEYBYTES)) { printf("ERROR: SK mismatch\n"); return -1; }

#else /* TEST_KEY == 0: still run keygen to advance the DRBG state */
        r = crypto_sign_keypair(pk, sk);
        if (r != 0) { printf("ERROR: crypto_sign_keypair returned %d\n", r); return -1; }
        /* Use test-vector keys for the sign/verify steps below */
        memcpy(pk, TVEC_PK[i], CRYPTO_PUBLICKEYBYTES);
        memcpy(sk, TVEC_SK[i], CRYPTO_SECRETKEYBYTES);
#endif /* TEST_KEY */

        //*************************************************************
        // SIGN
        //*************************************************************
#if TEST_SIGN
        memcpy(m, TVEC_MSG[i], MLEN_KAT);

        #ifdef PERF_CNT_CYCLES
            CSR_WRITE(CSR_REG_MCYCLE, 0);
        #endif

        r = crypto_sign(sm, &smlen, m, MLEN_KAT, sk);
        if (r != 0) { printf("ERROR: crypto_sign returned %d\n", r); return -1; }

        #ifdef PERF_CNT_CYCLES
            CSR_READ(CSR_REG_MCYCLE, &cycles_sign);
            printf("Sign cycles: %u\n", cycles_sign);
        #endif

        if (smlen != SMLEN_KAT)                               { printf("ERROR: smlen mismatch (%llu != %d)\n", smlen, SMLEN_KAT); return -1; }
        if (memcmp(sm, TVEC_SM[i], SMLEN_KAT))               { printf("ERROR: SM mismatch\n"); return -1; }

#else /* TEST_SIGN == 0: use TV signed message */
        memcpy(sm, TVEC_SM[i], SMLEN_KAT);
        smlen = SMLEN_KAT;
#endif /* TEST_SIGN */

        //*************************************************************
        // SIGN_OPEN
        //*************************************************************
#if TEST_SIGN_OPEN
        #ifdef PERF_CNT_CYCLES
            CSR_WRITE(CSR_REG_MCYCLE, 0);
        #endif

        r = crypto_sign_open(m1, &mlen1, sm, smlen, pk);
        if (r != 0) { printf("ERROR: crypto_sign_open returned %d\n", r); return -1; }

        #ifdef PERF_CNT_CYCLES
            CSR_READ(CSR_REG_MCYCLE, &cycles_sign_open);
            printf("Sign_open cycles: %u\n", cycles_sign_open);
        #endif

        if (mlen1 != MLEN_KAT)                        { printf("ERROR: mlen mismatch (%llu != %d)\n", mlen1, MLEN_KAT); return -1; }
        if (memcmp(m1, TVEC_MSG[i], MLEN_KAT))        { printf("ERROR: recovered message mismatch\n"); return -1; }

#endif /* TEST_SIGN_OPEN */

        printf("OK\n");
    }

    printf("Test Successful\n");
    return 0;
}
