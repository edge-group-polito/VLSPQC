# Legacy trees (Kyber, Dilithium, SPHINCS+ round 3) — upstream provenance and license

| Item | Value |
|---|---|
| Kyber | https://github.com/pq-crystals/kyber — closest state: branch `main` head, commit `3edd5af5991927164edd4aacebfcbee00b8064e7`; 20 of 34 distinct `.c` / `.h` files byte-identical. License: public domain (CC0) or Apache-2.0 ([`Kyber-round3/LICENSE`](Kyber-round3/LICENSE)) |
| Dilithium | https://github.com/pq-crystals/dilithium — closest state: branch `master` head, commit `d35ba3fe5449bee3e6d43e1f296c3ca818bd36be`; 18 of 34 files byte-identical. License: public domain (CC0), Apache-2.0 or GPL-2.0 ([`Dilithium-round3/LICENSE`](Dilithium-round3/LICENSE)) |
| SPHINCS+ (f sizes, Haraka, Haraka-AES) | https://github.com/sphincs/sphincsplus — closest state: branch `master` head, commit `7ec789ace6874d875f4bb84cb61b81155398167e`; 24 of 67 distinct files identical in the SHAKE tree, 23 of 40 in the Haraka tree. License: `(LicenseRef-SPHINCS-PLUS-Public-Domain OR CC0-1.0 OR 0BSD OR MIT-0) AND MIT` (`LICENSE` and `LICENSES/` in each SPHINCS+ folder, copied from upstream) |
| SPHINCS+ `*-s-simple` (SHAKE and SHA2 trees) | these variants contain an early FIPS 205 (ipd) implementation by Markku-Juhani O. Saarinen whose headers say "See LICENSE". Its original license text is **not in this tree and could not be retrieved** (his original repository is not reachable under its former name; the code was later donated to [slhdsa-c](https://github.com/pq-code-package/slhdsa-c) under ISC, see the first commit `ddbf57e`, 2025-06-12). The license of those files is therefore unconfirmed; the folder is kept only as a comparison reference |

## How the pin was found
The repository did not record where this code came from. The pin above is the **closest upstream state found**, not a verified import point: every distinct `.c` / `.h` file of this
folder was hashed and compared with the files of each upstream tag (and the current branch head) and the best match is reported. The code here is a port with local changes
(see the family section of the category README), so a perfect match is not expected. Looked up 2026-10-21.

The Haraka and Haraka-AES folders are live legacy code (they have `make` targets); the Kyber, Dilithium and SPHINCS+ round-3 folders are unwired copies of the tree at tag `pre-fips-migration`.
