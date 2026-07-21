///////////////////////////////////////////////////////////////////////////////////////
//                                                                                   //
// Auth: Alessandra Dolmeta, Valeria Piscopo                                         //
// @ EDGE group, at VLSI-LAB, Politecnico di Torino                                  //
// Date: July 2026                                                                   //
// Desc: ml-kem-512 KAT test entry point                                             //
//                                                                                   //
///////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "kem.h"
#include "test_vectors_512.h"


#define TEST_KEY  1
#define TEST_ENC  1
#define TEST_DEC  1


// Global variables
// Those are used for replacing the "randombytes" function.
// In order to test the algorithm in a deterministic manner,
// we load the "randombytes" output with test vectors obtained by running PQCgenKAT.c algorithm
uint8_t keypair_rnd[2*KYBER_SYMBYTES];
uint8_t encaps_rnd[KYBER_SYMBYTES];

void printVect(char* name, uint8_t* buf, size_t size) {
    printf("%s = ", name);
    for (int i=0; i<size; i++){
        printf("%02X", buf[i]);
    }
    printf("\n");
}

uint8_t pk[KYBER_PUBLICKEYBYTES];
uint8_t sk[KYBER_SECRETKEYBYTES];
uint8_t ct[KYBER_CIPHERTEXTBYTES];
uint8_t key_a[KYBER_SSBYTES];
uint8_t key_b[KYBER_SSBYTES];

int main(void)
{

    printf("Started test with size %d\n", 256*KYBER_K);

    char test_str[50] = "Testing";
    #if TEST_KEY
        strcat(test_str," keypair");
    #endif
    #if TEST_ENC
        strcat(test_str," encaps.");
    #endif
    #if TEST_DEC
        strcat(test_str," decaps.");
    #endif
        printf("%s\n", test_str);


    //************************************************* 
    // KEY
    //*************************************************
    #if TEST_KEY
        // Filling coin vector with indcpa_kem_keypair seeds, obtained from the initialization of randombyte with seed
        memcpy(keypair_rnd , TVEC_IN_KEM_KEYPAIR, KYBER_SYMBYTES*2);

        crypto_kem_keypair_derand(pk, sk, keypair_rnd);

        if(memcmp(pk, TVEC_OUT_PK, KYBER_PUBLICKEYBYTES)) { printf("ERROR: PK mismatch\n");}
        if(memcmp(sk, TVEC_OUT_SK, KYBER_SECRETKEYBYTES)) { printf("ERROR: SK mismatch\n");}
    #endif /* TEST_KEY */

    //************************************************* 
    // ENCAPSULATION
    //*************************************************
    #if TEST_ENC
        // Setting up Input test vectors
        memcpy(encaps_rnd, TVEC_IN_KEM_ENC, KYBER_SYMBYTES);

        #if TEST_KEY == 0
            memcpy(pk, TVEC_OUT_PK, KYBER_PUBLICKEYBYTES);
        #endif /* TEST_KEY */

        crypto_kem_enc_derand(ct, key_b, pk, encaps_rnd);

        if(memcmp(ct, TVEC_OUT_CT, KYBER_CIPHERTEXTBYTES)) { printf("ERROR: CT mismatch\n");}
        if(memcmp(key_b, TVEC_OUT_SS, KYBER_SSBYTES)) { printf("ERROR: SS mismatch\n");}
    #endif /* TEST_ENC */

    //************************************************* 
    // DECAPSULATION
    //*************************************************

    #if TEST_DEC
        #if TEST_KEY == 0
            // In case we do not generate SK in this test we take it from the test vectors
            memcpy(sk, TVEC_OUT_SK, KYBER_SECRETKEYBYTES);
        #endif /* TEST_KEY */
        #if TEST_ENC == 0
            // In case we do not generate CT in this test we take it from the test vectors
            memcpy(ct, TVEC_OUT_CT, KYBER_CIPHERTEXTBYTES);
        #endif /* TEST_ENC */

        crypto_kem_dec(key_a, ct, sk);

        if(memcmp(key_a, TVEC_OUT_SS, KYBER_SSBYTES)) { printf("ERROR: SS mismatch\n");}
    #endif /* TEST_DEC */

    printf("OK\n");


    printVect("key_b", key_b, KYBER_SSBYTES);
    printVect("key_a", key_a, KYBER_SSBYTES);
    printVect("key_r", TVEC_OUT_SS, KYBER_SSBYTES);
    printf("Test Successful\n");
    

    return 0;
}

