#!/usr/bin/env bash
#######################################################################################
##                                                                                   ##
## Auth: Alessandra Dolmeta, Valeria Piscopo                                         ##
## @ EDGE group, at VLSI-LAB, Politecnico di Torino                                  ##
## Date: July 2026                                                                   ##
## Desc: Golden-model static KAT batch test runner                                   ##
##                                                                                   ##
#######################################################################################

#
# Golden-model static KAT test runner.
#
# By default, "all" only covers the *standardized* families (ML-KEM, ML-DSA,
# all 4 SLH-DSA sub-families, FALCON, HQC -- same scope as `make test-all`).
# Pass --benchmark-all (or BENCHMARK_ALL=1) to additionally include one
# representative variant from each non-standardized family: Additional-DS
# (CROSS, HAWK, LESS, FAEST), Stateful-DS (XMSS, LMS, HSS -- already single-variant),
# and ASCON (ascon-aead, ascon-hash). Explicit `--targets cross` (or any
# specific family/variant name) always works regardless of --benchmark-all --
# the flag only changes what "all" expands to.
#
# For every resulting variant, this:
#   1. runs `make static-test-<variant>` (PROFILING is never passed, so it
#      always uses the Makefile's own default: PROFILING=none / PERF_CNT_CYCLES=0)
#   2. compiles the generated kat/static/<variant>/ folder with a plain host
#      compiler:  gcc -O2 -std=c11 *.c -o test
#   3. runs ./test
#   4. records PASS / FAIL / SKIP / GEN_FAIL / COMPILE_FAIL for each variant,
#      plus a final summary, into one timestamped log file.
#
# Usage:
#   tools/run_all_static_kat.sh [options]
#
# Options:
#   -k, --kat-num N      STATIC_KATNUM value                    (default: 1)
#   -m, --run-mode MODE  all | keygen | sign | verify            (default: all)
#                           all    -> RUN_KEYGEN=1 RUN_SIGN=1 RUN_VERIFY=1 RUN_ENCAPS=1 RUN_DECAPS=1
#                           keygen -> only keygen
#                           sign   -> only sign/encaps (keygen input taken from KAT vectors)
#                           verify -> only verify/decaps (sign/encaps input taken from KAT vectors)
#   -t, --targets LIST   space-separated variant and/or family names, or "all"
#                                                                 (default: all)
#   -b, --benchmark-all  when --targets is "all" (the default), also include
#                        one representative variant from each non-standardized
#                        family (Additional-DS, Stateful-DS, ASCON)
#                                                                 (default: off)
#   -l, --log FILE       log file path            (default: tools/../kat_golden_model_<timestamp>.log)
#   -c, --cc CC          compiler                                (default: gcc)
#       --cflags FLAGS   compiler flags                          (default: "-O2 -std=c11")
#       --list           list all known variant/family names and exit
#   -h, --help           show this help and exit
#
# Env var equivalents (a flag on the command line always wins):
#   KAT_NUM  RUN_MODE  TARGETS  BENCHMARK_ALL  LOGFILE  CC  CFLAGS
#
# Examples:
#   tools/run_all_static_kat.sh
#   tools/run_all_static_kat.sh --benchmark-all
#   tools/run_all_static_kat.sh --kat-num 10 --targets mlkem
#   tools/run_all_static_kat.sh --run-mode keygen --targets "ml-kem-512 ML-DSA-5 hawk"
#   tools/run_all_static_kat.sh --run-mode verify --targets all
#
# Detect sourcing (`source tools/run_all_static_kat.sh`) vs execution
# (`./tools/run_all_static_kat.sh` / `bash tools/...`). Plain `exit` inside a
# sourced script terminates the calling shell (closes your terminal); every
# early-return site below uses `return` instead in that case. This has to be
# `((IS_SOURCED)) && return N || exit N` inline at each call site rather than
# a helper function -- `return` inside a function only unwinds that function,
# not the sourced script, so a wrapper wouldn't actually stop execution.
if (return 0 2>/dev/null); then
    IS_SOURCED=1
else
    IS_SOURCED=0
fi

# Only when executed normally -- `set -uo pipefail` here would otherwise
# leak those options into the calling shell for the rest of its session.
((IS_SOURCED)) || set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

# =============================================================================
# Registry -- mirrors tools/algo_registry.json / the Makefile's
# STATIC_ALL_VARIANTS list. Keep in sync if a family/variant is added there.
# =============================================================================

declare -A FAMILIES=(
    [mlkem]="ml-kem-512 ml-kem-768 ml-kem-1024"
    [mldsa]="ML-DSA-2 ML-DSA-3 ML-DSA-5"
    [falcon]="falcon-512 falcon-1024"
    [slhdsa]="SPHINCSp128fsimple SPHINCSp128frobust SPHINCSp192fsimple SPHINCSp192frobust SPHINCSp256fsimple SPHINCSp256frobust SPHINCSp128ssimple SPHINCSp192ssimple SPHINCSp256ssimple"
    [slhdsa-haraka]="HARAKAp128fsimple HARAKAp128frobust HARAKAp192fsimple HARAKAp192frobust HARAKAp256fsimple HARAKAp256frobust"
    [slhdsa-haraka-aes]="HARAKAAESp128fsimple HARAKAAESp128frobust HARAKAAESp192fsimple HARAKAAESp192frobust HARAKAAESp256fsimple HARAKAAESp256frobust"
    [slhdsa-sha2]="SHA2p128fsimple SHA2p128frobust SHA2p192fsimple SHA2p192frobust SHA2p256fsimple SHA2p256frobust SHA2p128ssimple SHA2p192ssimple SHA2p256ssimple"
    [hqc2025]="HQC2025_1 HQC2025_3 HQC2025_5"
    # --- Non-standardized / separate-category families -- NOT part of the
    # default "all" expansion; see --benchmark-all above. ---
    [hawk]="hawk-512 hawk-1024"
    [less]="LESS_252_45 LESS_252_68 LESS_252_192 LESS_400_102 LESS_400_220 LESS_548_137 LESS_548_345"
    [cross]="CROSS_1_RSDP_b CROSS_1_RSDP_f CROSS_1_RSDP_s CROSS_1_RSDPG_b CROSS_1_RSDPG_f CROSS_1_RSDPG_s CROSS_3_RSDP_b CROSS_3_RSDP_f CROSS_3_RSDP_s CROSS_3_RSDPG_b CROSS_3_RSDPG_f CROSS_3_RSDPG_s CROSS_5_RSDP_b CROSS_5_RSDP_f CROSS_5_RSDP_s CROSS_5_RSDPG_b CROSS_5_RSDPG_f CROSS_5_RSDPG_s"
    [faest]="FAEST_128F FAEST_128S FAEST_192F FAEST_192S FAEST_256F FAEST_256S FAEST_EM_128F FAEST_EM_128S FAEST_EM_192F FAEST_EM_192S FAEST_EM_256F FAEST_EM_256S"
    # XMSS/LMS/HSS are runtime-parametrized (see original/Stateful-DS/README.md)
    # -- only their single default variant is wired into the static-test flow
    # so far, matching the dynamic flow's test-stateful-ds (not its much larger
    # -check/-check-slow sweeps, which have no static-test equivalent yet).
    [xmss]="XMSS-SHA2_10_256"
    [lms]="LMS_H10_W8"
    [hss]="HSS_L2_H5_W8"
    [ascon-aead]="ascon128v12 ascon128av12 ascon80pqv12"
    [ascon-hash]="asconhashv12 asconhashav12 asconxofv12 asconxofav12"
)

# Standardized families -- always covered by the default "all" expansion
# (same scope as `make test-all`/`make static-test-all`).
STANDARD_FAMILIES=(mlkem mldsa falcon slhdsa slhdsa-haraka slhdsa-haraka-aes slhdsa-sha2 hqc2025)

# Non-standardized / separate-category families -- only added to the "all"
# expansion when --benchmark-all is set (one representative variant each).
EXTRA_FAMILIES=(cross hawk less faest xmss lms hss ascon-aead ascon-hash)

# One representative variant per EXTRA_FAMILIES entry, used by --benchmark-all.
declare -A REPRESENTATIVE=(
    [cross]="CROSS_1_RSDP_b"
    [hawk]="hawk-512"
    [less]="LESS_252_45"
    [faest]="FAEST_128F"
    [xmss]="XMSS-SHA2_10_256"
    [lms]="LMS_H10_W8"
    [hss]="HSS_L2_H5_W8"
    [ascon-aead]="ascon128v12"
    [ascon-hash]="asconhashv12"
)

# Full family list, standardized + extra -- used for --list and for
# resolving explicit --targets names (which work regardless of --benchmark-all).
ORDERED_FAMILIES=("${STANDARD_FAMILIES[@]}" "${EXTRA_FAMILIES[@]}")

# Families whose RUN_KEYGEN=0 (sign/verify-only run modes) desyncs an internal
# DRBG/PRNG and reports false mismatches -- see tools/README.md, "The
# RUN_KEYGEN=0 desync caveat".
DESYNC_FAMILIES=(hqc2025)

declare -A VARIANT_SET=()
for f in "${ORDERED_FAMILIES[@]}"; do
    for v in ${FAMILIES[$f]}; do VARIANT_SET["$v"]=1; done
done

is_known_variant() { [[ -n "${VARIANT_SET[$1]+set}" ]]; }

variant_family() {
    local variant="$1" f
    for f in "${ORDERED_FAMILIES[@]}"; do
        if [[ " ${FAMILIES[$f]} " == *" $variant "* ]]; then
            echo "$f"
            return
        fi
    done
    echo "?"
}

list_targets() {
    echo "Families:"
    for f in "${ORDERED_FAMILIES[@]}"; do
        echo "  $f:"
        for v in ${FAMILIES[$f]}; do echo "    $v"; done
    done
}

# =============================================================================
# Config (env vars, overridable by flags below)
# =============================================================================

KAT_NUM="${KAT_NUM:-1}"
RUN_MODE="${RUN_MODE:-all}"
TARGETS="${TARGETS:-all}"
BENCHMARK_ALL="${BENCHMARK_ALL:-0}"
LOGFILE="${LOGFILE:-}"
CC="${CC:-gcc}"
CFLAGS="${CFLAGS:--O2 -std=c11}"

print_help() {
    sed -n '2,53p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -k|--kat-num) KAT_NUM="$2"; shift 2 ;;
        -m|--run-mode) RUN_MODE="$2"; shift 2 ;;
        -t|--targets) TARGETS="$2"; shift 2 ;;
        -b|--benchmark-all) BENCHMARK_ALL=1; shift ;;
        -l|--log) LOGFILE="$2"; shift 2 ;;
        -c|--cc) CC="$2"; shift 2 ;;
        --cflags) CFLAGS="$2"; shift 2 ;;
        --list) list_targets; ((IS_SOURCED)) && return 0 || exit 0 ;;
        -h|--help) print_help; ((IS_SOURCED)) && return 0 || exit 0 ;;
        *) echo "error: unknown option '$1' (see --help)" >&2
           ((IS_SOURCED)) && return 1 || exit 1 ;;
    esac
done

case "$RUN_MODE" in
    all)
        RUN_KEYGEN=1; RUN_SIGN=1; RUN_VERIFY=1; RUN_ENCAPS=1; RUN_DECAPS=1 ;;
    keygen)
        RUN_KEYGEN=1; RUN_SIGN=0; RUN_VERIFY=0; RUN_ENCAPS=0; RUN_DECAPS=0 ;;
    sign)
        RUN_KEYGEN=0; RUN_SIGN=1; RUN_VERIFY=0; RUN_ENCAPS=1; RUN_DECAPS=0 ;;
    verify)
        RUN_KEYGEN=0; RUN_SIGN=0; RUN_VERIFY=1; RUN_ENCAPS=0; RUN_DECAPS=1 ;;
    *)
        echo "error: unknown --run-mode '$RUN_MODE' (expected: all|keygen|sign|verify)" >&2
        ((IS_SOURCED)) && return 1 || exit 1 ;;
esac

[[ -z "$LOGFILE" ]] && LOGFILE="$ROOT/kat_golden_model_$(date +%Y%m%d_%H%M%S).log"

# Expand --targets into the flat list of variants to actually run.
declare -a TARGET_LIST=()
if [[ "$TARGETS" == "all" ]]; then
    for f in "${STANDARD_FAMILIES[@]}"; do
        for v in ${FAMILIES[$f]}; do TARGET_LIST+=("$v"); done
    done
    if [[ "$BENCHMARK_ALL" == "1" ]]; then
        for f in "${EXTRA_FAMILIES[@]}"; do
            TARGET_LIST+=("${REPRESENTATIVE[$f]}")
        done
    fi
else
    for tok in $TARGETS; do
        if [[ -n "${FAMILIES[$tok]+set}" ]]; then
            for v in ${FAMILIES[$tok]}; do TARGET_LIST+=("$v"); done
        elif is_known_variant "$tok"; then
            TARGET_LIST+=("$tok")
        else
            echo "error: unknown target '$tok' (not a known family or variant; use --list to see valid names)" >&2
            ((IS_SOURCED)) && return 1 || exit 1
        fi
    done
fi

if [[ ${#TARGET_LIST[@]} -eq 0 ]]; then
    echo "error: target list is empty" >&2
    ((IS_SOURCED)) && return 1 || exit 1
fi

# =============================================================================
# Run
# =============================================================================

log_block() {
    # $1=variant $2=family $3=status $4=elapsed(s) $5=summary $6=detail(optional)
    local variant="$1" family="$2" status="$3" elapsed="$4" summary="$5" detail="${6:-}"
    local line
    line="$(printf '[%-12s] %-24s (%-18s) %6ss  %s' "$status" "$variant" "$family" "$elapsed" "$summary")"
    echo "$line" >> "$LOGFILE"
    echo "$line"
    if [[ -n "$detail" ]]; then
        {
            echo "    ---- detail (last 60 lines) ----"
            printf '%s\n' "$detail" | tail -n 60 | sed 's/^/    /'
            echo "    ---------------------------------"
        } >> "$LOGFILE"
    fi
}

{
    echo "================================================================================"
    echo "Golden-model static KAT test run"
    echo "Started : $(date -Iseconds)"
    echo "Host    : $(uname -a)"
    echo "Root    : $ROOT"
    echo "Config  : KAT_NUM=$KAT_NUM RUN_MODE=$RUN_MODE BENCHMARK_ALL=$BENCHMARK_ALL PROFILING=none (never overridden)"
    echo "          RUN_KEYGEN=$RUN_KEYGEN RUN_SIGN=$RUN_SIGN RUN_VERIFY=$RUN_VERIFY RUN_ENCAPS=$RUN_ENCAPS RUN_DECAPS=$RUN_DECAPS"
    echo "Compiler: $CC $CFLAGS *.c -o test"
    echo "Targets : ${#TARGET_LIST[@]} requested (--targets '$TARGETS')"
    echo "================================================================================"
} | tee "$LOGFILE"

if [[ "$RUN_MODE" == "sign" || "$RUN_MODE" == "verify" ]]; then
    {
        echo
        echo "NOTE: hqc2025 reseeds an internal DRBG/PRNG instead of using an"
        echo "explicit-randomness API. With RUN_KEYGEN=0 (this run mode) their"
        echo "sign/encaps stage will report a mismatch even though the algorithm itself is"
        echo "correct -- see tools/README.md, \"The RUN_KEYGEN=0 desync caveat\". Any FAIL"
        echo "reported below for these families should be read with that in mind."
        echo
    } | tee -a "$LOGFILE"
fi

declare -A COUNT=([PASS]=0 [FAIL]=0 [SKIP]=0 [GEN_FAIL]=0 [COMPILE_FAIL]=0)
declare -a FAILED_LIST=()
declare -a NOTED_LIST=()

START_TS=$(date +%s)

for variant in "${TARGET_LIST[@]}"; do
    family="$(variant_family "$variant")"
    vdir="$ROOT/kat/static/$variant"
    variant_t0=$(date +%s.%N)
    t0="$variant_t0"

    gen_out=$(make -C "$ROOT" "static-test-$variant" \
        STATIC_KATNUM="$KAT_NUM" \
        RUN_KEYGEN="$RUN_KEYGEN" RUN_SIGN="$RUN_SIGN" RUN_VERIFY="$RUN_VERIFY" \
        RUN_ENCAPS="$RUN_ENCAPS" RUN_DECAPS="$RUN_DECAPS" 2>&1)
    gen_rc=$?
    elapsed=$(awk -v a="$t0" -v b="$(date +%s.%N)" 'BEGIN{d=b-a; if (d<0) d=0; printf "%.2f", d}')

    if [[ $gen_rc -ne 0 ]]; then
        COUNT[GEN_FAIL]=$((COUNT[GEN_FAIL]+1))
        FAILED_LIST+=("$variant (gen)")
        log_block "$variant" "$family" "GEN_FAIL" "$elapsed" "make static-test-$variant exited $gen_rc" "$gen_out"
        continue
    fi

    if [[ ! -f "$vdir/main.c" ]]; then
        # e.g. slhdsa-haraka-aes on non-RISC-V hosts: make exits 0 (prints its
        # own SKIP line) but never produces kat/static/<variant>/.
        reason=$(grep -m1 "SKIP:" <<<"$gen_out")
        [[ -z "$reason" ]] && reason="no kat/static/$variant/main.c produced (unknown reason)"
        COUNT[SKIP]=$((COUNT[SKIP]+1))
        log_block "$variant" "$family" "SKIP" "$elapsed" "$reason" "$gen_out"
        continue
    fi

    t0=$(date +%s.%N)
    compile_out=$(cd "$vdir" && "$CC" $CFLAGS *.c -o test 2>&1)
    compile_rc=$?
    elapsed=$(awk -v a="$t0" -v b="$(date +%s.%N)" 'BEGIN{d=b-a; if (d<0) d=0; printf "%.2f", d}')
    if [[ $compile_rc -ne 0 ]]; then
        COUNT[COMPILE_FAIL]=$((COUNT[COMPILE_FAIL]+1))
        FAILED_LIST+=("$variant (compile)")
        log_block "$variant" "$family" "COMPILE_FAIL" "$elapsed" "$CC $CFLAGS *.c -o test exited $compile_rc" "$compile_out"
        continue
    fi

    t0=$(date +%s.%N)
    run_out=$(cd "$vdir" && ./test 2>&1)
    run_rc=$?
    now=$(date +%s.%N)
    run_elapsed=$(awk -v a="$t0" -v b="$now" 'BEGIN{d=b-a; if (d<0) d=0; printf "%.2f", d}')
    total_elapsed=$(awk -v a="$variant_t0" -v b="$now" 'BEGIN{d=b-a; if (d<0) d=0; printf "%.2f", d}')
    summary=$(grep -m1 -E "Test (Successful|FAILED)" <<<"$run_out")
    [[ -z "$summary" ]] && summary="(no summary line -- test exited $run_rc)"
    errors=$(grep -E "ERROR:" <<<"$run_out")

    if [[ $run_rc -eq 0 ]]; then
        COUNT[PASS]=$((COUNT[PASS]+1))
        log_block "$variant" "$family" "PASS" "$total_elapsed" "$summary (gen+compile+run, test itself: ${run_elapsed}s)"
    else
        COUNT[FAIL]=$((COUNT[FAIL]+1))
        FAILED_LIST+=("$variant (run)")
        for df in "${DESYNC_FAMILIES[@]}"; do
            [[ "$df" == "$family" ]] && NOTED_LIST+=("$variant")
        done
        detail="$errors"
        [[ -z "$detail" ]] && detail="$run_out"
        log_block "$variant" "$family" "FAIL" "$total_elapsed" "$summary" "$detail"
    fi
done

END_TS=$(date +%s)
TOTAL=${#TARGET_LIST[@]}

{
    echo "================================================================================"
    echo "SUMMARY"
    echo "  total        : $TOTAL"
    echo "  pass         : ${COUNT[PASS]}"
    echo "  fail         : ${COUNT[FAIL]}"
    echo "  skip         : ${COUNT[SKIP]}"
    echo "  gen_fail     : ${COUNT[GEN_FAIL]}"
    echo "  compile_fail : ${COUNT[COMPILE_FAIL]}"
    echo "  elapsed      : $((END_TS-START_TS))s"
    if [[ ${#FAILED_LIST[@]} -gt 0 ]]; then
        echo "  failed targets:"
        for f in "${FAILED_LIST[@]}"; do echo "    - $f"; done
    fi
    if [[ ${#NOTED_LIST[@]} -gt 0 ]]; then
        echo "  NOTE: the following failures are in the known RUN_KEYGEN=0 desync families"
        echo "        (hqc2025) and may not indicate a real algorithm bug:"
        for f in "${NOTED_LIST[@]}"; do echo "    - $f"; done
    fi
    echo "Log file: $LOGFILE"
    echo "================================================================================"
} | tee -a "$LOGFILE"

((IS_SOURCED)) && return 0 || exit 0
