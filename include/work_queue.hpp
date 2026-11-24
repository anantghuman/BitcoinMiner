#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct BatchClaim {
    uint32_t nonceStart{0};
    uint32_t count{0};
    int64_t  batchIndex{0};
};

// Thread-safe dynamic batch scheduler partitioning [0, 2^32) across workers.
class NonceBatchScheduler {
public:
    static constexpr uint64_t TOTAL_NONCES       = uint64_t(1) << 32;
    static constexpr int64_t  DEFAULT_BATCH_SIZE = int64_t(1) << 23;  // 8,388,608 nonces

    explicit NonceBatchScheduler(int64_t batchSize = DEFAULT_BATCH_SIZE)
        : batchSize_(batchSize) {
        if (batchSize_ <= 0 || (TOTAL_NONCES % static_cast<uint64_t>(batchSize_)) != 0) {
            throw std::invalid_argument(
                "batchSize must be positive and evenly divide 2^32");
        }
        numBatches_ = static_cast<int64_t>(TOTAL_NONCES / static_cast<uint64_t>(batchSize_));
    }

    NonceBatchScheduler(const NonceBatchScheduler&)            = delete;
    NonceBatchScheduler& operator=(const NonceBatchScheduler&) = delete;

    std::optional<BatchClaim> claimNextBatch() noexcept {
        if (shouldStop())
            return std::nullopt;

        const int64_t idx = nextBatch_.fetch_add(1, std::memory_order_relaxed);
        if (idx >= numBatches_ || shouldStop())
            return std::nullopt;

        BatchClaim claim;
        claim.nonceStart = static_cast<uint32_t>(static_cast<uint64_t>(idx) *
                                                 static_cast<uint64_t>(batchSize_));
        claim.count      = static_cast<uint32_t>(batchSize_);
        claim.batchIndex = idx;
        return claim;
    }

    // Records the first winning nonce; returns true only for the winning caller.
    bool markFound(uint32_t nonce) noexcept {
        bool expected = false;
        if (!winnerClaimed_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return false;

        winningNonce_.store(nonce, std::memory_order_relaxed);
        found_.store(true, std::memory_order_release);
        return true;
    }

    void abort() noexcept {
        aborted_.store(true, std::memory_order_release);
    }

    bool shouldStop() const noexcept {
        return found_.load(std::memory_order_acquire) ||
               aborted_.load(std::memory_order_acquire);
    }

    bool isFound() const noexcept {
        return found_.load(std::memory_order_acquire);
    }

    uint32_t winningNonce() const noexcept {
        return winningNonce_.load(std::memory_order_acquire);
    }

    int64_t batchSize()  const noexcept { return batchSize_; }
    int64_t numBatches() const noexcept { return numBatches_; }

private:
    int64_t               batchSize_;
    int64_t               numBatches_{0};
    std::atomic<int64_t>  nextBatch_{0};
    std::atomic<bool>     winnerClaimed_{false};
    std::atomic<bool>     found_{false};
    std::atomic<bool>     aborted_{false};
    std::atomic<uint32_t> winningNonce_{0};
};

// Resolves requested GPU IDs against available device count, preserving order
// and removing duplicates. Empty requestedIds selects all [0, deviceCount).
inline std::vector<int> resolveDeviceIds(int deviceCount,
                                         const std::vector<int>& requestedIds) {
    if (deviceCount <= 0)
        throw std::runtime_error("No CUDA-capable GPU found.");

    std::vector<int> resolved;
    if (requestedIds.empty()) {
        resolved.reserve(static_cast<std::size_t>(deviceCount));
        for (int i = 0; i < deviceCount; ++i)
            resolved.push_back(i);
        return resolved;
    }

    std::vector<bool> seen(static_cast<std::size_t>(deviceCount), false);
    resolved.reserve(requestedIds.size());
    for (int id : requestedIds) {
        if (id < 0 || id >= deviceCount) {
            throw std::invalid_argument(
                "Requested GPU device ID " + std::to_string(id) +
                " is out of range [0, " + std::to_string(deviceCount - 1) + "]");
        }
        if (!seen[static_cast<std::size_t>(id)]) {
            seen[static_cast<std::size_t>(id)] = true;
            resolved.push_back(id);
        }
    }
    return resolved;
}
