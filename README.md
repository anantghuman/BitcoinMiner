# Bitcoin Miner in C++

An educational CPU and multi-GPU Bitcoin miner written in C++ and CUDA that connects to the live Bitcoin network, fetches the current chain tip, and attempts to mine the next block.

## Features

- **Blockstream.info API**: Fetches real-time chain tip data (no API key required)
- **Multi-threaded CPU mining**: Splits the nonce space across all available CPU cores using `std::thread`
- **Multi-GPU CUDA mining**: Scales dynamically across all detected NVIDIA GPUs (or a user-selected subset via `--gpus`) using an atomic work queue
- **SHA-256 midstate optimization**: Precomputes the SHA-256 state after the first 64 bytes of the header (constant across all nonces), halving compression work per hash
- **Correct Bitcoin header serialization**: `prevHash` and `merkleRoot` byte-reversed to wire format; nBits expanded to 32-byte big-endian target
- **Live progress reporting**: Prints aggregate and per-GPU hash rate and total hashes checked every 5 seconds

## Project Structure

```
include/
  miner.hpp               — MineResult / GpuDeviceStats structs + CPU mine() declaration
  miner_gpu.hpp           — GPU mineGPU() and printGpuDevices() declarations
  work_queue.hpp          — Thread-safe dynamic batch scheduler + GPU ID resolver
  cli.hpp                 — CLI option parsing declarations
  blockchain.hpp          — BlockInfo struct + API declarations
  sha256_constants.hpp    — SHA-256 init values and round constants
src/
  miner_cpu.cpp           — SHA-256 implementation + multi-threaded CPU mining loop
  miner_gpu.cu            — CUDA SHA-256 kernel + multi-GPU mining coordinator
  cli.cpp                 — Command-line option parser (--gpus, --list-gpus, --help)
  blockchain.cpp          — Blockstream.info API client
  main.cpp                — Header serialization, target expansion, entry point
tests/
  test_multi_gpu.cpp      — Host-side unit tests for scheduler, device selection, and CLI
third_party/
  json.hpp                — nlohmann/json v3.11.3 (vendored, no install needed)
build/                    — object files (git-ignored, created by make)
```

## Dependencies

Only `libcurl` is required for CPU builds — `nlohmann/json` is vendored in `third_party/`. GPU builds additionally require the [CUDA Toolkit](https://developer.nvidia.com/cuda-downloads) (tested with CUDA 11+).

### Ubuntu/Debian
```bash
sudo apt-get install libcurl4-openssl-dev
```

### macOS (using Homebrew)
```bash
brew install curl
```

### CentOS/RHEL
```bash
sudo yum install libcurl-devel
```

## Building and Running

```bash
# Clone the repository
git clone https://github.com/anantghuman/BitcoinMiner.git
cd BitcoinMiner

# ── CPU build (default) ──────────────────────────────────────────────────────
make              # outputs ./bitcoin_miner
make run          # build + run

# ── GPU build (requires CUDA Toolkit + NVIDIA GPU(s)) ────────────────────────
make GPU=1                    # outputs ./bitcoin_miner_gpu (uses all detected GPUs)
make run GPU=1                # build + run on all GPUs
./bitcoin_miner_gpu --list-gpus     # list detected CUDA GPUs
./bitcoin_miner_gpu --gpus 0,1      # mine on specific GPU device IDs

# ── Host-side unit tests ─────────────────────────────────────────────────────
make test

# ── Clean all build artifacts ─────────────────────────────────────────────────
make clean
```

**CPU:** Compiler `g++`, Standard C++17, Flags `-Wall -Wextra -O3 -march=native`

**GPU:** Compiler `nvcc`, Default arch `sm_75` (Turing/RTX 20xx+). Edit the `-arch` flag in the Makefile for your GPU:
| Flag | Architecture |
|------|-------------|
| `sm_61` | Pascal (GTX 10xx) |
| `sm_75` | Turing (RTX 20xx) |
| `sm_86` | Ampere (RTX 30xx) |
| `sm_89` | Ada (RTX 40xx) |

## How It Works

### 1. Fetch Chain Tip
Fetches the latest block from Blockstream.info in two steps:
1. Plain-text tip hash from `/api/blocks/tip/hash`
2. Full block JSON from `/api/block/{hash}`

Fields used: `version`, `id` (prevHash), `merkle_root`, `bits`, `height`.

### 2. Build 76-Byte Header Prefix
Constructs the constant portion of the 80-byte Bitcoin block header:

| Field         | Size    | Notes                                      |
|---------------|---------|--------------------------------------------|
| version       | 4 bytes | Little-endian                              |
| prevHash      | 32 bytes| Byte-reversed from display hex (wire format)|
| merkleRoot    | 32 bytes| Byte-reversed from display hex (wire format)|
| timestamp     | 4 bytes | Current UNIX time, little-endian           |
| bits          | 4 bytes | Compact difficulty target, little-endian   |

The 4-byte nonce is appended per iteration in the mining loop.

### 3. Mining (CPU or Multi-GPU)

**CPU (`src/miner_cpu.cpp`):**
- Nonce space `[0, 2³²)` is divided evenly across all CPU threads (`std::thread::hardware_concurrency()`)
- Each thread runs its range independently; a shared `std::atomic<bool>` flag stops all threads early on success
- **Midstate optimization**: The first 64 bytes of the header are constant — their SHA-256 compression is computed once and reused across all `2³²` nonce iterations
- Progress printed every 5 seconds: elapsed time, total hashes, MH/s

**Multi-GPU (`src/miner_gpu.cu`):**
- Spawns one host worker thread per selected CUDA GPU (`cudaSetDevice(id)` + dedicated non-blocking `cudaStream_t`)
- Nonce space `[0, 2³²)` is split into 512 batches of 8M nonces (`2²³`) each and claimed dynamically via `NonceBatchScheduler`, automatically load-balancing heterogeneous GPUs
- Each batch launches `32768 blocks × 256 threads` — every thread owns exactly one nonce
- SHA-256 constants, midstate, tail bytes, and target are uploaded per device into CUDA `__constant__` memory
- `atomicCAS` safely records the winning nonce on a GPU; the winning worker notifies `NonceBatchScheduler` to stop all other GPUs after their in-flight batch
- Progress printed every 5 seconds with aggregate and per-GPU MH/s, followed by a per-GPU breakdown at completion

### 4. Difficulty Check
`SHA-256²(header)` is byte-reversed to produce the Bitcoin display-form digest, then compared lexicographically against the 32-byte target expanded from `nBits`.

## Example Output

```
=== Bitcoin Miner (Educational) ===
Mining block #893412 on top of block #893411

Difficulty target (first 12 bytes): 00000000000000000003ab...

Starting mining with 16 threads...

5.0s  42M hashes  8.50 MH/s  threads=16
10.0s  85M hashes  8.49 MH/s  threads=16
...

--- Results ---
Hashes checked : 4294967296
Throughput     : 8.51 MH/s
Exhausted all 2^32 nonces. No block found.
Real Bitcoin requires ASIC hardware.
```

## API

```cpp
#include "miner.hpp"
#include "miner_gpu.hpp"

// Searches all 2^32 nonces across all CPU threads, or across selected CUDA GPUs.
// headerPrefix: 76-byte constant header (version + prevHash + merkleRoot + timestamp + bits)
// target:       32-byte big-endian limit from expandTarget(nBits)
MineResult cpuResult = mine(headerPrefix, target);
MineResult gpuResult = mineGPU(headerPrefix, target);            // all detected GPUs
MineResult subResult = mineGPU(headerPrefix, target, {0, 1});    // specific GPUs

// MineResult fields:
gpuResult.found           // bool — true if a valid nonce was found
gpuResult.nonce           // uint32_t — the winning nonce
gpuResult.displayHash     // std::array<uint8_t,32> — byte-reversed digest (display form)
gpuResult.hashesChecked   // uint64_t — total hashes computed
gpuResult.hashesPerSecond // double — average aggregate throughput
gpuResult.gpuStats        // std::vector<GpuDeviceStats> — per-GPU stats (GPU builds)
```

## Known Limitations

- Merkle root is taken from the fetched tip block, not constructed from new transactions
- Real Bitcoin difficulty (as of 2026) requires ~9 leading zero bytes; even multiple GPUs will exhaust all 2³² nonces without finding a valid block
- Single machine only; no pool protocol (Stratum) support

## Important Notes

**Educational Purpose**: This is a simplified Bitcoin miner for educational purposes. Real Bitcoin mining requires specialized ASIC hardware, mining pool participation, and significant energy resources.

**Network Compliance**: This miner fetches real blockchain data but mining solo with a CPU or GPU is not economically viable on the main Bitcoin network.

## Troubleshooting

1. **Compilation errors**: Ensure `libcurl` is installed and your compiler supports C++17
2. **Network errors**: Verify internet access; some networks block cryptocurrency-related requests
3. **JSON parsing errors**: Usually caused by network timeouts returning empty responses — retry

## License

This project is for educational purposes. Please ensure compliance with local regulations regarding cryptocurrency mining.

## Disclaimer

This software is provided for educational and research purposes only. The authors are not responsible for any financial losses, hardware damage, or legal issues arising from its use.
