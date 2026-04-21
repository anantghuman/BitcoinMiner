#include "miner.hpp"
#include "sha256_constants.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

namespace {

inline uint32_t rotr(uint32_t x, uint32_t n) noexcept { return (x >> n) | (x << (32u - n)); }

inline uint32_t sigma0(uint32_t x) noexcept { return rotr(x,  7) ^ rotr(x, 18) ^ (x >>  3); }
inline uint32_t sigma1(uint32_t x) noexcept { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }
inline uint32_t Sigma0(uint32_t x) noexcept { return rotr(x,  2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint32_t Sigma1(uint32_t x) noexcept { return rotr(x,  6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline uint32_t ch (uint32_t e, uint32_t f, uint32_t g) noexcept { return (e & f) ^ (~e & g); }
inline uint32_t maj(uint32_t a, uint32_t b, uint32_t c) noexcept { return (a & b) ^ (a & c) ^ (b & c); }

inline uint32_t loadBE32(const uint8_t* p) noexcept {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

// Single SHA-256 compression round: updates h[8] in-place given a 64-byte block.
// All buffers are on the caller's stack — zero heap involvement.
void sha256Compress(uint32_t h[8], const uint8_t blk[64]) noexcept {
    uint32_t w[64];
    for (int i = 0;  i < 16; ++i) w[i] = loadBE32(blk + i * 4);
    for (int i = 16; i < 64; ++i)
        w[i] = sigma1(w[i-2]) + w[i-7] + sigma0(w[i-15]) + w[i-16];

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

    for (int i = 0; i < 64; ++i) {
        const uint32_t T1 = hh + Sigma1(e) + ch(e,f,g) + SHA256_K[i] + w[i];
        const uint32_t T2 = Sigma0(a) + maj(a,b,c);
        hh = g; g = f; f = e; e = d + T1;
        d  = c; c = b; b = a; a = T1 + T2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
    h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
}

// Precomputes the SHA-256 state after the first 64-byte block of the header.
// The Bitcoin header is 80 bytes; bytes 0-63 (version + prevHash + merkleRoot[0:28])
// are constant across all nonces — compress them once and reuse the midstate.
void computeMidstate(const uint8_t prefix76[76], uint32_t midstate[8]) noexcept {
    for (int i = 0; i < 8; ++i) midstate[i] = SHA256_INIT[i];
    sha256Compress(midstate, prefix76);  // first 64 bytes of prefix
}

// Computes SHA-256²(header) for a given nonce starting from a precomputed midstate.
// out[32] is the natural (non-reversed) double-SHA256 digest.
void doubleSHA256Header(const uint32_t midstate[8], const uint8_t tail12[12],
                        uint32_t nonce, uint8_t out[32]) noexcept {
    // --- First SHA-256, second (and final) 64-byte block ---
    uint8_t blk1[64];
    std::memset(blk1, 0, sizeof(blk1));
    std::memcpy(blk1, tail12, 12);
    blk1[12] = static_cast<uint8_t>(nonce);
    blk1[13] = static_cast<uint8_t>(nonce >>  8);
    blk1[14] = static_cast<uint8_t>(nonce >> 16);
    blk1[15] = static_cast<uint8_t>(nonce >> 24);
    blk1[16] = 0x80;               // SHA-256 length padding marker
    blk1[62] = 0x02; blk1[63] = 0x80;  // 80 * 8 = 640 = 0x0000000000000280

    uint32_t h1[8];
    std::memcpy(h1, midstate, 32);
    sha256Compress(h1, blk1);

    // Serialise first digest as big-endian input to second SHA-256
    uint8_t blk2[64];
    std::memset(blk2, 0, sizeof(blk2));
    for (int i = 0; i < 8; ++i) {
        blk2[4*i]   = static_cast<uint8_t>(h1[i] >> 24);
        blk2[4*i+1] = static_cast<uint8_t>(h1[i] >> 16);
        blk2[4*i+2] = static_cast<uint8_t>(h1[i] >>  8);
        blk2[4*i+3] = static_cast<uint8_t>(h1[i]);
    }
    blk2[32] = 0x80;               // SHA-256 length padding marker
    blk2[62] = 0x01; blk2[63] = 0x00;  // 32 * 8 = 256 = 0x0000000000000100

    uint32_t h2[8];
    for (int i = 0; i < 8; ++i) h2[i] = SHA256_INIT[i];
    sha256Compress(h2, blk2);

    for (int i = 0; i < 8; ++i) {
        out[4*i]   = static_cast<uint8_t>(h2[i] >> 24);
        out[4*i+1] = static_cast<uint8_t>(h2[i] >> 16);
        out[4*i+2] = static_cast<uint8_t>(h2[i] >>  8);
        out[4*i+3] = static_cast<uint8_t>(h2[i]);
    }
}

// Bitcoin displays hashes byte-reversed; the target is in that display order.
// digest[32] is in natural order → reversed digest[31-i] compared to target[i].
bool hashMeetsTarget(const uint8_t digest[32],
                     const std::array<uint8_t, 32>& target) noexcept {
    for (int i = 0; i < 32; ++i) {
        const uint8_t d = digest[31 - i];
        if (d < target[i]) return true;
        if (d > target[i]) return false;
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

MineResult mine(const std::string& headerPrefix,
                const std::array<uint8_t, 32>& target) {
    // Precompute midstate from the first 64 bytes (constant across all nonces).
    const auto* prefix = reinterpret_cast<const uint8_t*>(headerPrefix.data());
    uint32_t midstate[8];
    computeMidstate(prefix, midstate);

    // Bytes 64-75 of the prefix: merkleRoot[28:32] + timestamp + bits.
    uint8_t tail12[12];
    std::memcpy(tail12, prefix + 64, 12);

    // Thread coordination — all reads/writes in workers are relaxed
    std::atomic<bool>     foundFlag{false};
    std::atomic<uint32_t> foundNonce{0};
    std::atomic<uint64_t> hashesChecked{0};
    std::atomic<int>      activeWorkers{0};

    const int      numThreads = static_cast<int>(std::thread::hardware_concurrency());
    const uint64_t total      = uint64_t(UINT32_MAX) + 1;
    const uint64_t perThread  = total / static_cast<uint64_t>(numThreads);

    using Clock = std::chrono::steady_clock;
    const auto startTime = Clock::now();

    // Each worker owns a contiguous nonce range and runs entirely on the stack.
    auto workerFn = [&](uint32_t nonceStart, uint32_t nonceEnd) {
        constexpr uint64_t CHUNK = 500'000;
        uint8_t  digest[32];
        uint64_t localCount = 0;

        for (uint64_t n = nonceStart; n <= uint64_t(nonceEnd); ++n) {
            if (foundFlag.load(std::memory_order_relaxed)) break;

            doubleSHA256Header(midstate, tail12, static_cast<uint32_t>(n), digest);
            ++localCount;

            if (hashMeetsTarget(digest, target)) {
                foundFlag.store(true,                        std::memory_order_relaxed);
                foundNonce.store(static_cast<uint32_t>(n),  std::memory_order_relaxed);
                break;
            }

            if (localCount == CHUNK) {
                hashesChecked.fetch_add(CHUNK, std::memory_order_relaxed);
                localCount = 0;
            }
        }
        // Flush remaining local count
        if (localCount > 0)
            hashesChecked.fetch_add(localCount, std::memory_order_relaxed);

        activeWorkers.fetch_sub(1, std::memory_order_relaxed);
    };

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(numThreads));
    activeWorkers.store(numThreads, std::memory_order_relaxed);

    for (int i = 0; i < numThreads; ++i) {
        const uint32_t ns = static_cast<uint32_t>(static_cast<uint64_t>(i) * perThread);
        const uint32_t ne = (i == numThreads - 1)
                          ? UINT32_MAX
                          : static_cast<uint32_t>(static_cast<uint64_t>(i + 1) * perThread - 1);
        threads.emplace_back(workerFn, ns, ne);
    }

    // Main thread polls progress every 5 s while workers run.
    while (activeWorkers.load(std::memory_order_relaxed) > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (activeWorkers.load(std::memory_order_relaxed) == 0) break;

        const double   elapsed = std::chrono::duration<double>(Clock::now() - startTime).count();
        const uint64_t checked = hashesChecked.load(std::memory_order_relaxed);
        const double   mhs     = static_cast<double>(checked) / elapsed / 1e6;

        std::cout << std::fixed << std::setprecision(1) << elapsed << "s  "
                  << checked / 1'000'000 << "M hashes  "
                  << std::setprecision(2) << mhs << " MH/s  "
                  << "threads=" << numThreads << "\n";
    }

    for (auto& t : threads) t.join();

    const double   totalElapsed = std::chrono::duration<double>(Clock::now() - startTime).count();
    const uint64_t totalChecked = hashesChecked.load(std::memory_order_relaxed);

    MineResult result;
    result.hashesChecked    = totalChecked;
    result.hashesPerSecond  = static_cast<double>(totalChecked) / totalElapsed;

    if (foundFlag.load(std::memory_order_relaxed)) {
        result.found = true;
        result.nonce = foundNonce.load(std::memory_order_relaxed);

        uint8_t digest[32];
        doubleSHA256Header(midstate, tail12, result.nonce, digest);
        for (int i = 0; i < 32; ++i)
            result.displayHash[i] = digest[31 - i];  // byte-reverse to display form
    }

    return result;
}
