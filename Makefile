#######################################################################################
##                                                                                   ##
## Auth: Alessandra Dolmeta, Valeria Piscopo                                         ##
## @ EDGE group, at VLSI-LAB, Politecnico di Torino                                  ##
## Date: July 2026                                                                   ##
## Desc: Top-level build system for the PQC KAT test suite                           ##
##                                                                                   ##
#######################################################################################

# =============================================================================
#  PQC KAT Test Suite — new-original/Makefile
# =============================================================================
#
#  QUICK START
#    make gen-all                  Generate .rsp files for the standardized set
#    make test-all                 Run KAT tests for the standardized set
#                                   (ML-KEM, ML-DSA, SLH-DSA x4, FALCON, HQC)
#    make test-additional-ds       Test CROSS + HAWK + LESS + FAEST + UOV + MAYO + QRUOV + SNOVA + SDITH + SQISIGN (not standardized)
#    make test-stateful-ds         Test XMSS + LMS + HSS (NIST SP 800-208)
#    make test-ascon               Test ASCON AEAD + hash/XOF (not PQC)
#    make test-cross               Test all CROSS variants
#    make test-CROSS_1_RSDP_b     Test one specific variant
#    make gen-CROSS_1_RSDP_b      Generate .rsp for one variant
#
#  CONFIGURABLE FLAGS (override on command line, e.g. make test-all KATNUM=10)
#
#    KATNUM    = 100    Number of KAT vectors to generate/test
#    RUN_KEYGEN  = 1    Enable keygen test    (1=yes, 0=no)
#    RUN_SIGN    = 1    Enable sign test      (DS only; 1=yes, 0=no)
#    RUN_VERIFY  = 1    Enable verify test    (DS only; 1=yes, 0=no)
#    RUN_ENCAPS  = 1    Enable encaps test    (KEM only, and ASCON's encrypt; 1=yes, 0=no)
#    RUN_DECAPS  = 1    Enable decaps test    (KEM only, and ASCON's decrypt; 1=yes, 0=no)
#    DEBUG     = 0    Verbose hex output    (1=yes)
#    MLEN      = 33   Message length for generation
#    CC        = gcc
#    CFLAGS    = -O2 -std=c11 -Wall -Wno-unused-function -Wno-unused-variable
#
#  FAMILY TARGETS
#    Standardized (in test-all): test-falcon test-mldsa test-slhdsa
#      test-slhdsa-haraka test-slhdsa-haraka-aes test-slhdsa-sha2 test-mlkem
#      test-hqc  (aggregates: test-ds, test-kem)
#    Additional-DS (NOT in test-all): test-cross test-hawk test-less test-faest test-uov test-mayo test-qruov test-snova test-sdith test-sqisign
#      (aggregate: test-additional-ds)
#    Stateful-DS (NOT in test-all): test-xmss test-lms test-hss
#      (aggregate: test-stateful-ds)
#    ASCON (NOT in test-all, not PQC): test-ascon-aead test-ascon-hash
#      (aggregate: test-ascon)
#
#    Corresponding gen-* targets exist for each of the above.
#
# =============================================================================

CC        ?= gcc
CFLAGS    ?= -O2 -std=c11 -Wall -Wno-unused-function -Wno-unused-variable

KATNUM    ?= 100
RUN_KEYGEN  ?= 1
RUN_SIGN    ?= 1
RUN_VERIFY  ?= 1
RUN_ENCAPS  ?= 1
RUN_DECAPS  ?= 1
DEBUG     ?= 0
# KAT vectors per variant for the *-check (full parameter-set sweep) targets
# (LMS/HSS/XMSS). Kept low by default since these multiply out over dozens
# of variants; override per-invocation, e.g. `make test-xmss-check CHECK_KATNUM=3`.
CHECK_KATNUM ?= 1
MLEN      ?= 33

ROOT    := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
GENDIR  := $(ROOT)/kat/gen
RSPDIR  := $(ROOT)/kat/rsp
COMMON  := $(ROOT)/common
BUILD   := $(ROOT)/build

# Flags passed to test binaries
SIGN_TFLAGS := \
    -DKATNUM=$(KATNUM) -DDEBUG=$(DEBUG) \
    -DRUN_KEYGEN=$(RUN_KEYGEN) -DRUN_SIGN=$(RUN_SIGN) -DRUN_VERIFY=$(RUN_VERIFY)

KEM_TFLAGS := \
    -DKATNUM=$(KATNUM) -DDEBUG=$(DEBUG) \
    -DRUN_KEYGEN=$(RUN_KEYGEN) -DRUN_ENCAPS=$(RUN_ENCAPS) -DRUN_DECAPS=$(RUN_DECAPS)

# ASCON has no keygen -- RUN_ENCAPS/RUN_DECAPS double as its encrypt/decrypt
# toggles (AEAD variants only; hash/XOF variants ignore both, single-stage).
ASCON_AEAD_TFLAGS := \
    -DKATNUM=$(KATNUM) -DDEBUG=$(DEBUG) \
    -DRUN_ENCAPS=$(RUN_ENCAPS) -DRUN_DECAPS=$(RUN_DECAPS)

ASCON_HASH_TFLAGS := \
    -DKATNUM=$(KATNUM) -DDEBUG=$(DEBUG)

# Flags for KAT generators
GEN_FLAGS := -DKATNUM=$(KATNUM) -DMLEN=$(MLEN)

# =============================================================================
#  Directory creation
# =============================================================================

DIRS := $(BUILD) \
        $(RSPDIR)/DS/FALCON \
        $(RSPDIR)/DS/ML-DSA \
        $(RSPDIR)/DS/SLH-DSA \
        $(RSPDIR)/DS/SLH-DSA-Haraka \
        $(RSPDIR)/DS/SLH-DSA-Haraka-AES \
        $(RSPDIR)/DS/SLH-DSA-SHA2 \
        $(RSPDIR)/Additional-DS/CROSS \
        $(RSPDIR)/Additional-DS/FAEST \
        $(RSPDIR)/Additional-DS/FAEST-decoupled \
        $(RSPDIR)/Additional-DS/HAWK \
        $(RSPDIR)/Additional-DS/LESS \
        $(RSPDIR)/Additional-DS/UOV \
        $(RSPDIR)/Additional-DS/MAYO \
        $(RSPDIR)/Additional-DS/QRUOV \
        $(RSPDIR)/Additional-DS/SNOVA \
        $(RSPDIR)/Additional-DS/SDITH \
        $(RSPDIR)/Additional-DS/SQISIGN \
        $(RSPDIR)/Stateful-DS/XMSS \
        $(RSPDIR)/Stateful-DS/LMS \
        $(RSPDIR)/Stateful-DS/HSS \
        $(RSPDIR)/KEM/ML-KEM \
        $(RSPDIR)/KEM/HQC/HQC-2025 \
        $(RSPDIR)/Symmetric/ASCON

$(DIRS):
	mkdir -p $@

# =============================================================================
#  CROSS  (18 variants – src/ + include/ layout)
#         No kat/gen/rng.c: CROSS defines its own randombytes().
# =============================================================================

CROSS_VARIANTS := \
    CROSS_1_RSDP_b  CROSS_1_RSDP_f  CROSS_1_RSDP_s \
    CROSS_1_RSDPG_b CROSS_1_RSDPG_f CROSS_1_RSDPG_s \
    CROSS_3_RSDP_b  CROSS_3_RSDP_f  CROSS_3_RSDP_s \
    CROSS_3_RSDPG_b CROSS_3_RSDPG_f CROSS_3_RSDPG_s \
    CROSS_5_RSDP_b  CROSS_5_RSDP_f  CROSS_5_RSDP_s \
    CROSS_5_RSDPG_b CROSS_5_RSDPG_f CROSS_5_RSDPG_s

define CROSS_RULES
$(1)_SRCS := $$(wildcard $(ROOT)/original/Additional-DS/CROSS/$(1)/src/*.c)
$(1)_INC  := -I$(ROOT)/original/Additional-DS/CROSS/$(1)/include -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/Additional-DS/CROSS/$(1).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) $(GENDIR)/PQCgenKAT_sign_cross.c $(COMMON)/kat_parser.c \
                  | $(BUILD) $(RSPDIR)/Additional-DS/CROSS
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/Additional-DS/CROSS
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) $(GENDIR)/main_cross.c $(COMMON)/kat_parser.c \
                   | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(CROSS_VARIANTS),$(eval $(call CROSS_RULES,$(v))))

test-cross: $(addprefix test-,$(CROSS_VARIANTS))
gen-cross:  $(addprefix gen-,$(CROSS_VARIANTS))
.PHONY: test-cross gen-cross

# =============================================================================
#  FALCON  (2 variants – src/ + include/ layout)
# =============================================================================

FALCON_VARIANTS := falcon-512 falcon-1024

define FALCON_RULES
$(subst -,_,$(1))_SRCS := $$(wildcard $(ROOT)/original/DS/FALCON/$(1)/src/*.c)
$(subst -,_,$(1))_INC  := -I$(ROOT)/original/DS/FALCON/$(1)/include -I$(GENDIR) -I$(COMMON)
$(subst -,_,$(1))_RSP  := $(RSPDIR)/DS/FALCON/$(1).rsp

$(BUILD)/gen_$(subst -,_,$(1)): $$($(subst -,_,$(1))_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_falcon.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/DS/FALCON
	$(CC) $(CFLAGS) $$($(subst -,_,$(1))_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(subst -,_,$(1))_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(subst -,_,$(1))_RSP): $(BUILD)/gen_$(subst -,_,$(1)) | $(RSPDIR)/DS/FALCON
	$$<

$(BUILD)/test_$(subst -,_,$(1)): $$($(subst -,_,$(1))_SRCS) \
    $(GENDIR)/main_falcon.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(subst -,_,$(1))_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(subst -,_,$(1))_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(subst -,_,$(1))_RSP)
test-$(1): $$($(subst -,_,$(1))_RSP) $(BUILD)/test_$(subst -,_,$(1))
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(subst -,_,$(1))

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(FALCON_VARIANTS),$(eval $(call FALCON_RULES,$(v))))

test-falcon: $(addprefix test-,$(FALCON_VARIANTS))
gen-falcon:  $(addprefix gen-,$(FALCON_VARIANTS))
.PHONY: test-falcon gen-falcon

# =============================================================================
#  HAWK  (3 variants – src/ + include/ layout)
#        HAWK src/rng.c IS the NIST DRBG – do NOT add kat/gen/rng.c.
# =============================================================================

HAWK_VARIANTS := hawk-256 hawk-512 hawk-1024

define HAWK_RULES
$(subst -,_,$(1))_SRCS := $$(wildcard $(ROOT)/original/Additional-DS/HAWK/$(1)/src/*.c)
$(subst -,_,$(1))_INC  := -I$(ROOT)/original/Additional-DS/HAWK/$(1)/include -I$(GENDIR) -I$(COMMON)
$(subst -,_,$(1))_RSP  := $(RSPDIR)/Additional-DS/HAWK/$(1).rsp

$(BUILD)/gen_$(subst -,_,$(1)): $$($(subst -,_,$(1))_SRCS) \
    $(GENDIR)/PQCgenKAT_sign_hawk.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Additional-DS/HAWK
	$(CC) $(CFLAGS) $$($(subst -,_,$(1))_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(subst -,_,$(1))_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(subst -,_,$(1))_RSP): $(BUILD)/gen_$(subst -,_,$(1)) | $(RSPDIR)/Additional-DS/HAWK
	$$<

$(BUILD)/test_$(subst -,_,$(1)): $$($(subst -,_,$(1))_SRCS) \
    $(GENDIR)/main_hawk.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(subst -,_,$(1))_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(subst -,_,$(1))_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(subst -,_,$(1))_RSP)
test-$(1): $$($(subst -,_,$(1))_RSP) $(BUILD)/test_$(subst -,_,$(1))
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(subst -,_,$(1))

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(HAWK_VARIANTS),$(eval $(call HAWK_RULES,$(v))))

test-hawk: $(addprefix test-,$(HAWK_VARIANTS))
gen-hawk:  $(addprefix gen-,$(HAWK_VARIANTS))
.PHONY: test-hawk gen-hawk

# =============================================================================
#  LESS  (7 variants – src/ + include/ layout)
#        LESS randombytes is static-inline; kat/gen/rng.c provides NIST DRBG.
# =============================================================================

LESS_VARIANTS := LESS_252_45 LESS_252_68 LESS_252_192 \
                 LESS_400_102 LESS_400_220 \
                 LESS_548_137 LESS_548_345

define LESS_RULES
$(1)_SRCS := $$(wildcard $(ROOT)/original/Additional-DS/LESS/$(1)/src/*.c)
$(1)_INC  := -I$(ROOT)/original/Additional-DS/LESS/$(1)/include -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/Additional-DS/LESS/$(1).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_less.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Additional-DS/LESS
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/Additional-DS/LESS
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) $(GENDIR)/main_less.c $(COMMON)/kat_parser.c \
                   | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(LESS_VARIANTS),$(eval $(call LESS_RULES,$(v))))

test-less: $(addprefix test-,$(LESS_VARIANTS))
gen-less:  $(addprefix gen-,$(LESS_VARIANTS))
.PHONY: test-less gen-less

# =============================================================================
#  FAEST  (12 variants – src/ + include/ layout, plus a shared common/)
#         Unlike CROSS/LESS/HAWK, the ~30 files identical across all 12
#         variants (generic engine, sha3, the 3 lambda AES/witness files,
#         parameters.h, the NIST DRBG) live once in FAEST/common/{src,include}
#         instead of being duplicated per variant -- each $(1)_SRCS/$(1)_INC
#         below pulls from both common/ and the variant's own dir. No
#         internal CSPRNG object (unlike CROSS/LESS): crypto_sign_keypair/
#         crypto_sign draw randomness via rand_bytes(), patched (see
#         tools/assemble_faest.sh) to always forward to common/'s DRBG
#         randombytes() -- upstream only does this when built with
#         -DHAVE_RANDOMBYTES, but the static-test folder is meant to compile
#         standalone with a plain `gcc *.c`, no special -D flags. Same
#         "reseed global DRBG once per count" shape as HAWK, so the
#         RUN_KEYGEN=0 desync caveat applies here too (see tools/README.md).
#         Every source file has "#define _GNU_SOURCE" prepended (see the
#         assembly script) so mempcpy()/explicit_bzero() get properly
#         declared -- compat.h auto-detects them as available on glibc and
#         wires faest_mempcpy/faest_explicit_bzero straight to them, but
#         without _GNU_SOURCE that's an implicit (int-returning) declaration
#         that corrupts pointers on 64-bit. The vendored sha3/ (XKCP)
#         subtree is flattened into src/+include/ (no subdirs), like every
#         other family here.
# =============================================================================

FAEST_COMMON := $(ROOT)/original/Additional-DS/FAEST/common
FAEST_VARIANTS := FAEST_128F FAEST_128S FAEST_192F FAEST_192S FAEST_256F FAEST_256S \
                   FAEST_EM_128F FAEST_EM_128S FAEST_EM_192F FAEST_EM_192S \
                   FAEST_EM_256F FAEST_EM_256S

define FAEST_RULES
$(1)_SRCS := $$(wildcard $(FAEST_COMMON)/src/*.c) \
             $$(wildcard $(ROOT)/original/Additional-DS/FAEST/$(1)/src/*.c)
$(1)_INC  := -I$(FAEST_COMMON)/include -I$(ROOT)/original/Additional-DS/FAEST/$(1)/include \
             -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/Additional-DS/FAEST/$(1).rsp
$(1)_DEFS := -Wno-psabi

$(BUILD)/gen_$(1): $$($(1)_SRCS) \
    $(GENDIR)/PQCgenKAT_sign_faest.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Additional-DS/FAEST
	$(CC) $(CFLAGS) $$($(1)_DEFS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/Additional-DS/FAEST
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) $(GENDIR)/main_faest.c \
    $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_DEFS) $$($(1)_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(FAEST_VARIANTS),$(eval $(call FAEST_RULES,$(v))))

test-faest: $(addprefix test-,$(FAEST_VARIANTS))
gen-faest:  $(addprefix gen-,$(FAEST_VARIANTS))
.PHONY: test-faest gen-faest

# =============================================================================
#  FAEST-decoupled (12 variants – same layout as FAEST above, but sign is
#  decoupled from keygen/the DRBG: crypto_sign_with_randomness(sk, msg, rho)
#  replays deterministically from a captured (sk, rho) pair alone, so
#  RUN_KEYGEN=0 skips keygen entirely instead of silently calling it just to
#  advance the DRBG. See original/Additional-DS/FAEST-decoupled/<variant>/
#  src/crypto_sign.c and kat/gen/{PQCgenKAT_sign_faest_decoupled,
#  main_faest_decoupled}.c. Targets are suffixed "-decoupled" to avoid
#  colliding with the FAEST_RULES targets above.
# =============================================================================

FAEST_DECOUPLED_COMMON := $(ROOT)/original/Additional-DS/FAEST-decoupled/common

define FAEST_DECOUPLED_RULES
$(1)_DEC_SRCS := $$(wildcard $(FAEST_DECOUPLED_COMMON)/src/*.c) \
             $$(wildcard $(ROOT)/original/Additional-DS/FAEST-decoupled/$(1)/src/*.c)
$(1)_DEC_INC  := -I$(FAEST_DECOUPLED_COMMON)/include -I$(ROOT)/original/Additional-DS/FAEST-decoupled/$(1)/include \
             -I$(GENDIR) -I$(COMMON)
$(1)_DEC_RSP  := $(RSPDIR)/Additional-DS/FAEST-decoupled/$(1).rsp
$(1)_DEC_DEFS := -Wno-psabi

$(BUILD)/gen_$(1)_decoupled: $$($(1)_DEC_SRCS) \
    $(GENDIR)/PQCgenKAT_sign_faest_decoupled.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Additional-DS/FAEST-decoupled
	$(CC) $(CFLAGS) $$($(1)_DEC_DEFS) $$($(1)_DEC_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_DEC_RSP)"' -DALGO_NAME='"$(1)-decoupled"' \
	    $$^ -o $$@

$$($(1)_DEC_RSP): $(BUILD)/gen_$(1)_decoupled | $(RSPDIR)/Additional-DS/FAEST-decoupled
	$$<

$(BUILD)/test_$(1)_decoupled: $$($(1)_DEC_SRCS) $(GENDIR)/main_faest_decoupled.c \
    $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_DEC_DEFS) $$($(1)_DEC_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_DEC_RSP)"' \
	    $$^ -o $$@

gen-$(1)-decoupled: $$($(1)_DEC_RSP)
test-$(1)-decoupled: $$($(1)_DEC_RSP) $(BUILD)/test_$(1)_decoupled
	@echo "=== Testing $(1)-decoupled ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)_decoupled

.PHONY: gen-$(1)-decoupled test-$(1)-decoupled
endef

$(foreach v,$(FAEST_VARIANTS),$(eval $(call FAEST_DECOUPLED_RULES,$(v))))

test-faest-decoupled: $(addprefix test-,$(addsuffix -decoupled,$(FAEST_VARIANTS)))
gen-faest-decoupled:  $(addprefix gen-,$(addsuffix -decoupled,$(FAEST_VARIANTS)))
.PHONY: test-faest-decoupled gen-faest-decoupled

# =============================================================================
#  UOV  (12 variants – shared common/ src, per-variant include/ (params.h only))
#       Ported from pqov (github.com/pqov/pqov, "OV signature"). Unlike FAEST,
#       only ONE file (params.h) differs across the 12 variants (4 security
#       levels x classic/pkc/pkc_skc key formats): every .c/.h in pqov's own
#       src is driven purely by params.h macros, so all of it lives once in
#       UOV/common/{src,include} and each variant dir holds nothing but its
#       own compile-time-fixed include/params.h (generated from pqov's
#       params.h the same way pqov's own create_nist_project.py instantiates
#       per-variant NIST submission dirs). pqov's own randombytes plumbing
#       (common/include/utils_randombytes.h) is patched to route _NIST_KAT_
#       through rng.h without also requiring _UTILS_OPENSSL_, and
#       common/include/config.h's SHAKE backend defaults to the
#       self-contained fips202.c instead of OpenSSL EVP -- two one-line
#       patches (see those files) that make this family fully OpenSSL-free.
#       Same "reseed global DRBG once per count" shape as HAWK/FAEST: UOV has
#       its own copy of the NIST DRBG in common/src/rng.c -- do NOT add
#       kat/gen/rng.c.
# =============================================================================

UOV_COMMON := $(ROOT)/original/Additional-DS/UOV/common
UOV_VARIANTS := UOV_Ip UOV_Ip_pkc UOV_Ip_pkc_skc \
                UOV_Is UOV_Is_pkc UOV_Is_pkc_skc \
                UOV_III UOV_III_pkc UOV_III_pkc_skc \
                UOV_V UOV_V_pkc UOV_V_pkc_skc

define UOV_RULES
$(1)_SRCS := $$(wildcard $(UOV_COMMON)/src/*.c)
$(1)_INC  := -I$(UOV_COMMON)/include -I$(ROOT)/original/Additional-DS/UOV/$(1)/include \
             -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/Additional-DS/UOV/$(1).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) \
    $(GENDIR)/PQCgenKAT_sign_uov.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Additional-DS/UOV
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/Additional-DS/UOV
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) $(GENDIR)/main_uov.c \
    $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(UOV_VARIANTS),$(eval $(call UOV_RULES,$(v))))

test-uov: $(addprefix test-,$(UOV_VARIANTS))
gen-uov:  $(addprefix gen-,$(UOV_VARIANTS))
.PHONY: test-uov gen-uov

# =============================================================================
#  MAYO  (4 variants – shared common/ src, per-variant src/api.c + include/)
#        Ported from MAYO-C (github.com/PQCMayo/MAYO-C), the multivariate
#        signature MAYO. All of mayo.c/params.c/arithmetic.c and the common/
#        support code (fips202.c, portable AES, the NIST CTR-DRBG) are
#        byte-identical across the 4 variants and live once in MAYO/common/;
#        each MAYO_<N>/ directory holds only its own src/api.c (the thin
#        crypto_sign_* wrapper) and include/api.h -- upstream's own per-variant
#        files -- plus a generated include/mayo_variant.h (#define MAYO_VARIANT
#        MAYO_<N> / MAYO_BUILD_TYPE_OPT) so the variant is fixed at compile
#        time without any -D flags, matching every other family here. Already
#        fully self-contained upstream (no OpenSSL anywhere in MAYO-C, unlike
#        pqov/UOV) -- common/include/mayo.h just gained a guarded
#        `#include "mayo_variant.h"` fallback so mayo_variant.h's definitions
#        take effect before mayo.h's own `#if defined(MAYO_VARIANT)` checks.
#        Uses the src/generic (portable, no AVX2/NEON/AES-NI) backend. Same
#        "reseed global DRBG once per count" shape as HAWK/FAEST/UOV: MAYO has
#        no derand entry point, and common/src/randombytes_ctrdrbg.c IS the
#        NIST AES-256-CTR DRBG -- do NOT add kat/gen/rng.c.
# =============================================================================

MAYO_COMMON := $(ROOT)/original/Additional-DS/MAYO/common
MAYO_VARIANTS := MAYO_1 MAYO_2 MAYO_3 MAYO_5

define MAYO_RULES
$(1)_SRCS := $$(wildcard $(MAYO_COMMON)/src/*.c) \
             $$(wildcard $(ROOT)/original/Additional-DS/MAYO/$(1)/src/*.c)
$(1)_INC  := -I$(MAYO_COMMON)/include -I$(ROOT)/original/Additional-DS/MAYO/$(1)/include \
             -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/Additional-DS/MAYO/$(1).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) \
    $(GENDIR)/PQCgenKAT_sign_mayo.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Additional-DS/MAYO
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/Additional-DS/MAYO
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) $(GENDIR)/main_mayo.c \
    $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(MAYO_VARIANTS),$(eval $(call MAYO_RULES,$(v))))

test-mayo: $(addprefix test-,$(MAYO_VARIANTS))
gen-mayo:  $(addprefix gen-,$(MAYO_VARIANTS))
.PHONY: test-mayo gen-mayo

# =============================================================================
#  QRUOV  (24 variants – shared common/ src, per-variant include/ only)
#         Ported from the QR-UOV round-2 reference implementation
#         (github.com/qruov/round2), a multivariate signature. 12 official
#         parameter sets (4 per NIST level 1/3/5: q7L10, q31L3, q31L10,
#         q127L3) x 2 public-key-expansion PRG modes (_a = AES-CTR, _s =
#         SHAKE) = 24 variants, matching upstream's own src/Makefile +
#         src/qruov_config.src build matrix (platform "ref", not the
#         portable64/avx2/avx512 backends). Like UOV, every variant differs
#         by exactly one generated header (include/qruov_config.h, plus a
#         derived include/api.h) -- both produced by literally compiling
#         and running upstream's own qruov_config_h_gen.c/api_h_gen.c
#         generator programs once per variant (same idea as UOV's
#         create_nist_project.py-style params.h instantiation, but using
#         upstream's actual generator instead of text substitution).
#
#         Upstream's OpenSSL dependency here is much deeper than pqov/
#         MAYO-C: the default MGF (mgf.c) reaches into *private* OpenSSL
#         EVP_MD_CTX/EVP_MD struct internals (vendored from openssl-1.1.1t/
#         3.1.0 source) and calls non-public SHA3_absorb/SHA3_squeeze
#         symbols for a streaming SHAKE XOF; the PRG additionally uses
#         EVP_aes_*_ctr (AES-CTR keystream) or EVP_shake* (one-shot XOF).
#         All replaced with self-contained code in QRUOV/common/:
#           - mgf.c/mgf.h: rewritten on the portable fips202.c (already
#             used by the pqov/UOV and MAYO-C ports) -- its keccak_state
#             naturally supports "absorb, finalize once, squeeze
#             repeatedly", the exact MGF_init/update/yield contract.
#           - qruov_prg.c/qruov_prg.h (new files, upstream has no
#             equivalent -- these functions were static inline code in
#             qruov.c): AES-CTR-mode PRG built on MAYO-C's portable
#             AES-128/192/256 (qruov_aes.h/aes_c.c, copied from MAYO-C's
#             src/common/aes_c.c -- constant-time, BearSSL-derived, ECB
#             only) wrapped as CTR-mode-of-zeros == raw keystream, matching
#             upstream's own `memset(dst,0,len); EVP_EncryptUpdate(...)`
#             trick; SHAKE-mode PRG is a one-shot fips202 absorb+squeeze.
#           - qruov.c: trimmed of the ~150 lines of OpenSSL PRG/MGF/hash
#             glue (RejSamp, PRG_init_*, RejSampPRG_*, Expand_mu, Hash),
#             now just #includes qruov_prg.h for those declarations.
#           - qruov.h: OpenSSL includes and EVP_AES_CTR/EVP_SHAKE selection
#             macros removed; the PRG_init/PRG_yield/... name-switching
#             table (AES-CTR vs SHAKE, on QRUOV_PRG_SHAKE) kept as-is but
#             repointed at the qruov_prg.h/mgf.h functions.
#           - rng.c: same OpenSSL-AES256_ECB-swap treatment as pqov/UOV
#             (see UOV's section above) -- kept qruov's own rng.h contract
#             (int-returning randombytes(), unlike HAWK/FAEST/UOV's
#             void-returning kat/gen/rng.h) since qruov's own sign.c/api.c
#             already expect that signature.
#         Same "reseed global DRBG once per count" shape as HAWK/FAEST/UOV/
#         MAYO -- do NOT add kat/gen/rng.c (QRUOV/common/src/rng.c is its
#         own copy, patched the same way).
# =============================================================================

QRUOV_COMMON := $(ROOT)/original/Additional-DS/QRUOV/common
QRUOV_VARIANTS := \
    QRUOV_1_q7L10v740m100_a QRUOV_1_q7L10v740m100_s \
    QRUOV_1_q31L3v165m60_a QRUOV_1_q31L3v165m60_s QRUOV_1_q31L10v600m70_a \
    QRUOV_1_q31L10v600m70_s QRUOV_1_q127L3v156m54_a \
    QRUOV_1_q127L3v156m54_s QRUOV_3_q7L10v1100m140_a \
    QRUOV_3_q7L10v1100m140_s QRUOV_3_q31L3v246m87_a \
    QRUOV_3_q31L3v246m87_s QRUOV_3_q31L10v890m100_a \
    QRUOV_3_q31L10v890m100_s QRUOV_3_q127L3v228m78_a \
    QRUOV_3_q127L3v228m78_s QRUOV_5_q7L10v1490m190_a \
    QRUOV_5_q7L10v1490m190_s QRUOV_5_q31L3v324m114_a \
    QRUOV_5_q31L3v324m114_s QRUOV_5_q31L10v1120m120_a \
    QRUOV_5_q31L10v1120m120_s QRUOV_5_q127L3v306m105_a \
    QRUOV_5_q127L3v306m105_s

define QRUOV_RULES
$(1)_SRCS := $$(wildcard $(QRUOV_COMMON)/src/*.c)
$(1)_INC  := -I$(QRUOV_COMMON)/include -I$(ROOT)/original/Additional-DS/QRUOV/$(1)/include \
             -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/Additional-DS/QRUOV/$(1).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) \
    $(GENDIR)/PQCgenKAT_sign_qruov.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Additional-DS/QRUOV
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/Additional-DS/QRUOV
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) $(GENDIR)/main_qruov.c \
    $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(QRUOV_VARIANTS),$(eval $(call QRUOV_RULES,$(v))))

test-qruov: $(addprefix test-,$(QRUOV_VARIANTS))
gen-qruov:  $(addprefix gen-,$(QRUOV_VARIANTS))
.PHONY: test-qruov gen-qruov

# =============================================================================
#  SNOVA  (36 variants – shared common/ src, per-variant include/ only)
#         Ported from the SNOVA reference implementation
#         (github.com/SNOVA-project or upstream's own checkout -- see the
#         family's own README.md), a multivariate signature. 18 official
#         parameter sets (grouped in 3s: NIST levels 1/3/5) x 2
#         public-key-expansion PRG modes (plain = SHAKE, _AES = AES-CTR) =
#         36 variants, matching upstream's own dist/makesnova.py build
#         matrix -- using the "ref" backend (snova_ref.c/symmetric_ref.c,
#         not the opt/avx2 SIMD ones), same convention as every other
#         family here.
#
#         Unlike UOV/QRUOV, **no OpenSSL patch was needed at all**: SNOVA
#         already ships fully self-contained by default. Its AES
#         (src/aes.c, an OQS-derived portable/AES-NI implementation) and
#         its NIST KAT DRBG (src/rng.c) both gate their OpenSSL EVP calls
#         behind `#ifdef USE_OPENSSL` / `#if 0`, neither ever defined by
#         upstream's own build system -- same situation as MAYO-C. Only
#         real difference from a stock upstream "ref" build: every
#         variant's `snova_params.h` (the only variant-specific file --
#         `snova.h`/`api.h`/`sign.c`/etc. compute all key/signature sizes
#         from it via preprocessor arithmetic, no other file differs) is
#         checked in directly instead of written by
#         dist/makesnova.py at build time, mirroring that script's own
#         template exactly (see the family's own README.md for the exact
#         18-parameter-set list and naming).
#         Same "reseed global DRBG once per count" shape as HAWK/FAEST/
#         UOV/MAYO/QRUOV -- do NOT add kat/gen/rng.c (SNOVA/common/src/rng.c
#         is its own copy, already self-contained, no patch needed).
#
#         The 4 largest parameter sets (level-5 tier: v=97/96 with l=2, and
#         v=52 with l=4x6, x2 for AES/SHAKE = 8 variants) overflow the
#         default 8 MB Linux thread stack inside upstream's own
#         snova_ref.c keygen/sign/verify (large fixed-size local arrays
#         scaling with v/o -- not a bug introduced by this port, verified
#         by running the same binaries with a bigger ulimit). The gen-*/
#         test-* recipes below bump the stack to 64 MB via `ulimit -s`
#         before invoking the generator/tester binaries; do the same
#         (`ulimit -s 65536`) before running these variants' checked-in
#         main.c standalone -- see the family's own README.md.
# =============================================================================

SNOVA_COMMON := $(ROOT)/original/Additional-DS/SNOVA/common
SNOVA_VARIANTS := \
    SNOVA_28_5_19_4_AES SNOVA_28_5_19_4 SNOVA_24_5_16_4_AES \
    SNOVA_24_5_16_4 SNOVA_48_17_16_2_AES SNOVA_48_17_16_2 \
    SNOVA_48_16_19_2_AES SNOVA_48_16_19_2 SNOVA_28_4_16_4x5_AES \
    SNOVA_28_4_16_4x5 SNOVA_28_4_19_4x5_AES SNOVA_28_4_19_4x5 \
    SNOVA_40_7_19_4_AES SNOVA_40_7_19_4 SNOVA_37_8_16_4_AES \
    SNOVA_37_8_16_4 SNOVA_72_25_16_2_AES SNOVA_72_25_16_2 \
    SNOVA_72_24_19_2_AES SNOVA_72_24_19_2 SNOVA_38_5_16_4x5_AES \
    SNOVA_38_5_16_4x5 SNOVA_38_5_19_4x5_AES SNOVA_38_5_19_4x5 \
    SNOVA_50_9_19_4_AES SNOVA_50_9_19_4 SNOVA_60_10_16_4_AES \
    SNOVA_60_10_16_4 SNOVA_97_33_16_2_AES SNOVA_97_33_16_2 \
    SNOVA_96_32_19_2_AES SNOVA_96_32_19_2 SNOVA_52_6_16_4x6_AES \
    SNOVA_52_6_16_4x6 SNOVA_52_6_19_4x6_AES SNOVA_52_6_19_4x6

define SNOVA_RULES
$(1)_SRCS := $$(wildcard $(SNOVA_COMMON)/src/*.c)
$(1)_INC  := -I$(SNOVA_COMMON)/include -I$(ROOT)/original/Additional-DS/SNOVA/$(1)/include \
             -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/Additional-DS/SNOVA/$(1).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) \
    $(GENDIR)/PQCgenKAT_sign_snova.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Additional-DS/SNOVA
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/Additional-DS/SNOVA
	@ulimit -s 65536 2>/dev/null; $$<

$(BUILD)/test_$(1): $$($(1)_SRCS) $(GENDIR)/main_snova.c \
    $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(1) ==="
	@ulimit -s 65536 2>/dev/null; KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(SNOVA_VARIANTS),$(eval $(call SNOVA_RULES,$(v))))

test-snova: $(addprefix test-,$(SNOVA_VARIANTS))
gen-snova:  $(addprefix gen-,$(SNOVA_VARIANTS))
.PHONY: test-snova gen-snova

# =============================================================================
#  SDITH  (12 variants – src/ + include/ layout, full per-variant copy,
#         same convention as HAWK/LESS)
#         Ported from the SDitH reference implementation
#         (github.com/sdith/sdith, see the family's own README.md). Two
#         structurally distinct sub-architectures -- Hypercube_Variant
#         (~10 files: sdith.c/sign.c/treeprg.c/...) and Threshold_Variant
#         (~25 files: mpc.c/keygen.c/commit.c/merkle-tree.c/...) -- each with
#         6 parameter sets (cat1/cat3/cat5 x gf256/p251). Unlike UOV/MAYO/
#         SNOVA, no shared common/ was used: the two sub-architectures differ
#         too much internally, and each already needs its own per-variant
#         patch (see below), so all 12 variants are full self-contained
#         copies, same convention as HAWK/LESS/CROSS.
#
#         Two patches applied per variant to drop the OpenSSL dependency and
#         make it compile-time-fixed:
#          - generator/rng.c (the NIST KAT DRBG) had its OpenSSL EVP-based
#            AES256_ECB() replaced with the same self-contained software
#            AES-256 block cipher used by kat/gen/rng.c (reused verbatim
#            across UOV/QRUOV/SDitH); its `#include <openssl/...>` lines are
#            commented out, never live.
#          - Hypercube's param.h originally selects its security level via
#            an `#ifndef CAT_1/CAT_3/CAT_5` fallback chain driven by a `-D`
#            build flag; patched to a single hardcoded `#define CAT_N`, same
#            technique as UOV's params.h. Hypercube's precomputed.c (the only
#            file that used CAT_N without including param.h -- upstream
#            relies on the compiler's `-DCAT_N` being global to the
#            translation unit) gained an explicit `#include "param.h"` so it
#            picks up the same hardcoded value. Threshold's parameters.h is
#            already fully per-directory-instantiated upstream (literal
#            values, no CAT_N macro at all) -- no equivalent patch needed.
#          - Both sub-architectures also hardcode `#define NDEBUG` (Hypercube
#            in param.h, Threshold in parameters.h) since upstream's own
#            Makefile always passes `-DNDEBUG`; without it, Hypercube's
#            sdith.c prints verbose per-call debug hex dumps to stdout on
#            every keygen/sign/verify.
#         The vendored sha3/ (XKCP) subtree is flattened into src/+include/
#         (no subdirs), like every other family here. Hypercube has a real
#         filename collision when flattened: a core XOF-abstraction rng.h/.c
#         (used by sdith.c/sign.c/treeprg.c/hash-sha3.c) and the KAT
#         generator's NIST-DRBG rng.h/.c are both named "rng" upstream
#         (resolved via nested-directory quote-include order); the core one
#         is renamed to sdith_rng.h/.c here so canonical rng.h/.c stays the
#         NIST DRBG, matching every other family's convention. Threshold has
#         no such collision (its core randomness lives in rnd.c/rnd.h,
#         a distinct name).
#         Same "reseed global DRBG once per count" shape as HAWK/FAEST/UOV/
#         MAYO/QRUOV/SNOVA -- do NOT add kat/gen/rng.c (each variant's own
#         src/rng.c is already the patched, self-contained NIST DRBG).
# =============================================================================

SDITH_VARIANTS := \
    sdith_hypercube_cat1_gf256 sdith_hypercube_cat1_p251 \
    sdith_hypercube_cat3_gf256 sdith_hypercube_cat3_p251 \
    sdith_hypercube_cat5_gf256 sdith_hypercube_cat5_p251 \
    sdith_threshold_cat1_gf256 sdith_threshold_cat1_p251 \
    sdith_threshold_cat3_gf256 sdith_threshold_cat3_p251 \
    sdith_threshold_cat5_gf256 sdith_threshold_cat5_p251

define SDITH_RULES
$(1)_SRCS := $$(wildcard $(ROOT)/original/Additional-DS/SDITH/$(1)/src/*.c)
$(1)_INC  := -I$(ROOT)/original/Additional-DS/SDITH/$(1)/include -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/Additional-DS/SDITH/$(1).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) \
    $(GENDIR)/PQCgenKAT_sign_sdith.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Additional-DS/SDITH
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/Additional-DS/SDITH
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) $(GENDIR)/main_sdith.c \
    $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(SDITH_VARIANTS),$(eval $(call SDITH_RULES,$(v))))

test-sdith: $(addprefix test-,$(SDITH_VARIANTS))
gen-sdith:  $(addprefix gen-,$(SDITH_VARIANTS))
.PHONY: test-sdith gen-sdith

# =============================================================================
#  SQISIGN  (3 variants – shared common/ src, per-variant src/+include/)
#           Ported from the-sqisign reference implementation
#           (github.com/SQIsign/the-sqisign), an isogeny-based signature over
#           quaternion algebras. 3 variants (lvl1/lvl3/lvl5, NIST levels
#           1/3/5), "ref" build type (no ASM/SIMD backends), using upstream's
#           own bundled mini-gmp (GMP_LIBRARY=MINI in their CMake) instead of
#           system GMP -- confirmed byte-identical against upstream's official
#           KAT/*.rsp when built via their own CMake with mini-gmp. Of the
#           ~72 files needed per variant, 48 .c + 33 .h are identical text
#           across all 3 levels (common/) -- only 8 .c + 11 .h (api.c/api.h,
#           the field-arithmetic fp_p*.c, and 6 SageMath-precomputed constant
#           files/headers) actually differ per level, same shape as MAYO's
#           common/ + per-variant src/api.c split. No OpenSSL, no real GMP:
#           its own AES (common/ref/aes_c.c) backs the NIST KAT DRBG
#           (common/ref/randombytes_ctrdrbg.c), and MINI_GMP routes all
#           big-integer arithmetic through the vendored mini-gmp/ instead of
#           libgmp. The handful of upstream CMake `-D` build flags that
#           aren't already baked into a per-file `#ifdef` (MINI_GMP,
#           RANDOMBYTES_C, HAVE_UINT128, RADIX_64, GMP_LIMB_BITS=64) are
#           hardcoded into a small common/include/sqisign_build_config.h,
#           `#include`d first by every .c file, so no `-D` flags are needed
#           to build a specific variant; SQISIGN_VARIANT/SQISIGN_BUILD_TYPE_REF/
#           ENABLE_SIGN are hardcoded the same way inside each variant's own
#           copy of sqisign_namespace.h (the one file whose value genuinely
#           differs per level). Needs `-lm` (libm) at link time -- the only
#           family here that does, since quaternion/ arithmetic uses libm.
# =============================================================================

SQISIGN_COMMON := $(ROOT)/original/Additional-DS/SQISIGN/common
SQISIGN_VARIANTS := sqisign_lvl1 sqisign_lvl3 sqisign_lvl5

define SQISIGN_RULES
$(1)_SRCS := $$(wildcard $(SQISIGN_COMMON)/src/*.c) \
             $$(wildcard $(ROOT)/original/Additional-DS/SQISIGN/$(1)/src/*.c)
$(1)_INC  := -I$(SQISIGN_COMMON)/include -I$(ROOT)/original/Additional-DS/SQISIGN/$(1)/include \
             -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/Additional-DS/SQISIGN/$(1).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) \
    $(GENDIR)/PQCgenKAT_sign_sqisign.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Additional-DS/SQISIGN
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@ -lm

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/Additional-DS/SQISIGN
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) $(GENDIR)/main_sqisign.c \
    $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@ -lm

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(SQISIGN_VARIANTS),$(eval $(call SQISIGN_RULES,$(v))))

test-sqisign: $(addprefix test-,$(SQISIGN_VARIANTS))
gen-sqisign:  $(addprefix gen-,$(SQISIGN_VARIANTS))
.PHONY: test-sqisign gen-sqisign

# =============================================================================
#  ML-DSA  (3 variants – src/ + include/ layout)
#           Exclude src/randombytes.c (uses /dev/urandom); kat/gen/rng.c provides
#           the NIST DRBG randombytes() for generators and satisfies the linker
#           symbol for test binaries (not actually called with derand API).
# =============================================================================

MLDSA_VARIANTS := ML-DSA-2 ML-DSA-3 ML-DSA-5

define MLDSA_RULES
$(subst -,_,$(1))_SRCS_ALL := $$(wildcard $(ROOT)/original/DS/ML-DSA/$(1)/src/*.c)
$(subst -,_,$(1))_SRCS := $$(filter-out %/randombytes.c,$$($(subst -,_,$(1))_SRCS_ALL))
$(subst -,_,$(1))_INC  := -I$(ROOT)/original/DS/ML-DSA/$(1)/include -I$(GENDIR) -I$(COMMON)
$(subst -,_,$(1))_RSP  := $(RSPDIR)/DS/ML-DSA/$(1).rsp

$(BUILD)/gen_$(subst -,_,$(1)): $$($(subst -,_,$(1))_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_mldsa.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/DS/ML-DSA
	$(CC) $(CFLAGS) $$($(subst -,_,$(1))_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(subst -,_,$(1))_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(subst -,_,$(1))_RSP): $(BUILD)/gen_$(subst -,_,$(1)) | $(RSPDIR)/DS/ML-DSA
	$$<

$(BUILD)/test_$(subst -,_,$(1)): $$($(subst -,_,$(1))_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_mldsa.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(subst -,_,$(1))_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$$($(subst -,_,$(1))_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(subst -,_,$(1))_RSP)
test-$(1): $$($(subst -,_,$(1))_RSP) $(BUILD)/test_$(subst -,_,$(1))
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(subst -,_,$(1))

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(MLDSA_VARIANTS),$(eval $(call MLDSA_RULES,$(v))))

test-mldsa: $(addprefix test-,$(MLDSA_VARIANTS))
gen-mldsa:  $(addprefix gen-,$(MLDSA_VARIANTS))
.PHONY: test-mldsa gen-mldsa

# =============================================================================
#  SLH-DSA families  (4 families × 6 variants = 24 total)
#  All use the same pattern: exclude randombytes.c, use kat/gen/rng.c.
#  Macro: $(1)=safe-name $(2)=variant-dir $(3)=family-subdir $(4)=rsp-family-dir
# =============================================================================

define SLHDSA_RULES
# $(1)=safe-name $(2)=variant-dir $(3)=family-subdir $(4)=rsp-family-dir $(5)=extra-cflags
$(1)_SRCS_ALL := $$(wildcard $(ROOT)/original/DS/$(3)/$(2)/src/*.c)
$(1)_SRCS := $$(filter-out %/randombytes.c,$$($(1)_SRCS_ALL))
$(1)_INC  := -I$(ROOT)/original/DS/$(3)/$(2)/include -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/DS/$(4)/$(2).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_slhdsa.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/DS/$(4)
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) $(5) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(2)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/DS/$(4)
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_slhdsa.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(SIGN_TFLAGS) $(5) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(2) ($(3)) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

# --- SLH-DSA (SHAKE reference) ---
$(eval $(call SLHDSA_RULES,SPHINCSp128fsimple,SPHINCS+-128f-simple,SLH-DSA,SLH-DSA))
$(eval $(call SLHDSA_RULES,SPHINCSp128frobust,SPHINCS+-128f-robust,SLH-DSA,SLH-DSA))
$(eval $(call SLHDSA_RULES,SPHINCSp192fsimple,SPHINCS+-192f-simple,SLH-DSA,SLH-DSA))
$(eval $(call SLHDSA_RULES,SPHINCSp192frobust,SPHINCS+-192f-robust,SLH-DSA,SLH-DSA))
$(eval $(call SLHDSA_RULES,SPHINCSp256fsimple,SPHINCS+-256f-simple,SLH-DSA,SLH-DSA))
$(eval $(call SLHDSA_RULES,SPHINCSp256frobust,SPHINCS+-256f-robust,SLH-DSA,SLH-DSA))
# "s" (small) sizes -- from a different (Saarinen, FIPS 205 final) reference that
# only implements "simple" mode, see original/DS/SLH-DSA/SPHINCS+-*-simple/src/sign.c
$(eval $(call SLHDSA_RULES,SPHINCSp128ssimple,SPHINCS+-128s-simple,SLH-DSA,SLH-DSA))
$(eval $(call SLHDSA_RULES,SPHINCSp192ssimple,SPHINCS+-192s-simple,SLH-DSA,SLH-DSA))
$(eval $(call SLHDSA_RULES,SPHINCSp256ssimple,SPHINCS+-256s-simple,SLH-DSA,SLH-DSA))

SLHDSA_SAFE_NAMES := SPHINCSp128fsimple SPHINCSp128frobust \
                     SPHINCSp192fsimple SPHINCSp192frobust \
                     SPHINCSp256fsimple SPHINCSp256frobust \
                     SPHINCSp128ssimple SPHINCSp192ssimple SPHINCSp256ssimple

test-slhdsa: $(addprefix test-,$(SLHDSA_SAFE_NAMES))
gen-slhdsa:  $(addprefix gen-,$(SLHDSA_SAFE_NAMES))
.PHONY: test-slhdsa gen-slhdsa

# --- SLH-DSA-Haraka ---
$(eval $(call SLHDSA_RULES,HARAKAp128fsimple,SPHINCS+-128f-simple,SLH-DSA-Haraka,SLH-DSA-Haraka))
$(eval $(call SLHDSA_RULES,HARAKAp128frobust,SPHINCS+-128f-robust,SLH-DSA-Haraka,SLH-DSA-Haraka))
$(eval $(call SLHDSA_RULES,HARAKAp192fsimple,SPHINCS+-192f-simple,SLH-DSA-Haraka,SLH-DSA-Haraka))
$(eval $(call SLHDSA_RULES,HARAKAp192frobust,SPHINCS+-192f-robust,SLH-DSA-Haraka,SLH-DSA-Haraka))
$(eval $(call SLHDSA_RULES,HARAKAp256fsimple,SPHINCS+-256f-simple,SLH-DSA-Haraka,SLH-DSA-Haraka))
$(eval $(call SLHDSA_RULES,HARAKAp256frobust,SPHINCS+-256f-robust,SLH-DSA-Haraka,SLH-DSA-Haraka))

HARAKA_SAFE_NAMES := HARAKAp128fsimple HARAKAp128frobust \
                     HARAKAp192fsimple HARAKAp192frobust \
                     HARAKAp256fsimple HARAKAp256frobust

test-slhdsa-haraka: $(addprefix test-,$(HARAKA_SAFE_NAMES))
gen-slhdsa-haraka:  $(addprefix gen-,$(HARAKA_SAFE_NAMES))
.PHONY: test-slhdsa-haraka gen-slhdsa-haraka

# --- SLH-DSA-Haraka-AES ---
# haraka.c uses the aes64esm RISC-V scalar AES instruction and cannot be
# assembled on x86/ARM. On non-RISC-V hosts we emit stub targets that
# print a clear skip message so the rest of the build is unaffected.
ARCH := $(shell uname -m)

ifeq ($(filter riscv%,$(ARCH)),riscv64)
$(eval $(call SLHDSA_RULES,HARAKAAESp128fsimple,SPHINCS+-128f-simple,SLH-DSA-Haraka-AES,SLH-DSA-Haraka-AES,-DSPX_HARAKA_RVAES_ACCEL))
$(eval $(call SLHDSA_RULES,HARAKAAESp128frobust,SPHINCS+-128f-robust,SLH-DSA-Haraka-AES,SLH-DSA-Haraka-AES,-DSPX_HARAKA_RVAES_ACCEL))
$(eval $(call SLHDSA_RULES,HARAKAAESp192fsimple,SPHINCS+-192f-simple,SLH-DSA-Haraka-AES,SLH-DSA-Haraka-AES,-DSPX_HARAKA_RVAES_ACCEL))
$(eval $(call SLHDSA_RULES,HARAKAAESp192frobust,SPHINCS+-192f-robust,SLH-DSA-Haraka-AES,SLH-DSA-Haraka-AES,-DSPX_HARAKA_RVAES_ACCEL))
$(eval $(call SLHDSA_RULES,HARAKAAESp256fsimple,SPHINCS+-256f-simple,SLH-DSA-Haraka-AES,SLH-DSA-Haraka-AES,-DSPX_HARAKA_RVAES_ACCEL))
$(eval $(call SLHDSA_RULES,HARAKAAESp256frobust,SPHINCS+-256f-robust,SLH-DSA-Haraka-AES,SLH-DSA-Haraka-AES,-DSPX_HARAKA_RVAES_ACCEL))
test-slhdsa-haraka-aes gen-slhdsa-haraka-aes: \
    test-HARAKAAESp128fsimple test-HARAKAAESp128frobust \
    test-HARAKAAESp192fsimple test-HARAKAAESp192frobust \
    test-HARAKAAESp256fsimple test-HARAKAAESp256frobust
else
_HARAKAAE_VARIANTS := HARAKAAESp128fsimple HARAKAAESp128frobust \
                      HARAKAAESp192fsimple HARAKAAESp192frobust \
                      HARAKAAESp256fsimple HARAKAAESp256frobust
$(addprefix test-,$(_HARAKAAE_VARIANTS)) \
$(addprefix gen-,$(_HARAKAAE_VARIANTS)) \
test-slhdsa-haraka-aes gen-slhdsa-haraka-aes:
	@echo "SKIP: SLH-DSA-Haraka-AES requires RISC-V hardware (aes64esm). Host: $(ARCH)"
endif
.PHONY: test-slhdsa-haraka-aes gen-slhdsa-haraka-aes

# --- SLH-DSA-SHA2 ---
$(eval $(call SLHDSA_RULES,SHA2p128fsimple,SPHINCS+-128f-simple,SLH-DSA-SHA2,SLH-DSA-SHA2))
$(eval $(call SLHDSA_RULES,SHA2p128frobust,SPHINCS+-128f-robust,SLH-DSA-SHA2,SLH-DSA-SHA2))
$(eval $(call SLHDSA_RULES,SHA2p192fsimple,SPHINCS+-192f-simple,SLH-DSA-SHA2,SLH-DSA-SHA2))
$(eval $(call SLHDSA_RULES,SHA2p192frobust,SPHINCS+-192f-robust,SLH-DSA-SHA2,SLH-DSA-SHA2))
$(eval $(call SLHDSA_RULES,SHA2p256fsimple,SPHINCS+-256f-simple,SLH-DSA-SHA2,SLH-DSA-SHA2))
$(eval $(call SLHDSA_RULES,SHA2p256frobust,SPHINCS+-256f-robust,SLH-DSA-SHA2,SLH-DSA-SHA2))
# "s" (small) sizes -- see the SLH-DSA "s" comment above.
$(eval $(call SLHDSA_RULES,SHA2p128ssimple,SPHINCS+-128s-simple,SLH-DSA-SHA2,SLH-DSA-SHA2))
$(eval $(call SLHDSA_RULES,SHA2p192ssimple,SPHINCS+-192s-simple,SLH-DSA-SHA2,SLH-DSA-SHA2))
$(eval $(call SLHDSA_RULES,SHA2p256ssimple,SPHINCS+-256s-simple,SLH-DSA-SHA2,SLH-DSA-SHA2))

SHA2_SAFE_NAMES := SHA2p128fsimple SHA2p128frobust \
                   SHA2p192fsimple SHA2p192frobust \
                   SHA2p256fsimple SHA2p256frobust \
                   SHA2p128ssimple SHA2p192ssimple SHA2p256ssimple

test-slhdsa-sha2: $(addprefix test-,$(SHA2_SAFE_NAMES))
gen-slhdsa-sha2:  $(addprefix gen-,$(SHA2_SAFE_NAMES))
.PHONY: test-slhdsa-sha2 gen-slhdsa-sha2

# =============================================================================
#  XMSS / XMSS^MT  (Stateful-DS -- NIST SP 800-208, not part of test-all)
#  Unlike every other family, pk/sk/sig sizes are runtime-variable (chosen by
#  parameter-set name, not compile-time constants), and one shared source
#  tree covers all 77 parameter sets -- see original/Stateful-DS/XMSS/README.md.
#    test-xmss        default variant only (XMSS-SHA2_10_256) -- in test-stateful-ds
#    test-xmss-check       49 "fast" variants (7 single-tree h=10 + 42
#                           XMSS^MT sets with per-layer height <=10) --
#                           NOT in test-all/test-stateful-ds
#    test-xmss-check-slow  28 "slow" variants (effective tree height 16/20,
#                           whether single-tree or XMSS^MT "40/2"/"60/3";
#                           each can take minutes to over an hour) --
#                           NOT in test-all/test-stateful-ds
# =============================================================================

XMSS_DEFAULT_VARIANT := XMSS-SHA2_10_256
XMSS_SRCS := $(wildcard $(ROOT)/original/Stateful-DS/XMSS/src/*.c)
XMSS_INC  := -I$(ROOT)/original/Stateful-DS/XMSS/include -I$(GENDIR) -I$(COMMON)
XMSS_RSP  := $(RSPDIR)/Stateful-DS/XMSS/$(XMSS_DEFAULT_VARIANT).rsp

$(BUILD)/gen_xmss: $(XMSS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_xmss.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Stateful-DS/XMSS
	$(CC) $(CFLAGS) $(XMSS_INC) $(GEN_FLAGS) \
	    -DXMSS_VARIANT_STR='"$(XMSS_DEFAULT_VARIANT)"' \
	    -DRSP_PATH='"$(XMSS_RSP)"' -DALGO_NAME='"$(XMSS_DEFAULT_VARIANT)"' \
	    $^ -o $@

$(XMSS_RSP): $(BUILD)/gen_xmss | $(RSPDIR)/Stateful-DS/XMSS
	$<

$(BUILD)/test_xmss: $(XMSS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_xmss.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(XMSS_INC) $(SIGN_TFLAGS) \
	    -DXMSS_VARIANT_STR='"$(XMSS_DEFAULT_VARIANT)"' \
	    -DRSP_PATH='"$(XMSS_RSP)"' \
	    $^ -o $@

gen-xmss: $(XMSS_RSP)
test-xmss: $(XMSS_RSP) $(BUILD)/test_xmss
	@echo "=== Testing XMSS ($(XMSS_DEFAULT_VARIANT)) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_xmss

.PHONY: gen-xmss test-xmss

# --- check sweeps: one binary per tier, each looping internally over its
#     own variant table (see original/Stateful-DS/XMSS/src/sign.c) ---
define XMSS_CHECK_RULES
# $(1)=tier (check|check-slow) $(2)=extra cflags (empty, or -DXMSS_CHECK_SLOW=1)
$(BUILD)/gen_xmss_$(subst -,_,$(1)): $$(XMSS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_xmss.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$(XMSS_INC) -DKATNUM=$(CHECK_KATNUM) -DMLEN=$(MLEN) \
	    -DXMSS_CHECK_ALL=1 $(2) \
	    -DRSP_DIR='"$(RSPDIR)/Stateful-DS/XMSS-$(1)"' \
	    $$^ -o $$@

$(BUILD)/test_xmss_$(subst -,_,$(1)): $$(XMSS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_xmss.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$(XMSS_INC) $(SIGN_TFLAGS) -DKATNUM=$(CHECK_KATNUM) \
	    -DXMSS_CHECK_ALL=1 $(2) \
	    -DRSP_DIR='"$(RSPDIR)/Stateful-DS/XMSS-$(1)"' \
	    $$^ -o $$@

gen-xmss-$(1): $(BUILD)/gen_xmss_$(subst -,_,$(1))
	@echo "=== Generating XMSS $(1) sweep (KATNUM=$(CHECK_KATNUM) per variant) ==="
	@$$<

test-xmss-$(1): gen-xmss-$(1) $(BUILD)/test_xmss_$(subst -,_,$(1))
	@echo "=== Testing XMSS $(1) sweep ==="
	@KATNUM=$(CHECK_KATNUM) $(BUILD)/test_xmss_$(subst -,_,$(1))

.PHONY: gen-xmss-$(1) test-xmss-$(1)
endef

$(eval $(call XMSS_CHECK_RULES,check,))
$(eval $(call XMSS_CHECK_RULES,check-slow,-DXMSS_CHECK_SLOW=1))

# =============================================================================
#  LMS / HSS  (Cisco hash-sigs; LMS = levels=1, HSS = levels 2-3 here)
#  Stateful-DS -- NIST SP 800-208, not part of test-all. Same
#  runtime-variable-size situation as XMSS -- see
#  original/Stateful-DS/HSS/README.md and original/Stateful-DS/LMS/README.md.
#    test-lms / test-hss            default variant only -- in test-stateful-ds
#    test-lms-check / test-hss-check       8/16 "fast" variants (heights
#                                           5/10) -- NOT in test-all/test-stateful-ds
#    test-lms-check-slow / test-hss-check-slow   12/24 "slow" variants
#                                           (heights 15/20/25; keygen alone
#                                           can take minutes to hours at the
#                                           taller ones) -- NOT in test-all/test-stateful-ds
# =============================================================================

LMS_SRCS := $(wildcard $(ROOT)/original/Stateful-DS/LMS/src/*.c)
LMS_INC  := -I$(ROOT)/original/Stateful-DS/LMS/include -I$(GENDIR) -I$(COMMON)
LMS_RSP  := $(RSPDIR)/Stateful-DS/LMS/lms_default.rsp

$(BUILD)/gen_lms: $(LMS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_lms.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Stateful-DS/LMS
	$(CC) $(CFLAGS) $(LMS_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$(LMS_RSP)"' -DALGO_NAME='"lms"' \
	    $^ -o $@

$(LMS_RSP): $(BUILD)/gen_lms | $(RSPDIR)/Stateful-DS/LMS
	$<

$(BUILD)/test_lms: $(LMS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_lms.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(LMS_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$(LMS_RSP)"' \
	    $^ -o $@

gen-lms: $(LMS_RSP)
test-lms: $(LMS_RSP) $(BUILD)/test_lms
	@echo "=== Testing lms (default variant) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_lms

.PHONY: gen-lms test-lms

$(BUILD)/gen_lms_check: $(LMS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_lms.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(LMS_INC) -DKATNUM=$(CHECK_KATNUM) -DMLEN=$(MLEN) \
	    -DLMS_CHECK_ALL=1 \
	    -DRSP_DIR='"$(RSPDIR)/Stateful-DS/LMS-check"' \
	    $^ -o $@

$(BUILD)/test_lms_check: $(LMS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_lms.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(LMS_INC) $(SIGN_TFLAGS) -DKATNUM=$(CHECK_KATNUM) \
	    -DLMS_CHECK_ALL=1 \
	    -DRSP_DIR='"$(RSPDIR)/Stateful-DS/LMS-check"' \
	    $^ -o $@

gen-lms-check: $(BUILD)/gen_lms_check
	@echo "=== Generating lms check sweep (KATNUM=$(CHECK_KATNUM) per variant) ==="
	@$<

test-lms-check: gen-lms-check $(BUILD)/test_lms_check
	@echo "=== Testing lms check sweep ==="
	@KATNUM=$(CHECK_KATNUM) $(BUILD)/test_lms_check

.PHONY: gen-lms-check test-lms-check

$(BUILD)/gen_lms_check_slow: $(LMS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_lms.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(LMS_INC) -DKATNUM=$(CHECK_KATNUM) -DMLEN=$(MLEN) \
	    -DLMS_CHECK_ALL=1 -DLMS_CHECK_SLOW=1 \
	    -DRSP_DIR='"$(RSPDIR)/Stateful-DS/LMS-check-slow"' \
	    $^ -o $@

$(BUILD)/test_lms_check_slow: $(LMS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_lms.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(LMS_INC) $(SIGN_TFLAGS) -DKATNUM=$(CHECK_KATNUM) \
	    -DLMS_CHECK_ALL=1 -DLMS_CHECK_SLOW=1 \
	    -DRSP_DIR='"$(RSPDIR)/Stateful-DS/LMS-check-slow"' \
	    $^ -o $@

gen-lms-check-slow: $(BUILD)/gen_lms_check_slow
	@echo "=== Generating lms check-slow sweep (KATNUM=$(CHECK_KATNUM) per variant) ==="
	@$<

test-lms-check-slow: gen-lms-check-slow $(BUILD)/test_lms_check_slow
	@echo "=== Testing lms check-slow sweep ==="
	@KATNUM=$(CHECK_KATNUM) $(BUILD)/test_lms_check_slow

.PHONY: gen-lms-check-slow test-lms-check-slow

HSS_SRCS := $(wildcard $(ROOT)/original/Stateful-DS/HSS/src/*.c)
HSS_INC  := -I$(ROOT)/original/Stateful-DS/HSS/include -I$(GENDIR) -I$(COMMON)
HSS_RSP  := $(RSPDIR)/Stateful-DS/HSS/hss_default.rsp

$(BUILD)/gen_hss: $(HSS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_hss.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Stateful-DS/HSS
	$(CC) $(CFLAGS) $(HSS_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$(HSS_RSP)"' -DALGO_NAME='"hss"' \
	    $^ -o $@

$(HSS_RSP): $(BUILD)/gen_hss | $(RSPDIR)/Stateful-DS/HSS
	$<

$(BUILD)/test_hss: $(HSS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_hss.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(HSS_INC) $(SIGN_TFLAGS) \
	    -DRSP_PATH='"$(HSS_RSP)"' \
	    $^ -o $@

gen-hss: $(HSS_RSP)
test-hss: $(HSS_RSP) $(BUILD)/test_hss
	@echo "=== Testing hss (default variant) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_hss

.PHONY: gen-hss test-hss

$(BUILD)/gen_hss_check: $(HSS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_hss.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(HSS_INC) -DKATNUM=$(CHECK_KATNUM) -DMLEN=$(MLEN) \
	    -DHSS_CHECK_ALL=1 \
	    -DRSP_DIR='"$(RSPDIR)/Stateful-DS/HSS-check"' \
	    $^ -o $@

$(BUILD)/test_hss_check: $(HSS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_hss.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(HSS_INC) $(SIGN_TFLAGS) -DKATNUM=$(CHECK_KATNUM) \
	    -DHSS_CHECK_ALL=1 \
	    -DRSP_DIR='"$(RSPDIR)/Stateful-DS/HSS-check"' \
	    $^ -o $@

gen-hss-check: $(BUILD)/gen_hss_check
	@echo "=== Generating hss check sweep (KATNUM=$(CHECK_KATNUM) per variant) ==="
	@$<

test-hss-check: gen-hss-check $(BUILD)/test_hss_check
	@echo "=== Testing hss check sweep ==="
	@KATNUM=$(CHECK_KATNUM) $(BUILD)/test_hss_check

.PHONY: gen-hss-check test-hss-check

$(BUILD)/gen_hss_check_slow: $(HSS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_sign_hss.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(HSS_INC) -DKATNUM=$(CHECK_KATNUM) -DMLEN=$(MLEN) \
	    -DHSS_CHECK_ALL=1 -DHSS_CHECK_SLOW=1 \
	    -DRSP_DIR='"$(RSPDIR)/Stateful-DS/HSS-check-slow"' \
	    $^ -o $@

$(BUILD)/test_hss_check_slow: $(HSS_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_hss.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(HSS_INC) $(SIGN_TFLAGS) -DKATNUM=$(CHECK_KATNUM) \
	    -DHSS_CHECK_ALL=1 -DHSS_CHECK_SLOW=1 \
	    -DRSP_DIR='"$(RSPDIR)/Stateful-DS/HSS-check-slow"' \
	    $^ -o $@

gen-hss-check-slow: $(BUILD)/gen_hss_check_slow
	@echo "=== Generating hss check-slow sweep (KATNUM=$(CHECK_KATNUM) per variant) ==="
	@$<

test-hss-check-slow: gen-hss-check-slow $(BUILD)/test_hss_check_slow
	@echo "=== Testing hss check-slow sweep ==="
	@KATNUM=$(CHECK_KATNUM) $(BUILD)/test_hss_check_slow

.PHONY: gen-hss-check-slow test-hss-check-slow

# =============================================================================
#  ML-KEM  (3 variants)
#           Exclude src/randombytes.c; kat/gen/rng.c provides randombytes()
#           (needed by kem.c non-derand wrappers even in test builds).
# =============================================================================

MLKEM_VARIANTS := ml-kem-512 ml-kem-768 ml-kem-1024

define MLKEM_RULES
$(subst -,_,$(1))_SRCS_ALL := $$(wildcard $(ROOT)/original/KEM/ML-KEM/$(1)/src/*.c)
$(subst -,_,$(1))_SRCS := $$(filter-out %/randombytes.c,$$($(subst -,_,$(1))_SRCS_ALL))
$(subst -,_,$(1))_INC  := -I$(ROOT)/original/KEM/ML-KEM/$(1)/include -I$(GENDIR) -I$(COMMON)
$(subst -,_,$(1))_RSP  := $(RSPDIR)/KEM/ML-KEM/$(1).rsp

$(BUILD)/gen_$(subst -,_,$(1)): $$($(subst -,_,$(1))_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_kem_mlkem.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/KEM/ML-KEM
	$(CC) $(CFLAGS) $$($(subst -,_,$(1))_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(subst -,_,$(1))_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(subst -,_,$(1))_RSP): $(BUILD)/gen_$(subst -,_,$(1)) | $(RSPDIR)/KEM/ML-KEM
	$$<

$(BUILD)/test_$(subst -,_,$(1)): $$($(subst -,_,$(1))_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_mlkem.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(subst -,_,$(1))_INC) $(KEM_TFLAGS) \
	    -DRSP_PATH='"$$($(subst -,_,$(1))_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(subst -,_,$(1))_RSP)
test-$(1): $$($(subst -,_,$(1))_RSP) $(BUILD)/test_$(subst -,_,$(1))
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(subst -,_,$(1))

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(MLKEM_VARIANTS),$(eval $(call MLKEM_RULES,$(v))))

test-mlkem: $(addprefix test-,$(MLKEM_VARIANTS))
gen-mlkem:  $(addprefix gen-,$(MLKEM_VARIANTS))
.PHONY: test-mlkem gen-mlkem

# =============================================================================
#  HQC  (HQC-2025 only, 3 variants)
#  HQC-2025 uses the standard 3-arg NIST KEM API and seeds randomness via
#  prng_init()/prng_get_bytes() internally — no explicit seed parameters.
#  Macro: $(1)=safe-name $(2)=variant-dir $(3)=version $(4)=rsp-version-dir
# =============================================================================

define HQC2025_RULES
# $(1)=safe-name $(2)=variant-dir $(3)=version $(4)=rsp-version-dir
$(1)_SRCS := $$(wildcard $(ROOT)/original/KEM/HQC/$(3)/$(2)/src/*.c)
$(1)_INC  := -I$(ROOT)/original/KEM/HQC/$(3)/$(2)/include -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/KEM/HQC/$(4)/$(2).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_kem_hqc2025.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/KEM/HQC/$(4)
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"HQC-$(3)/$(2)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/KEM/HQC/$(4)
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/main_hqc2025.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(KEM_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(2) ($(3)) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

# HQC-2025: standard 3-arg NIST API with internal SHAKE-256 PRNG
$(eval $(call HQC2025_RULES,HQC2025_1,HQC-1,HQC-2025,HQC-2025))
$(eval $(call HQC2025_RULES,HQC2025_3,HQC-3,HQC-2025,HQC-2025))
$(eval $(call HQC2025_RULES,HQC2025_5,HQC-5,HQC-2025,HQC-2025))

HQC_SAFE_NAMES := HQC2025_1 HQC2025_3 HQC2025_5

test-hqc: $(addprefix test-,$(HQC_SAFE_NAMES))
gen-hqc:  $(addprefix gen-,$(HQC_SAFE_NAMES))
.PHONY: test-hqc gen-hqc

# =============================================================================
#  ASCON  (Symmetric -- not PQC, not part of test-all/gen-all/static-test-all.
#          No keygen: AEAD variants are encrypt/decrypt, hash/XOF variants
#          are a single crypto_hash() call. See original/Symmetric/ASCON/README.md.)
# =============================================================================

ASCON_AEAD_VARIANTS := ascon128v12 ascon128av12 ascon80pqv12
ASCON_HASH_VARIANTS := asconhashv12 asconhashav12 asconxofv12 asconxofav12

define ASCON_AEAD_RULES
$(1)_SRCS := $$(wildcard $(ROOT)/original/Symmetric/ASCON/$(1)/src/*.c)
$(1)_INC  := -I$(ROOT)/original/Symmetric/ASCON/$(1)/include -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/Symmetric/ASCON/$(1).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_aead_ascon.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Symmetric/ASCON
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/Symmetric/ASCON
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) \
    $(GENDIR)/main_ascon_aead.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(ASCON_AEAD_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(ASCON_AEAD_VARIANTS),$(eval $(call ASCON_AEAD_RULES,$(v))))

test-ascon-aead: $(addprefix test-,$(ASCON_AEAD_VARIANTS))
gen-ascon-aead:  $(addprefix gen-,$(ASCON_AEAD_VARIANTS))
.PHONY: test-ascon-aead gen-ascon-aead

define ASCON_HASH_RULES
$(1)_SRCS := $$(wildcard $(ROOT)/original/Symmetric/ASCON/$(1)/src/*.c)
$(1)_INC  := -I$(ROOT)/original/Symmetric/ASCON/$(1)/include -I$(GENDIR) -I$(COMMON)
$(1)_RSP  := $(RSPDIR)/Symmetric/ASCON/$(1).rsp

$(BUILD)/gen_$(1): $$($(1)_SRCS) $(GENDIR)/rng.c \
    $(GENDIR)/PQCgenKAT_hash_ascon.c $(COMMON)/kat_parser.c \
    | $(BUILD) $(RSPDIR)/Symmetric/ASCON
	$(CC) $(CFLAGS) $$($(1)_INC) $(GEN_FLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' -DALGO_NAME='"$(1)"' \
	    $$^ -o $$@

$$($(1)_RSP): $(BUILD)/gen_$(1) | $(RSPDIR)/Symmetric/ASCON
	$$<

$(BUILD)/test_$(1): $$($(1)_SRCS) \
    $(GENDIR)/main_ascon_hash.c $(COMMON)/kat_parser.c | $(BUILD)
	$(CC) $(CFLAGS) $$($(1)_INC) $(ASCON_HASH_TFLAGS) \
	    -DRSP_PATH='"$$($(1)_RSP)"' \
	    $$^ -o $$@

gen-$(1): $$($(1)_RSP)
test-$(1): $$($(1)_RSP) $(BUILD)/test_$(1)
	@echo "=== Testing $(1) ==="
	@KATNUM=$(KATNUM) $(BUILD)/test_$(1)

.PHONY: gen-$(1) test-$(1)
endef

$(foreach v,$(ASCON_HASH_VARIANTS),$(eval $(call ASCON_HASH_RULES,$(v))))

test-ascon-hash: $(addprefix test-,$(ASCON_HASH_VARIANTS))
gen-ascon-hash:  $(addprefix gen-,$(ASCON_HASH_VARIANTS))
.PHONY: test-ascon-hash gen-ascon-hash

test-ascon: test-ascon-aead test-ascon-hash
gen-ascon:  gen-ascon-aead gen-ascon-hash
.PHONY: test-ascon gen-ascon

# =============================================================================
#  Top-level aggregation targets
#
#  test-all / gen-all cover only the *standardized* algorithms: ML-KEM,
#  ML-DSA, all 4 SLH-DSA subfamilies, FALCON, HQC. Additional-DS (CROSS,
#  HAWK, LESS -- not standardized), Stateful-DS (XMSS, LMS, HSS -- NIST
#  SP 800-208, standardized separately from FIPS 203/204/205/206), and
#  ASCON (not PQC at all) are deliberately excluded -- use their own
#  aggregate targets below.
# =============================================================================

DS_GEN_TARGETS := \
    gen-falcon gen-mldsa \
    gen-slhdsa gen-slhdsa-haraka gen-slhdsa-haraka-aes gen-slhdsa-sha2
DS_TEST_TARGETS := \
    test-falcon test-mldsa \
    test-slhdsa test-slhdsa-haraka test-slhdsa-haraka-aes test-slhdsa-sha2

KEM_GEN_TARGETS  := gen-mlkem gen-hqc
KEM_TEST_TARGETS := test-mlkem test-hqc

gen-ds:   $(DS_GEN_TARGETS)
test-ds:  $(DS_TEST_TARGETS)
gen-kem:  $(KEM_GEN_TARGETS)
test-kem: $(KEM_TEST_TARGETS)
.PHONY: gen-ds test-ds gen-kem test-kem

gen-additional-ds:  gen-cross gen-hawk gen-less gen-faest gen-uov gen-mayo gen-qruov gen-snova gen-sdith gen-sqisign
test-additional-ds: test-cross test-hawk test-less test-faest test-uov test-mayo test-qruov test-snova test-sdith test-sqisign
.PHONY: gen-additional-ds test-additional-ds

gen-stateful-ds:  gen-xmss gen-lms gen-hss
test-stateful-ds: test-xmss test-lms test-hss
.PHONY: gen-stateful-ds test-stateful-ds

ALL_GEN_TARGETS  := $(DS_GEN_TARGETS) $(KEM_GEN_TARGETS)
ALL_TEST_TARGETS := $(DS_TEST_TARGETS) $(KEM_TEST_TARGETS)

gen-all:  $(ALL_GEN_TARGETS)
test-all: $(ALL_TEST_TARGETS)

all: test-all

.PHONY: gen-all test-all all

# =============================================================================
#  STATIC (no file I/O) KAT TESTS -- for bare-metal / FPGA / RISC-V targets
#
#    Generates a self-contained kat/static/<variant>/ folder (main.c +
#    test_vectors_*.h with KAT data baked in as C arrays + flattened
#    src/include) instead of a binary that fopen()s a .rsp at runtime.
#    See tools/gen_static_test.py and tools/algo_registry.json for details.
#
#    Currently supported families: mlkem, mldsa, falcon, hawk, less, cross, faest, uov, mayo, qruov, snova, sdith,
#    slhdsa, slhdsa-haraka, slhdsa-haraka-aes, slhdsa-sha2, hqc2025,
#    ascon-aead, ascon-hash (see tools/algo_registry.json)
#
#    static-test-all covers only the standardized set (same scope as
#    test-all): mlkem, mldsa, falcon, all 4 slhdsa subfamilies, hqc. Use
#    static-test-additional-ds / static-test-stateful-ds / static-test-ascon
#    for CROSS/HAWK/LESS, XMSS/LMS/HSS, and ASCON respectively.
#
#    HAWK and HQC-2025 have no explicit-randomness API and replay
#    determinism by reseeding their internal DRBG/PRNG; with RUN_KEYGEN=0
#    the reseeded stream desyncs and sign/encaps will report mismatches
#    even though the underlying algorithm is correct -- see tools/README.md.
#
#  CONFIGURABLE FLAGS
#    STATIC_KATNUM = 1              Number of KAT vectors baked into the test
#    STATICDIR     = kat/static     Output directory
#    RUN_KEYGEN/RUN_SIGN/RUN_VERIFY/RUN_ENCAPS/RUN_DECAPS as above, but baked
#    in as literal #define 0/1 in the generated main.c (not compile flags).
#    PROFILING     = none           Name of a tools/profiling_commands/<name>.txt
#                                   file (e.g. cva6, xheep) to bake in as
#                                   PERF_CNT_CYCLES=1 cycle-counter hooks
#                                   around keygen/sign-or-encaps/verify-or-decaps.
#                                   "none" (default) -> PERF_CNT_CYCLES=0, hooks
#                                   present in the generated main.c but empty.
#
#  EXAMPLES
#    make static-test-ml-kem-512 STATIC_KATNUM=10
#    make static-test-mldsa
#    make static-test-all STATIC_KATNUM=5
#    make static-test-ml-kem-512 PROFILING=cva6
#    make clean-static             Remove kat/static/ to force a clean re-gen
# =============================================================================

STATIC_KATNUM ?= 1
STATICDIR     ?= $(ROOT)/kat/static
PROFILING     ?= none

STATIC_GEN := python3 $(ROOT)/tools/gen_static_test.py \
    --kat-num $(STATIC_KATNUM) --outdir $(STATICDIR) --profiling $(PROFILING) \
    --run-keygen $(RUN_KEYGEN) --run-sign $(RUN_SIGN) --run-verify $(RUN_VERIFY) \
    --run-encaps $(RUN_ENCAPS) --run-decaps $(RUN_DECAPS)

define STATIC_TEST_RULE
static-test-$(1):
	$(STATIC_GEN) --target $(1)
.PHONY: static-test-$(1)
endef

HARAKA_AES_SAFE_NAMES := HARAKAAESp128fsimple HARAKAAESp128frobust \
                         HARAKAAESp192fsimple HARAKAAESp192frobust \
                         HARAKAAESp256fsimple HARAKAAESp256frobust

STATIC_ALL_VARIANTS := \
    $(MLKEM_VARIANTS) $(MLDSA_VARIANTS) $(FALCON_VARIANTS) $(HAWK_VARIANTS) \
    $(LESS_VARIANTS) $(CROSS_VARIANTS) $(FAEST_VARIANTS) $(UOV_VARIANTS) $(MAYO_VARIANTS) $(QRUOV_VARIANTS) $(SNOVA_VARIANTS) $(SDITH_VARIANTS) $(SQISIGN_VARIANTS) $(SLHDSA_SAFE_NAMES) \
    $(HARAKA_SAFE_NAMES) $(HARAKA_AES_SAFE_NAMES) $(SHA2_SAFE_NAMES) $(HQC_SAFE_NAMES) \
    XMSS-SHA2_10_256 LMS_H10_W8 HSS_L2_H5_W8

$(foreach v,$(STATIC_ALL_VARIANTS),$(eval $(call STATIC_TEST_RULE,$(v))))

# ASCON per-variant static-test targets -- kept in a separate list since
# ASCON is neither standardized-DS/KEM nor part of static-test-all.
ASCON_STATIC_VARIANTS := $(ASCON_AEAD_VARIANTS) $(ASCON_HASH_VARIANTS)

$(foreach v,$(ASCON_STATIC_VARIANTS),$(eval $(call STATIC_TEST_RULE,$(v))))

define STATIC_FAMILY_RULE
static-test-$(1):
	$(STATIC_GEN) --target $(1)
.PHONY: static-test-$(1)
endef

$(foreach f,mlkem mldsa falcon hawk less cross faest uov mayo qruov snova sdith sqisign slhdsa slhdsa-haraka slhdsa-haraka-aes slhdsa-sha2 hqc2025 xmss lms hss ascon-aead ascon-hash,$(eval $(call STATIC_FAMILY_RULE,$(f))))

static-test-hqc: static-test-hqc2025
.PHONY: static-test-hqc

static-test-ascon: static-test-ascon-aead static-test-ascon-hash
.PHONY: static-test-ascon

# static-test-all / static-test-ds / static-test-kem cover only the
# *standardized* algorithms, mirroring test-all/test-ds/test-kem above.
# Additional-DS, Stateful-DS, and ASCON have their own aggregates and are
# never part of static-test-all.
static-test-ds: $(addprefix static-test-,falcon mldsa \
    slhdsa slhdsa-haraka slhdsa-haraka-aes slhdsa-sha2)
static-test-kem: $(addprefix static-test-,mlkem hqc2025)
.PHONY: static-test-ds static-test-kem

static-test-additional-ds: $(addprefix static-test-,hawk less cross faest uov mayo qruov snova sdith sqisign)
static-test-stateful-ds: $(addprefix static-test-,xmss lms hss)
.PHONY: static-test-additional-ds static-test-stateful-ds

static-test-all: static-test-ds static-test-kem

.PHONY: static-test-all

# =============================================================================
#  Utility targets
# =============================================================================

clean:
	rm -rf $(BUILD)

clean-rsp:
	rm -rf $(RSPDIR)

clean-static:
	rm -rf $(STATICDIR)

distclean: clean clean-rsp clean-static

help:
	@echo "PQC KAT Test Suite"
	@echo ""
	@echo "Main targets (standardized algorithms only -- ML-KEM, ML-DSA, SLH-DSA,"
	@echo "FALCON, HQC):"
	@echo "  make gen-all          Generate all .rsp KAT files"
	@echo "  make test-all         Run all KAT tests"
	@echo "  make clean            Remove build binaries"
	@echo "  make clean-rsp        Remove generated .rsp files"
	@echo "  make clean-static     Remove kat/static/ (generated static KAT tests)"
	@echo "  make distclean        Remove build/, kat/rsp/ and kat/static/"
	@echo ""
	@echo "Non-standardized / separate-category targets (NOT part of test-all):"
	@echo "  make test-additional-ds   CROSS, HAWK, LESS, FAEST, UOV, MAYO, QRUOV, SNOVA, SDITH, SQISIGN (candidate signature schemes)"
	@echo "  make test-stateful-ds     XMSS, LMS, HSS (NIST SP 800-208, default variant each)"
	@echo "  make test-ascon           ASCON AEAD + hash/XOF (not PQC)"
	@echo "  (each also has gen-* and static-test-* equivalents)"
	@echo ""
	@echo "Static (no file I/O) KAT test targets, e.g. for FPGA/RISC-V:"
	@echo "  make static-test-all               Standardized families, all variants"
	@echo "  make static-test-mlkem             One family"
	@echo "  make static-test-ml-kem-512        One variant"
	@echo "  make static-test-ascon             ASCON, all 7 variants"
	@echo "  STATIC_KATNUM=N to set vector count, PROFILING=cva6|xheep|... for cycle counters"
	@echo "  See tools/README.md for details."
	@echo ""
	@echo "Per-family targets (prefix with gen- or test-):"
	@echo "  ds kem                                          (standardized aggregates)"
	@echo "  falcon mldsa slhdsa slhdsa-haraka slhdsa-haraka-aes slhdsa-sha2"
	@echo "  mlkem hqc"
	@echo "  additional-ds cross hawk less faest uov mayo qruov snova sdith sqisign (not standardized)"
	@echo "  stateful-ds xmss lms hss                         (NIST SP 800-208)"
	@echo "  ascon ascon-aead ascon-hash                      (not PQC)"
	@echo ""
	@echo "Per-variant examples:"
	@echo "  make test-CROSS_1_RSDP_b"
	@echo "  make test-hawk-256"
	@echo "  make test-UOV_Ip"
	@echo "  make test-MAYO_1"
	@echo "  make test-QRUOV_1_q127L3v156m54_a"
	@echo "  make test-SNOVA_28_5_19_4"
	@echo "  make test-SPHINCSp128fsimple"
	@echo "  make test-ML-DSA-2"
	@echo "  make test-HQC2025_1"
	@echo "  make test-ascon128v12"
	@echo "  make test-asconhashv12"
	@echo ""
	@echo "Configuration (current values):"
	@echo "  KATNUM=$(KATNUM)  RUN_KEYGEN=$(RUN_KEYGEN)  RUN_SIGN=$(RUN_SIGN)  RUN_VERIFY=$(RUN_VERIFY)"
	@echo "  RUN_ENCAPS=$(RUN_ENCAPS)  RUN_DECAPS=$(RUN_DECAPS)  DEBUG=$(DEBUG)  MLEN=$(MLEN)"
	@echo "  CC=$(CC)"

.PHONY: clean clean-rsp clean-static distclean help
