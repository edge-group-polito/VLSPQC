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

#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "csprng_hash.h"
#include "parameters.h"
#include "restr_arith.h"

#define NUM_BITS_P (BITS_TO_REPRESENT(P))


#define FPRED_SINGLE(x) (((x) & 0x7F) + ((x) >> 7))
#define FPRED_DOUBLE(x) FPRED_SINGLE(FPRED_SINGLE(x))
//#define FPRED_DOUBLE(x) ((((x) & 0x7F) + ((x) >> 7)) & 0x7F) + ((((x) & 0x7F) + ((x) >> 7)) >> 7)
#define FPRED_OPPOSITE(x) ((x) ^ 0x7F)
#define FP_DOUBLE_ZERO_NORM(x) (((x) + (((x) + 1) >> 7)) & 0x7F)
#define RESTR_TO_VAL(x) ( (FP_ELEM) (RESTR_G_TABLE >> (8*(uint64_t)(x))) )



/* in-place normalization of redundant zero representation for syndromes*/
static inline
void fp_dz_norm_synd(FP_ELEM v[N-K]){
    //printf("==== Start FP_DOUBLE_ZERO_NORM ====\n");
    for (int i = 0; i < N-K; i++){  
       //printf("v_in[%d] = %u\n", i, v[i]);
       v[i] = FP_DOUBLE_ZERO_NORM(v[i]);
       //printf("v_out[%d] = %u\n\n", i, v[i]);
    }
}

static inline
void fp_dz_norm(FP_ELEM v[N]){
    //printf("==== Start FP_DOUBLE_ZERO_NORM ====\n");
    for (int i = 0; i < N; i++){
       //printf("v_in[%d] = %u\n", i, v[i]);
       v[i] = FP_DOUBLE_ZERO_NORM(v[i]);
       //printf("v_out[%d] = %u\n\n", i, v[i]);
    }
}
/* Computes the product e*H of an n-element restricted vector by a (n-k)*n
 * FP H is in systematic form. Only the non systematic portion of H =[V I],
 * V, is provided, transposed, hence linearized by columns so that syndrome
 * computation is vectorizable. */

static
void restr_vec_by_fp_matrix(FP_ELEM res[N-K],
                            FZ_ELEM e[N],
                            FP_ELEM V_tr[K][N-K])
{

    //printf("==== Start restr_vec_by_fp_matrix ====\n");
    //printf("Params: N=%d, K=%d\n", (int)N, (int)K);
    //printf("Input vector e[%d]: ", (int)N);
    //for (int idx = 0; idx < N; idx++) {
    //    printf("%u ", (unsigned)e[idx]);
    //}
    //printf("\n");
    //printf("Matrix V_tr[%d][%d]:\n", (int)K, (int)(N-K));
    //for (int i = 0; i < K; i++) {
    //    printf("  Row %d: ", i);
    //    for (int j = 0; j < N-K; j++) {
    //        printf("%u ", (unsigned)V_tr[i][j]);
    //    }
    //    printf("\n");
    //}


    for (int i = K; i < N; i++) {
        FP_ELEM r = (FP_ELEM)RESTR_TO_VAL(e[i]);
        res[i-K] = r;
    }

    for (int i = 0; i < K; i++) {
        //printf("Processing i = %d\n", i);
        //printf("e[%d] = %u\n\n", i, e[i]);
        FP_ELEM restr_i = (FP_ELEM)RESTR_TO_VAL(e[i]);

        for (int j = 0; j < N-K; j++) {
            FP_DOUBLEPREC mul     = (FP_DOUBLEPREC)restr_i * (FP_DOUBLEPREC)V_tr[i][j];
            FP_DOUBLEPREC sum     = (FP_DOUBLEPREC)res[j] + mul;
            FP_DOUBLEPREC reduced = FPRED_DOUBLE(sum);
            
            //printf("V_tr[%d][%d] = %u\n", i, j, V_tr[i][j]);
            //printf("res[%d] = %u\n", j, res[j]);
            //printf("mul = %u\n", mul);
            //printf("sum = %u\n", sum);
            //printf("→ RED = %u\n", reduced);

            res[j] = (FP_ELEM)reduced;
        }
    }

}


//static
//void fp_vec_by_fp_matrix(FP_ELEM res[N-K],
//                         FP_ELEM e[N],
//                         FP_ELEM V_tr[K][N-K]){
//    memcpy(res,e+K,(N-K)*sizeof(FP_ELEM));
//    for(int i = 0; i < K; i++){
//       for(int j = 0; j < N-K; j++){
//           res[j] = FPRED_DOUBLE( (FP_DOUBLEPREC) res[j] +
//                                  (FP_DOUBLEPREC) e[i] *
//                                  (FP_DOUBLEPREC) V_tr[i][j]);
//       }
//    }
//}
static
void fp_vec_by_fp_matrix(FP_ELEM res[N-K],
                         FP_ELEM e[N],
                         FP_ELEM V_tr[K][N-K]){
    //printf("==== Start fp_vec_by_fp_matrix ====\n");

    // Step 1: Initialize res = e[K .. N-1]
    //printf("Copying e[%d..%d] into res[0..%d]\n", K, N-1, N-K-1);
    memcpy(res, e+K, (N-K)*sizeof(FP_ELEM));

    // Print e vector
    //printf("Input vector e[%d]: ", N);
    //for (int idx = 0; idx < N; idx++) {
    //    printf("%u ", e[idx]);
    //}
    //printf("\n");

    // Print V_tr matrix
    //printf("Matrix V_tr[%d][%d]:\n", K, N-K);
    //for (int i = 0; i < K; i++) {
    //    printf("Row %d: ", i);
    //    for (int j = 0; j < N-K; j++) {
    //        printf("%u ", V_tr[i][j]);
    //    }
    //    printf("\n");
    //}

    // Print initial res
    //printf("Initial res[%d]: ", N-K);
    //for (int j = 0; j < N-K; j++) {
    //    printf("%u ", res[j]);
    //}
    //printf("\n");

    // Step 2: Matrix-vector product with reduction
    for (int i = 0; i < K; i++) {
        //printf("Processing i = %d\n", i);
        //printf("e[%d] = %u\n\n", i, e[i]);

        for (int j = 0; j < N-K; j++) {

            //printf("V_tr[%d][%d] = %u\n", i, j, V_tr[i][j]);
            //printf("res[%d] = %u\n", j, res[j]);

            FP_DOUBLEPREC mul = (FP_DOUBLEPREC)e[i] * (FP_DOUBLEPREC)V_tr[i][j];
            FP_DOUBLEPREC sum = (FP_DOUBLEPREC)res[j] + mul;
            FP_DOUBLEPREC reduced = FPRED_DOUBLE(sum);

            //printf("mul = %u\n", mul);
            //printf("sum = %u\n", sum);
            //printf("→ RED = %u\n", reduced);

            //printf("j = %d: res[%d] = %u + (%u * %u = %u) = %u → RED = %u\n",
            //       j, j, res[j], e[i], V_tr[i][j], mul, sum, reduced);

            res[j] = reduced;
        }
        //printf("\n\n\n");
    }

    // Final result
    //printf("Final res: ");
    //for (int j = 0; j < N-K; j++) {
    //    printf("%u ", res[j]);
    //}
    //printf("\n==== End fp_vec_by_fp_matrix ====\n");
}

static inline
void fp_vec_by_fp_vec_pointwise(FP_ELEM res[N],
                                const FP_ELEM in1[N],
                                const FP_ELEM in2[N]){
    //printf("==== Start fp_vec_by_fp_vec_pointwise ====\n");
    for(int i = 0; i < N; i++){
        //printf("in1[%d] = %u\n", i, in1[i]);
        //printf("in2[%d] = %u\n", i, in2[i]);
        res[i] = FPRED_DOUBLE( (FP_DOUBLEPREC) in1[i] *
                               (FP_DOUBLEPREC) in2[i] );
        //printf("res[%d] = %u\n\n", i, res[i]);
    }
}


static inline
void restr_by_fp_vec_pointwise(FP_ELEM res[N],
                                const FZ_ELEM in1[N],
                                const FP_ELEM in2[N]){
    //printf("==== Start restr_by_fp_vec_pointwise ====\n");
    for(int i = 0; i < N; i++){
        //printf("in1[%d] = %u\n", i, in1[i]);
        //printf("in2[%d] = %u\n", i, in2[i]);
        
        res[i] = FPRED_DOUBLE( (FP_DOUBLEPREC) RESTR_TO_VAL(in1[i]) *
                               (FP_DOUBLEPREC) in2[i]);
        //printf("res[%d] = %u\n\n", i, res[i]);
    }
}

/* e*chall_1 + u_prime*/
static inline
void fp_vec_by_restr_vec_scaled(FP_ELEM res[N],
                                const FZ_ELEM e[N],
                                const FP_ELEM chall_1,
                                const FP_ELEM u_prime[N]){
    //printf("==== Start fp_vec_by_restr_vec_scaled ====\n");
    for(int i = 0; i < N; i++){
        //printf("u_prime[%d] = %u\n", i, u_prime[i]);
        //printf("e[%d] = %u\n", i, e[i]);
        //printf("chall_1[%d] = %u\n", i, chall_1);
        res[i] = FPRED_DOUBLE( (FP_DOUBLEPREC) u_prime[i] +
                               (FP_DOUBLEPREC) RESTR_TO_VAL(e[i]) * (FP_DOUBLEPREC) chall_1) ;
        
        //FP_DOUBLEPREC mul = (FP_DOUBLEPREC)RESTR_TO_VAL(e[i]) * (FP_DOUBLEPREC)chall_1;
        //FP_DOUBLEPREC sum = (FP_DOUBLEPREC)u_prime[i] + mul;
        //FP_DOUBLEPREC reduced = FPRED_DOUBLE(sum);
        //printf("mul = %u\n", mul);
        //printf("sum = %u\n", sum);
        //printf("→ RED = %u\n", reduced);
        //printf("res[%d] = %u\n\n", i, res[i]);
    }
}


static inline
void fp_synd_minus_fp_vec_scaled(FP_ELEM res[N-K],
                                 const FP_ELEM synd[N-K],
                                 const FP_ELEM chall_1,
                                 const FP_ELEM s[N-K]){
    //printf("==== Start fp_synd_minus_fp_vec_scaled ====\n");

    for(int j = 0; j < N-K; j++){
        //printf("s[%d] = %u\n", j, s[j]);
        //printf("synd[%d] = %u\n", j, synd[j]);
        //printf("chall_1 = %u\n", chall_1);

        FP_ELEM tmp = FPRED_DOUBLE( (FP_DOUBLEPREC) s[j] * (FP_DOUBLEPREC) chall_1);
        //printf("tmp = %u\n", tmp);

        tmp = FP_DOUBLE_ZERO_NORM(tmp);
        //printf("norm = %u\n", tmp);
        tmp = FPRED_OPPOSITE(tmp);
        //printf("opposite = %u\n", tmp);
        res[j] = FPRED_SINGLE( (FP_DOUBLEPREC) synd[j] + tmp );
        //printf("res[%d] = %u\n\n", j, res[j]);
    }
}

static inline
void convert_restr_vec_to_fp(FP_ELEM res[N],
                            const FZ_ELEM in[N]){
    //printf("==== Start convert_restr_vec_to_fp ====\n");

    for(int j = 0; j < N; j++){
        //printf("input = %u\n", in[j]);
        res[j] = RESTR_TO_VAL(in[j]);
        //printf("output = %u\n", res[j]);
    }
}
