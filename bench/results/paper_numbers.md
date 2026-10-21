# Number -> CSV cell map for paper_paragraph.md / paper_table.tex (FAEST v3 run, 2026-10-14)

M = `bench/results_median3/results.csv` (per-metric median of runs 1-3 = `bench/results/`, `bench/results_run2/`, `bench/results_run3/`).
S = `bench/results_median3/spread.csv` ((max-min)/median per metric). Derived numbers are ratios of M cells, computed with a short script over M
(re-checked by hand for the sign ratios). Table cells are copied programmatically by `tools/bench/make_paper_table.py` (rows: `paper_table_cells.md`).
Peak mem = max over ops of heap_<op>_KiB + stack_<op>_KiB (same M row). Old FAEST-v2 numbers: `bench/old_v2_faest_runs/`.

| Claim | Value | Source |
|---|---|---|
| FAEST pk 32 B smallest; pk+sk 64 B (next MQOM-gf2-shorter 52+72=124) | 32 / 64 | M[FAEST-*].pk_B, sk_B |
| only MAYO sk smaller (24 B) | 24 | M[MAYO-1/2].sk_B |
| FAEST sig 3466 to 5170 B | | M[FAEST-EM-128s].sig_B / M[FAEST-128f].sig_B |
| MQOM 2492-3316; SDitH 3721-4914; others <= 541 | | sig_B of those families (max 541 = QR-UOV-1q127L10) |
| FAEST keygen 0.023-0.032 ms | 0.0233-0.0315 | M[FAEST-*].keygen_ms |
| SDitH keygen 0.032-0.038 | 0.0322-0.0378 | M[SDitH-*].keygen_ms |
| others >= 0.49 ms; "at least 15x less" | 0.4864/0.0315 = 15.4 | M[MQOM-gf16-fast-ct].keygen_ms vs M[FAEST-128s].keygen_ms |
| FAEST-EM-128f sign 505 / verify 580 ms | 504.99 / 580.44 | M[FAEST-EM-128f] |
| vs SDitH-fast sign 16x; SDitH-fast-cipherpow verify 21x | 504.99/31.58=15.99; 580.44/27.86=20.83 | M[SDitH-*] |
| vs SQIsign sign 8.5x / verify 78x | 504.99/59.50=8.49; 580.44/7.423=78.2 | M[SQIsign-p324-3] |
| 80 to 2000x sign vs best MQOM/SNOVA/QR-UOV/MAYO/UOV | 80.2 (MQOM-gf16-fast-ct), 89.9 (SNOVA-I-B-AES), 114.2 (QR-UOV-1q127L3-shake), 421 (MAYO-2), 2003 (UOV-Is-pkc) | sign_ms |
| s variants 4.2-6.3 s sign | 4154 / 6306 ms | M[FAEST-EM-128s], M[FAEST-128s].sign_ms |
| sign peak 271-2712 KiB; f variants 271-345 | 270.6, 344.8, 2182.1, 2712.0 | table Peak mem |
| s sig smaller by 17-21%; ~8x time | 3466/4170=0.83, 4066/5170=0.79; 8.23, 8.26 | M sig_B, sign_ms |
| EM vs non-EM: 505 vs 763 ms, 4170 vs 5170 B | | M[FAEST-EM-128f], M[FAEST-128f] |
| SQIsign keygen 20 ms | 20.03 | M[SQIsign-p324-3].keygen_ms |
| FAEST timing spread up to 56%, median 34% | 55.9 / 34.4 | S, FAEST rows, keygen/sign/verify ms |
| all-variant timing spread median 31%, max 80% | 31.4 / 79.7 | S, metrics keygen_ms/sign_ms/verify_ms (also in table caption, computed) |
| UOV-Is-pkc sig 96 B, pk 66576 B | | M[UOV-Is-pkc] |
| sizes/heap/stack/text identical in all 3 runs | 0 non-identical | S identical_in_all_runs (checked) |
