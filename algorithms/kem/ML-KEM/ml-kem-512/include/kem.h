#ifndef KEM_H
#define KEM_H

/* Compatibility layer: maps the harness' crypto_kem_* names onto mlkem-native
 * (FIPS 203), parameter set 512. Only the deterministic API is built
 * (MLK_CONFIG_NO_RANDOMIZED_API), so no randombytes() provider is required. */
#include "mlkem_native.h"
#include "api.h"
#include "params.h"

#define crypto_kem_keypair_derand PQCP_MLKEM_NATIVE_MLKEM512_keypair_derand
#define crypto_kem_enc_derand     PQCP_MLKEM_NATIVE_MLKEM512_enc_derand
#define crypto_kem_dec            PQCP_MLKEM_NATIVE_MLKEM512_dec

#endif
