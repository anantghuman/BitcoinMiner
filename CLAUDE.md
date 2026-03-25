# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Install dependency (Ubuntu/Debian)
sudo apt-get install libcurl4-openssl-dev

# Build (outputs ./bitcoin_miner)
make

# Build and run
make run

# Clean build artifacts
make clean
```

Compiler: `g++`, Standard: C++17, Flags: `-Wall -Wextra -O2`, Output: `./bitcoin_miner`

`nlohmann/json` is vendored as `third_party/json.hpp` — no install required.

## Repository Layout

```
include/
  sha256.hpp              — public sha256() declaration
  sha256_constants.hpp    — SHA-256 init values and round constants
  blockchain.hpp          — BlockInfo struct + public API declarations
src/
  sha256.cpp              — SHA-256 implementation (byte/word level, FIPS 180-4)
  blockchain.cpp          — Blockstream.info API client + hexStringToBytes
  main.cpp                — mining loop, header serialisation, target checking
third_party/
  json.hpp                — nlohmann/json v3.11.3 (header-only, vendored)
build/                    — object files (git-ignored, created by make)
```

## Architecture

Educational CPU-based Bitcoin miner that mines against the real live network.

**Data flow:**
```
Blockstream.info API → BlockInfo → 76-byte header prefix → mining loop
                                                              │
                                          nonce 0..2³²─1 ──►│
                                                              ▼
                                              SHA-256(SHA-256(header+nonce))
                                                              │
                                               byte-reverse digest
                                                              │
                                              compare vs expandTarget(nBits)
```

**`src/sha256.cpp`** — Pure byte/word-level SHA-256 (FIPS 180-4 §6.2.2). No string-of-bits conversion. Internal helpers (`sigma0`, `sigma1`, `Sigma0`, `Sigma1`, `ch`, `maj`) live in an anonymous namespace. Called twice per nonce in `doubleSHA256`.

**`src/blockchain.cpp`** — Two-step Blockstream fetch: plain-text tip hash from `/api/blocks/tip/hash`, then full JSON from `/api/block/{hash}`. Internal helpers (`httpGet`, `curlWriteCallback`, `parseBlock`, `fetchChainTip`) are in an anonymous namespace. `hexStringToBytes` decodes hex without per-byte `substr` allocation.

**`src/main.cpp`** — Builds the 76-byte header prefix once (version + prevHash LE + merkleRoot LE + timestamp + bits), then appends the 4-byte nonce per iteration. `expandTarget(nBits)` decodes the compact difficulty field into a 32-byte big-endian target. `meetsTarget` does a byte-by-byte lexicographic comparison.

## Key Implementation Details

- **Byte ordering:** Bitcoin stores `prevHash` and `merkleRoot` in wire format as byte-reversed (little-endian) relative to their display hex. `appendHashLE` handles this reversal.
- **Difficulty comparison:** `SHA-256²(header)` is byte-reversed after computation to produce the display form; this reversed digest is then compared numerically against `expandTarget(nBits)`.
- **Version:** Uses the `version` field fetched from the chain tip (typically `0x2xxxxxxx` per BIP9), not a hardcoded value.
- **Nonce loop:** `uint64_t` counter over `[0, UINT32_MAX]` to avoid unsigned overflow; inner `appendLE32` casts to `uint32_t`.

## Known Limitations

- Merkle root is taken from the fetched tip block, not constructed from new transactions.
- Real Bitcoin difficulty as of 2026 requires ≈ 9 leading zero bytes; a CPU miner will exhaust all 2³² nonces without finding a valid block.
- Single-threaded; no GPU/ASIC support.
