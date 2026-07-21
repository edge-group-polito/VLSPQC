///////////////////////////////////////////////////////////////////////////////////////
//                                                                                   //
// Auth: Alessandra Dolmeta, Valeria Piscopo                                         //
// @ EDGE group, at VLSI-LAB, Politecnico di Torino                                  //
// Date: July 2026                                                                   //
// Desc: SPHINCS+-256f-robust KAT test entry point                                   //
//                                                                                   //
///////////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api.h"

#ifndef APPLI_NAME
#define APPLI_NAME "SPHINCS+"
#endif

#define TEST_KEY 1
#define TEST_SIGN 1
#define TEST_SIGN_OPEN 1

#define MLEN_KAT 33

static uint8_t keypair_rnd[CRYPTO_SEEDBYTES];
static uint8_t signature_rnd[SPX_N];
static uint8_t sk[CRYPTO_SECRETKEYBYTES];
static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
static uint8_t m[MLEN_KAT];
static uint8_t sm[MLEN_KAT + CRYPTO_BYTES];
static uint8_t m1[MLEN_KAT + CRYPTO_BYTES];

static void fill_pattern(uint8_t *buf, size_t len, uint8_t seed)
{
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(seed + (uint8_t)(17U * i));
    }
}

int main(void)
{
    unsigned long long smlen = 0;
    unsigned long long mlen1 = 0;
    int fail = 0;

    printf("Testing %s keypair sign. sign_open.\n", APPLI_NAME);

    fill_pattern(keypair_rnd, sizeof(keypair_rnd), 0x42);
    fill_pattern(signature_rnd, sizeof(signature_rnd), 0xA5);
    fill_pattern(m, sizeof(m), 0x11);

#if TEST_KEY
    if (crypto_sign_keypair(pk, sk, keypair_rnd) != 0) {
        printf("ERROR: keypair failed\n");
        fail = 1;
    }
#endif

#if TEST_SIGN
    if (crypto_sign(sm, &smlen, m, MLEN_KAT, sk, signature_rnd) != 0) {
        printf("ERROR: sign failed\n");
        fail = 1;
    }
    if (smlen != MLEN_KAT + CRYPTO_BYTES) {
        printf("ERROR: signed message length mismatch\n");
        fail = 1;
    }
#endif

#if TEST_SIGN_OPEN
    if (crypto_sign_open(m1, &mlen1, sm, smlen, pk) != 0) {
        printf("ERROR: sign_open failed\n");
        fail = 1;
    }
    if (mlen1 != MLEN_KAT) {
        printf("ERROR: opened message length mismatch\n");
        fail = 1;
    }
    if (memcmp(m1, m, MLEN_KAT) != 0) {
        printf("ERROR: opened message mismatch\n");
        fail = 1;
    }
#endif

    if (fail) {
        return 1;
    }

    printf("OK\n");
    printf("Test Successful\n");

    return 0;
}

