///////////////////////////////////////////////////////////////////////////////////////
//                                                                                   //
// Auth: Alessandra Dolmeta, Valeria Piscopo                                         //
// @ EDGE group, at VLSI-LAB, Politecnico di Torino                                  //
// Date: July 2026                                                                   //
// Desc: CROSS_5_RSDP_f KAT test entry point                                         //
//                                                                                   //
///////////////////////////////////////////////////////////////////////////////////////

/**
 *
 * Reference ISO-C11 Implementation of CROSS.
 *
 * @version 2.0 (February 2025)
 *
 * Authors listed in alphabetical order:
 * 
 * @author: Alessandro Barenghi <alessandro.barenghi@polimi.it>
 * @author: Marco Gianvecchio <marco.gianvecchio@mail.polimi.it>
 * @author: Patrick Karl <patrick.karl@tum.de>
 * @author: Gerardo Pelosi <gerardo.pelosi@polimi.it>
 * @author: Jonas Schupp <jonas.schupp@tum.de>
 * 
 * 
 * This code is hereby placed in the public domain.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "csprng_hash.h"
#include "CROSS.h"
#include "api.h"
#include "fp_arith.h"
#include "parameters.h"
#include "CROSS_KAT_5_RSDP_f.h"

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

int main(void)
{

    unsigned char sk[CRYPTO_SECRETKEYBYTES];
    unsigned char pk[CRYPTO_PUBLICKEYBYTES];
    unsigned long long  mlen, smlen, mlen1;
    unsigned char       *m, *sm, *m1;

    unsigned char       entropy_input[48] = {0};
    uint8_t seed_prng[48];

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


    for (int i=0; i<48; i++) {
        entropy_input[i] = i;
    }
    csprng_initialize(&platform_csprng_state, (const unsigned char *)entropy_input, 48, CSPRNG_DOMAIN_SEP_CONST);

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
        memcpy(m, TVEC_IN_M_SIGN, MLEN_KAT);

        #if TEST_KEY == 0 
            uint8_t throwaway[KEYPAIR_SEED_LENGTH_BYTES];
            randombytes(throwaway, KEYPAIR_SEED_LENGTH_BYTES);
            memcpy(sk, TVEC_OUT_SK, CRYPTO_SECRETKEYBYTES);
        #endif /* TEST_KEY */
      
        crypto_sign(sm, &smlen, m, MLEN_KAT, sk);
        
        if(memcmp(sm, TVEC_IN_SM_SIGN, 74623)) { printf("ERROR: SM mismatch\n"); }


    #endif /* TEST_SIGN */
    
    //************************************************* 
    // IGN_OPEN
    //*************************************************
    #if TEST_SIGN_OPEN

        #if TEST_KEY== 0 
          memcpy(pk, TVEC_OUT_PK, CRYPTO_PUBLICKEYBYTES);
        #endif /* TEST_KEY */
        #if TEST_SIGN == 0 
          memcpy(sm, TVEC_IN_SM_SIGN, 74623);
        #endif /* TEST_SIGN */

        crypto_sign_open(m1, &mlen1, sm, 74623, pk);
        
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
