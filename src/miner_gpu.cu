#include "miner_gpu.hpp"
#include "sha256_constants.hpp"
#include "work_queue.hpp"

#include <cuda_runtime.h>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// ── CUDA error helper ─────────────────────────────────────────────────────────

#define CUDA_CHECK(call)                                                      \
    do {                                                                      \
        cudaError_t _err = (call);                                            \
        if (_err != cudaSuccess)                                              \
            throw std::runtime_error(                                         \
                std::string("CUDA error: ") + cudaGetErrorString(_err));      \
    } while (0)

// ── Device constant memory ────────────────────────────────────────────────────
// Constant memory is instantiated per CUDA device; each worker thread uploads
// these symbols to its own active device after cudaSetDevice().

__device__ __constant__ uint32_t d_K[64];          // SHA-256 round constants
__device__ __constant__ uint32_t d_INIT[8];        // SHA-256 initial hash values
__device__ __constant__ uint32_t d_midstate[8];    // SHA-256 state after first 64 bytes of header
__device__ __constant__ uint8_t  d_tail12[12];     // header bytes 64-75 (constant across nonces)
__device__ __constant__ uint8_t  d_target[32];     // 32-byte big-endian difficulty target

// ── Device SHA-256 ────────────────────────────────────────────────────────────

__device__ __forceinline__ uint32_t rotr32(uint32_t x, uint32_t n) noexcept {
    return (x >> n) | (x << (32u - n));
}

__device__ __forceinline__ uint32_t sig0(uint32_t x) noexcept { return rotr32(x, 7)  ^ rotr32(x, 18) ^ (x >>  3); }
__device__ __forceinline__ uint32_t sig1(uint32_t x) noexcept { return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10); }
__device__ __forceinline__ uint32_t SIG0(uint32_t x) noexcept { return rotr32(x, 2)  ^ rotr32(x, 13) ^ rotr32(x, 22); }
__device__ __forceinline__ uint32_t SIG1(uint32_t x) noexcept { return rotr32(x, 6)  ^ rotr32(x, 11) ^ rotr32(x, 25); }
__device__ __forceinline__ uint32_t Ch (uint32_t e, uint32_t f, uint32_t g) noexcept { return  (e & f) ^ (~e & g); }
__device__ __forceinline__ uint32_t Maj(uint32_t a, uint32_t b, uint32_t c) noexcept { return  (a & b) ^ ( a & c) ^ (b & c); }

__device__ __forceinline__ uint32_t ldBE32(const uint8_t* p) noexcept {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

__device__ void sha256Compress(uint32_t h[8], const uint8_t blk[64]) noexcept {
    uint32_t w[64];
    #pragma unroll 16
    for (int i = 0;  i < 16; ++i) w[i] = ldBE32(blk + i * 4);
    #pragma unroll 48
    for (int i = 16; i < 64; ++i)
        w[i] = sig1(w[i-2]) + w[i-7] + sig0(w[i-15]) + w[i-16];

    uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], hh=h[7];
    #pragma unroll 64
    for (int i = 0; i < 64; ++i) {
        uint32_t T1 = hh + SIG1(e) + Ch(e,f,g) + d_K[i] + w[i];
        uint32_t T2 = SIG0(a) + Maj(a,b,c);
        hh=g; g=f; f=e; e=d+T1;
        d =c; c=b; b=a; a=T1+T2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
    h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
}

// Computes SHA-256^2(header) for a given nonce, starting from the precomputed midstate.
// out[32] receives the raw (non-byte-reversed) double-SHA-256 digest.
__device__ void doubleSHA256(uint32_t nonce, uint8_t out[32]) noexcept {
    // ---- First SHA-256: second (and final) 64-byte block of the 80-byte header ----
    // Layout: tail12 (12 B) | nonce LE (4 B) | 0x80 | zeros | length (2 B at [62-63])
    uint8_t blk1[64];
    for (int i = 0; i < 64; ++i) blk1[i] = 0;
    for (int i = 0; i < 12; ++i) blk1[i] = d_tail12[i];
    blk1[12] = uint8_t(nonce);
    blk1[13] = uint8_t(nonce >>  8);
    blk1[14] = uint8_t(nonce >> 16);
    blk1[15] = uint8_t(nonce >> 24);
    blk1[16] = 0x80;               // SHA-256 padding marker
    blk1[62] = 0x02; blk1[63] = 0x80;  // message length: 80*8 = 640 = 0x0000000000000280

    uint32_t h1[8];
    for (int i = 0; i < 8; ++i) h1[i] = d_midstate[i];
    sha256Compress(h1, blk1);

    // ---- Second SHA-256: single 64-byte block over the 32-byte first digest ----
    uint8_t blk2[64];
    for (int i = 0; i < 64; ++i) blk2[i] = 0;
    for (int i = 0; i < 8; ++i) {
        blk2[4*i]   = uint8_t(h1[i] >> 24);
        blk2[4*i+1] = uint8_t(h1[i] >> 16);
        blk2[4*i+2] = uint8_t(h1[i] >>  8);
        blk2[4*i+3] = uint8_t(h1[i]);
    }
    blk2[32] = 0x80;               // SHA-256 padding marker
    blk2[62] = 0x01; blk2[63] = 0x00;  // message length: 32*8 = 256 = 0x0000000000000100

    uint32_t h2[8];
    for (int i = 0; i < 8; ++i) h2[i] = d_INIT[i];
    sha256Compress(h2, blk2);

    for (int i = 0; i < 8; ++i) {
        out[4*i]   = uint8_t(h2[i] >> 24);
        out[4*i+1] = uint8_t(h2[i] >> 16);
        out[4*i+2] = uint8_t(h2[i] >>  8);
        out[4*i+3] = uint8_t(h2[i]);
    }
}

// Bitcoin displays hashes byte-reversed; compare reversed digest against d_target.
__device__ bool meetsTarget(const uint8_t digest[32]) noexcept {
    for (int i = 0; i < 32; ++i) {
        const uint8_t d = digest[31 - i];
        if (d < d_target[i]) return true;
        if (d > d_target[i]) return false;
    }
    return false;   // exactly equal does not meet target
}

// ── Mining kernel ─────────────────────────────────────────────────────────────
// Each thread owns one nonce. On success, uses atomicCAS to claim the result slot.

__global__ void mineKernel(uint32_t nonceStart, uint32_t* d_foundNonce, int* d_found) {
    const uint32_t nonce = nonceStart + blockIdx.x * blockDim.x + threadIdx.x;

    uint8_t digest[32];
    doubleSHA256(nonce, digest);

    if (meetsTarget(digest)) {
        if (atomicCAS(d_found, 0, 1) == 0)
            *d_foundNonce = nonce;
    }
}

// ── Host-side SHA-256 & RAII CUDA helpers ─────────────────────────────────────

namespace {

inline uint32_t hRotr(uint32_t x, uint32_t n) noexcept { return (x >> n) | (x << (32u - n)); }
inline uint32_t hSig0(uint32_t x) noexcept { return hRotr(x,7)  ^ hRotr(x,18) ^ (x>>3);  }
inline uint32_t hSig1(uint32_t x) noexcept { return hRotr(x,17) ^ hRotr(x,19) ^ (x>>10); }
inline uint32_t hSIG0(uint32_t x) noexcept { return hRotr(x,2)  ^ hRotr(x,13) ^ hRotr(x,22); }
inline uint32_t hSIG1(uint32_t x) noexcept { return hRotr(x,6)  ^ hRotr(x,11) ^ hRotr(x,25); }
inline uint32_t hCh (uint32_t e, uint32_t f, uint32_t g) noexcept { return (e&f)^(~e&g); }
inline uint32_t hMaj(uint32_t a, uint32_t b, uint32_t c) noexcept { return (a&b)^(a&c)^(b&c); }
inline uint32_t hLdBE32(const uint8_t* p) noexcept {
    return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
}

void hostCompress(uint32_t h[8], const uint8_t blk[64]) noexcept {
    uint32_t w[64];
    for (int i = 0;  i < 16; ++i) w[i] = hLdBE32(blk + i*4);
    for (int i = 16; i < 64; ++i)
        w[i] = hSig1(w[i-2]) + w[i-7] + hSig0(w[i-15]) + w[i-16];
    uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], hh=h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t T1 = hh + hSIG1(e) + hCh(e,f,g) + SHA256_K[i] + w[i];
        uint32_t T2 = hSIG0(a) + hMaj(a,b,c);
        hh=g; g=f; f=e; e=d+T1; d=c; c=b; b=a; a=T1+T2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
    h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
}

// Recompute double-SHA256 on the CPU for a known nonce (used to build displayHash).
void hostDoubleSHA256(const uint32_t midstate[8], const uint8_t tail12[12],
                      uint32_t nonce, uint8_t out[32]) noexcept {
    uint8_t blk1[64] = {};
    for (int i = 0; i < 12; ++i) blk1[i] = tail12[i];
    blk1[12] = uint8_t(nonce);
    blk1[13] = uint8_t(nonce >>  8);
    blk1[14] = uint8_t(nonce >> 16);
    blk1[15] = uint8_t(nonce >> 24);
    blk1[16] = 0x80;
    blk1[62] = 0x02; blk1[63] = 0x80;

    uint32_t h1[8];
    for (int i = 0; i < 8; ++i) h1[i] = midstate[i];
    hostCompress(h1, blk1);

    uint8_t blk2[64] = {};
    for (int i = 0; i < 8; ++i) {
        blk2[4*i]   = uint8_t(h1[i] >> 24);
        blk2[4*i+1] = uint8_t(h1[i] >> 16);
        blk2[4*i+2] = uint8_t(h1[i] >>  8);
        blk2[4*i+3] = uint8_t(h1[i]);
    }
    blk2[32] = 0x80;
    blk2[62] = 0x01; blk2[63] = 0x00;

    uint32_t h2[8];
    for (int i = 0; i < 8; ++i) h2[i] = SHA256_INIT[i];
    hostCompress(h2, blk2);

    for (int i = 0; i < 8; ++i) {
        out[4*i]   = uint8_t(h2[i] >> 24);
        out[4*i+1] = uint8_t(h2[i] >> 16);
        out[4*i+2] = uint8_t(h2[i] >>  8);
        out[4*i+3] = uint8_t(h2[i]);
    }
}

struct CudaDeviceResources {
    int          deviceId{0};
    cudaStream_t stream{nullptr};
    int*         d_found{nullptr};
    uint32_t*    d_foundNonce{nullptr};

    explicit CudaDeviceResources(int devId) : deviceId(devId) {
        CUDA_CHECK(cudaSetDevice(deviceId));
        CUDA_CHECK(cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking));
        CUDA_CHECK(cudaMalloc(&d_found, sizeof(int)));
        CUDA_CHECK(cudaMalloc(&d_foundNonce, sizeof(uint32_t)));
        CUDA_CHECK(cudaMemsetAsync(d_found, 0, sizeof(int), stream));
        CUDA_CHECK(cudaMemsetAsync(d_foundNonce, 0, sizeof(uint32_t), stream));
        CUDA_CHECK(cudaStreamSynchronize(stream));
    }

    ~CudaDeviceResources() {
        cudaSetDevice(deviceId);
        if (d_foundNonce) cudaFree(d_foundNonce);
        if (d_found)      cudaFree(d_found);
        if (stream)       cudaStreamDestroy(stream);
    }

    CudaDeviceResources(const CudaDeviceResources&)            = delete;
    CudaDeviceResources& operator=(const CudaDeviceResources&) = delete;
};

void uploadConstantsToActiveDevice(const uint32_t kHost[64],
                                   const uint32_t initHost[8],
                                   const uint32_t midstate[8],
                                   const uint8_t  tail12[12],
                                   const std::array<uint8_t, 32>& target) {
    CUDA_CHECK(cudaMemcpyToSymbol(d_K,        kHost,         64 * sizeof(uint32_t)));
    CUDA_CHECK(cudaMemcpyToSymbol(d_INIT,     initHost,       8 * sizeof(uint32_t)));
    CUDA_CHECK(cudaMemcpyToSymbol(d_midstate, midstate,       8 * sizeof(uint32_t)));
    CUDA_CHECK(cudaMemcpyToSymbol(d_tail12,   tail12,        12 * sizeof(uint8_t)));
    CUDA_CHECK(cudaMemcpyToSymbol(d_target,   target.data(), 32 * sizeof(uint8_t)));
}

} // namespace

// ── Public interface ──────────────────────────────────────────────────────────

void printGpuDevices() {
    int deviceCount = 0;
    CUDA_CHECK(cudaGetDeviceCount(&deviceCount));
    if (deviceCount == 0) {
        std::cout << "No CUDA-capable GPUs detected.\n";
        return;
    }

    std::cout << "Detected " << deviceCount << " CUDA GPU(s):\n";
    for (int id = 0; id < deviceCount; ++id) {
        cudaDeviceProp prop;
        CUDA_CHECK(cudaGetDeviceProperties(&prop, id));
        std::cout << "  [" << id << "] " << prop.name
                  << "  CC " << prop.major << '.' << prop.minor
                  << "  SMs: " << prop.multiProcessorCount
                  << "  VRAM: " << prop.totalGlobalMem / (1u << 20) << " MB\n";
    }
}

MineResult mineGPU(const std::string& headerPrefix,
                   const std::array<uint8_t, 32>& target,
                   const std::vector<int>& deviceIds) {
    if (headerPrefix.size() != 76)
        throw std::runtime_error("headerPrefix must be exactly 76 bytes");

    // ── Device discovery & selection ──────────────────────────────────────────
    int deviceCount = 0;
    CUDA_CHECK(cudaGetDeviceCount(&deviceCount));
    const std::vector<int> selectedDevices = resolveDeviceIds(deviceCount, deviceIds);
    const std::size_t      numGpus         = selectedDevices.size();

    std::vector<GpuDeviceStats> gpuStats(numGpus);
    std::cout << "Using " << numGpus << " GPU(s):\n";
    for (std::size_t i = 0; i < numGpus; ++i) {
        cudaDeviceProp prop;
        CUDA_CHECK(cudaGetDeviceProperties(&prop, selectedDevices[i]));
        gpuStats[i].deviceId = selectedDevices[i];
        gpuStats[i].name     = prop.name;
        gpuStats[i].smCount  = prop.multiProcessorCount;
        std::cout << "  GPU " << selectedDevices[i] << ": " << prop.name
                  << "  SMs: " << prop.multiProcessorCount
                  << "  VRAM: " << prop.totalGlobalMem / (1u << 20) << " MB\n";
    }
    std::cout << '\n';

    // ── Compute midstate on CPU (first 64 bytes, constant across all nonces) ─
    const auto*    prefix = reinterpret_cast<const uint8_t*>(headerPrefix.data());
    const uint8_t* tail12 = prefix + 64;   // bytes 64-75 of header

    uint32_t midstate[8];
    for (int i = 0; i < 8; ++i) midstate[i] = SHA256_INIT[i];
    hostCompress(midstate, prefix);

    uint32_t kHost[64], initHost[8];
    for (int i = 0; i < 64; ++i) kHost[i]    = SHA256_K[i];
    for (int i = 0;  i < 8; ++i) initHost[i] = SHA256_INIT[i];

    // ── Multi-GPU dynamic batch scheduler ─────────────────────────────────────
    constexpr int     BLOCK_SIZE = 256;
    constexpr int64_t BATCH_SIZE = NonceBatchScheduler::DEFAULT_BATCH_SIZE;
    constexpr int     GRID_SIZE  = static_cast<int>(BATCH_SIZE / BLOCK_SIZE);

    NonceBatchScheduler             scheduler(BATCH_SIZE);
    std::vector<std::atomic<uint64_t>> perGpuHashes(numGpus);
    for (auto& counter : perGpuHashes)
        counter.store(0, std::memory_order_relaxed);

    std::vector<std::exception_ptr> workerErrors(numGpus);
    std::atomic<std::size_t>        activeWorkers{numGpus};
    std::mutex                      cvMutex;
    std::condition_variable         cv;

    using Clock = std::chrono::steady_clock;
    const auto startTime = Clock::now();

    auto workerFn = [&](std::size_t idx) {
        const int devId = selectedDevices[idx];
        const auto workerStart = Clock::now();
        try {
            CudaDeviceResources res(devId);
            uploadConstantsToActiveDevice(kHost, initHost, midstate, tail12, target);

            while (auto claim = scheduler.claimNextBatch()) {
                mineKernel<<<GRID_SIZE, BLOCK_SIZE, 0, res.stream>>>(
                    claim->nonceStart, res.d_foundNonce, res.d_found);
                CUDA_CHECK(cudaGetLastError());

                int h_found = 0;
                CUDA_CHECK(cudaMemcpyAsync(&h_found, res.d_found, sizeof(int),
                                           cudaMemcpyDeviceToHost, res.stream));
                CUDA_CHECK(cudaStreamSynchronize(res.stream));

                perGpuHashes[idx].fetch_add(claim->count, std::memory_order_relaxed);

                if (h_found) {
                    uint32_t h_foundNonce = 0;
                    CUDA_CHECK(cudaMemcpyAsync(&h_foundNonce, res.d_foundNonce, sizeof(uint32_t),
                                               cudaMemcpyDeviceToHost, res.stream));
                    CUDA_CHECK(cudaStreamSynchronize(res.stream));
                    scheduler.markFound(h_foundNonce);
                    break;
                }
            }
        } catch (...) {
            workerErrors[idx] = std::current_exception();
            scheduler.abort();
        }

        const double workerElapsed =
            std::chrono::duration<double>(Clock::now() - workerStart).count();
        const uint64_t hashes = perGpuHashes[idx].load(std::memory_order_relaxed);
        gpuStats[idx].hashesChecked  = hashes;
        gpuStats[idx].elapsedSeconds = workerElapsed;
        gpuStats[idx].hashesPerSecond =
            (workerElapsed > 0.0) ? static_cast<double>(hashes) / workerElapsed : 0.0;

        if (activeWorkers.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard<std::mutex> lock(cvMutex);
            cv.notify_all();
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(numGpus);
    for (std::size_t i = 0; i < numGpus; ++i)
        workers.emplace_back(workerFn, i);

    // Report aggregate + per-GPU progress every 5 seconds until all workers finish.
    {
        std::unique_lock<std::mutex> lock(cvMutex);
        while (activeWorkers.load(std::memory_order_acquire) > 0) {
            const bool done = cv.wait_for(lock, std::chrono::seconds(5), [&]() {
                return activeWorkers.load(std::memory_order_acquire) == 0;
            });
            if (done) break;

            const double elapsed =
                std::chrono::duration<double>(Clock::now() - startTime).count();
            uint64_t totalHashes = 0;
            std::ostringstream perGpuLine;
            perGpuLine << std::fixed << std::setprecision(2);
            for (std::size_t i = 0; i < numGpus; ++i) {
                const uint64_t h = perGpuHashes[i].load(std::memory_order_relaxed);
                totalHashes += h;
                if (i > 0) perGpuLine << ", ";
                perGpuLine << "GPU" << selectedDevices[i] << "="
                           << (static_cast<double>(h) / elapsed / 1e6) << " MH/s";
            }
            const double totalMhs = static_cast<double>(totalHashes) / elapsed / 1e6;
            std::cout << std::fixed << std::setprecision(1) << elapsed << "s  "
                      << totalHashes / 1'000'000 << "M hashes  "
                      << std::setprecision(2) << totalMhs << " MH/s"
                      << "  [" << perGpuLine.str() << "]\n";
        }
    }

    for (auto& t : workers)
        t.join();

    for (const auto& err : workerErrors) {
        if (err) std::rethrow_exception(err);
    }

    const double totalElapsed =
        std::chrono::duration<double>(Clock::now() - startTime).count();

    uint64_t totalHashes = 0;
    for (const auto& s : gpuStats)
        totalHashes += s.hashesChecked;

    MineResult result;
    result.hashesChecked   = totalHashes;
    result.hashesPerSecond = (totalElapsed > 0.0)
                                 ? static_cast<double>(totalHashes) / totalElapsed
                                 : 0.0;
    result.gpuStats        = std::move(gpuStats);

    if (scheduler.isFound()) {
        result.found = true;
        result.nonce = scheduler.winningNonce();

        // Recompute display hash (byte-reversed digest) on CPU for the winning nonce.
        uint8_t digest[32];
        hostDoubleSHA256(midstate, tail12, result.nonce, digest);
        for (int i = 0; i < 32; ++i)
            result.displayHash[i] = digest[31 - i];
    }

    return result;
}
