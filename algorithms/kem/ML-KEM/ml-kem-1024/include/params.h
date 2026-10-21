#ifndef PARAMS_H
#define PARAMS_H

#include "mlkem_native.h"

/* Names kept from the pre-FIPS Kyber layout, used by the harness/gen mlkem sources */
#define KYBER_SYMBYTES        MLKEM_SYMBYTES
#define KYBER_SSBYTES         MLKEM1024_BYTES
#define KYBER_PUBLICKEYBYTES  MLKEM1024_PUBLICKEYBYTES
#define KYBER_SECRETKEYBYTES  MLKEM1024_SECRETKEYBYTES
#define KYBER_CIPHERTEXTBYTES MLKEM1024_CIPHERTEXTBYTES

#endif
