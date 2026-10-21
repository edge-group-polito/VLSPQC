# HAWK — upstream provenance and license

| Item | Value |
|---|---|
| Upstream | https://github.com/hawk-sign/dev (the NIST submission repository; no tags) |
| Closest upstream state | branch head `main`, commit `1b9fef52559273fe7b40fe3e22968eaedd3a4c2a` (2025-07-04); 19 of 31 distinct `.c` / `.h` files byte-identical |
| License | MIT, Copyright (c) 2022 Hawk Project ([`LICENSE`](LICENSE), copied from upstream). The NIST-derived DRBG files keep the NIST notice in their headers ("The software developed by NIST employees is not subject to copyright protection within the United States") |
| NIST status | round-3 candidate; **NIST states the submission team has withdrawn HAWK**, see [`../README.md`](../README.md) |

## How the pin was found
The repository did not record where this code came from. The pin above is the **closest upstream state found**, not a verified import point: every distinct `.c` / `.h` file of this
folder was hashed and compared with the files of each upstream tag (and the current branch head) and the best match is reported. The code here is a port with local changes
(see the family section of the category README), so a perfect match is not expected. Looked up 2026-10-21.
