///////////////////////////////////////////////////////////////////////////////////////
//                                                                                   //
// Auth: Alessandra Dolmeta, Valeria Piscopo                                         //
// @ EDGE group, at VLSI-LAB, Politecnico di Torino                                  //
// Date: July 2026                                                                   //
// Desc: LESS_252_45 KAT test entry point                                            //
//                                                                                   //
///////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "api.h"
#include "parameters.h"
#include "rng.h"

#include "LESS_KAT_252_45.h" // Test Vectors



#define RSDP
#define MLEN_KAT 33

#define TEST_KEY  1
#define TEST_SIGN 1
#define TEST_SIGN_OPEN  1

#if PRINT_VECT
void printVect(char* name, uint8_t* buf, size_t size) {
    printf("%s = ", name);
    for (int i=0; i<size; i++){
        printf("%02X", buf[i]);
    }
    printf("\n");
}
#endif

unsigned char sk[CRYPTO_SECRETKEYBYTES];
unsigned char pk[CRYPTO_PUBLICKEYBYTES];
unsigned long long  mlen, smlen, mlen1;

int main(void)
{
    unsigned char       *m, *sm, *m1;


    mlen = MLEN_KAT;
    m = (unsigned char *)calloc(mlen, sizeof(unsigned char));
    m1 = (unsigned char *)calloc(mlen+CRYPTO_BYTES, sizeof(unsigned char));
    sm = (unsigned char *)calloc(mlen+CRYPTO_BYTES, sizeof(unsigned char));

    int r;
    size_t i, k;
  
    char test_str[50] = "Testing";
    #if TEST_KEY
        strcat(test_str," keypair");
    #endif
    #if TEST_SIGN
        strcat(test_str," sign.");
    #endif
    #if TEST_SIGN_OPEN
        strcat(test_str," sign_open.");
    #endif
        printf("%s\n", test_str);


    initialize_csprng(&platform_csprng_state, (const unsigned char *)seed, 48);
    //************************************************* 
    // KEY
    //*************************************************

    #if TEST_KEY
        crypto_sign_keypair(pk, sk);
        
        if(memcmp(pk, TVEC_OUT_PK, CRYPTO_PUBLICKEYBYTES)) { printf("ERROR: PK mismatch\n"); }
        if(memcmp(sk, TVEC_OUT_SK, CRYPTO_SECRETKEYBYTES)) { printf("ERROR: SK mismatch\n"); }

    #endif /* TEST_KEY */

    //************************************************* 
    // SIGN
    //*************************************************
    #if TEST_SIGN
        // Setting up Input test vectors
        memcpy(m, TVEC_IN_M_SIGN, MLEN_KAT);

        #if TEST_KEY == 0 
            uint8_t throwaway[PRIVATE_KEY_SEED_LENGTH_BYTES];
            randombytes(throwaway, PRIVATE_KEY_SEED_LENGTH_BYTES);
            memcpy(sk, TVEC_OUT_SK, CRYPTO_SECRETKEYBYTES);
        #endif /* TEST_KEY */
        
        crypto_sign(sm, &smlen, m, MLEN_KAT, sk);
        
        if(memcmp(sm, TVEC_IN_SM_SIGN, 1346)) { printf("ERROR: SM mismatch\n"); }
        
    #endif /* TEST_SIGN */
    
    //************************************************* 
    // SIGN_OPEN
    //*************************************************
    #if TEST_SIGN_OPEN

        #if TEST_KEY== 0 
          memcpy(pk, TVEC_OUT_PK, CRYPTO_PUBLICKEYBYTES);
        #endif /* TEST_KEY */
        #if TEST_SIGN == 0 
          memcpy(sm, TVEC_IN_SM_SIGN, 1346);
        #endif /* TEST_SIGN */
        
        crypto_sign_open(m1, &mlen1, sm, 1346, pk);
        
        if(memcmp(m1, TVEC_IN_M_SIGN, MLEN_KAT)) { printf("ERROR: M mismatch\n"); }
        printf("m1: ");
        for (int i = 0; i < MLEN_KAT; i++) {
          printf("%02x", m1[i]);
        }
        printf("\n");

    #endif /* TEST_SIGN_OPEN */


    printf("Test Successful\n");

    return 0;


}
