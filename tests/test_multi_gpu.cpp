#include "cli.hpp"
#include "work_queue.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

template <typename Fn>
void expectThrowsInvalidArg(Fn&& fn, const char* label) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    if (!threw) {
        std::cerr << "FAIL: expected std::invalid_argument for " << label << '\n';
        assert(false);
    }
}

void testSchedulerExhaustiveParallel() {
    NonceBatchScheduler scheduler;
    assert(scheduler.numBatches() == 512);
    assert(scheduler.batchSize() == NonceBatchScheduler::DEFAULT_BATCH_SIZE);

    std::vector<std::atomic<int>> seen(static_cast<std::size_t>(scheduler.numBatches()));
    for (auto& s : seen) s.store(0, std::memory_order_relaxed);

    std::atomic<uint64_t> totalNonces{0};
    constexpr int kWorkers = 8;
    std::vector<std::thread> workers;
    workers.reserve(kWorkers);

    for (int w = 0; w < kWorkers; ++w) {
        workers.emplace_back([&]() {
            while (auto claim = scheduler.claimNextBatch()) {
                assert(claim->batchIndex >= 0 && claim->batchIndex < scheduler.numBatches());
                assert(claim->count == static_cast<uint32_t>(scheduler.batchSize()));
                const uint32_t expectedStart = static_cast<uint32_t>(
                    static_cast<uint64_t>(claim->batchIndex) *
                    static_cast<uint64_t>(scheduler.batchSize()));
                assert(claim->nonceStart == expectedStart);
                seen[static_cast<std::size_t>(claim->batchIndex)].fetch_add(1, std::memory_order_relaxed);
                totalNonces.fetch_add(claim->count, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : workers) t.join();

    assert(!scheduler.isFound());
    assert(!scheduler.shouldStop());
    assert(totalNonces.load() == NonceBatchScheduler::TOTAL_NONCES);
    for (int64_t i = 0; i < scheduler.numBatches(); ++i)
        assert(seen[static_cast<std::size_t>(i)].load() == 1);
}

void testHeterogeneousDynamicLoadBalance() {
    NonceBatchScheduler scheduler(int64_t(1) << 25);  // 128 batches
    assert(scheduler.numBatches() == 128);

    std::atomic<int> fastCount{0};
    std::atomic<int> slowCount{0};

    std::thread fastWorker([&]() {
        while (auto claim = scheduler.claimNextBatch()) {
            (void)claim;
            fastCount.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    std::thread slowWorker([&]() {
        while (auto claim = scheduler.claimNextBatch()) {
            (void)claim;
            slowCount.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(250));
        }
    });

    fastWorker.join();
    slowWorker.join();

    assert(fastCount.load() + slowCount.load() == 128);
    assert(fastCount.load() > slowCount.load());
}

void testEarlyStopAndWinnerRace() {
    NonceBatchScheduler scheduler;
    std::atomic<int> winners{0};
    std::atomic<uint64_t> processed{0};
    constexpr uint32_t kTargetBatch = 20;

    std::vector<std::thread> workers;
    for (int w = 0; w < 4; ++w) {
        workers.emplace_back([&, w]() {
            while (auto claim = scheduler.claimNextBatch()) {
                processed.fetch_add(1, std::memory_order_relaxed);
                if (claim->batchIndex >= static_cast<int64_t>(kTargetBatch)) {
                    if (scheduler.markFound(claim->nonceStart + static_cast<uint32_t>(w)))
                        winners.fetch_add(1, std::memory_order_relaxed);
                    break;
                }
            }
        });
    }
    for (auto& t : workers) t.join();

    assert(scheduler.isFound());
    assert(scheduler.shouldStop());
    assert(winners.load() == 1);
    assert(processed.load() < static_cast<uint64_t>(scheduler.numBatches()));
    assert(scheduler.claimNextBatch() == std::nullopt);
}

void testAbortStopsQueue() {
    NonceBatchScheduler scheduler;
    auto first = scheduler.claimNextBatch();
    assert(first.has_value());
    scheduler.abort();
    assert(scheduler.shouldStop());
    assert(!scheduler.isFound());
    assert(!scheduler.claimNextBatch().has_value());
}

void testInvalidBatchSize() {
    expectThrowsInvalidArg([]() { NonceBatchScheduler(0); }, "batchSize=0");
    expectThrowsInvalidArg([]() { NonceBatchScheduler(-1); }, "batchSize=-1");
    expectThrowsInvalidArg([]() { NonceBatchScheduler(1000); }, "non-divisor batchSize");
}

void testResolveDeviceIds() {
    assert((resolveDeviceIds(3, {}) == std::vector<int>{0, 1, 2}));
    assert((resolveDeviceIds(4, {2, 0, 2, 3}) == std::vector<int>{2, 0, 3}));

    expectThrowsInvalidArg([]() { resolveDeviceIds(2, {2}); }, "device out of upper bound");
    expectThrowsInvalidArg([]() { resolveDeviceIds(2, {-1}); }, "negative device id");

    bool threwNoGpu = false;
    try {
        resolveDeviceIds(0, {});
    } catch (const std::runtime_error&) {
        threwNoGpu = true;
    }
    assert(threwNoGpu);
}

void testCliParser() {
    {
        char arg0[] = "bitcoin_miner_gpu";
        char* argv[] = {arg0};
        const CliOptions opts = parseCliOptions(1, argv);
        assert(!opts.showHelp && !opts.listGpus && opts.gpuIds.empty());
    }
    {
        char arg0[] = "bitcoin_miner_gpu";
        char arg1[] = "--gpus";
        char arg2[] = "0,2,1";
        char arg3[] = "--list-gpus";
        char* argv[] = {arg0, arg1, arg2, arg3};
        const CliOptions opts = parseCliOptions(4, argv);
        assert(opts.listGpus);
        assert((opts.gpuIds == std::vector<int>{0, 2, 1}));
    }
    {
        char arg0[] = "bitcoin_miner_gpu";
        char arg1[] = "--gpus=3,1";
        char arg2[] = "-h";
        char* argv[] = {arg0, arg1, arg2};
        const CliOptions opts = parseCliOptions(3, argv);
        assert(opts.showHelp);
        assert((opts.gpuIds == std::vector<int>{3, 1}));
    }

    expectThrowsInvalidArg([]() {
        char arg0[] = "prog"; char arg1[] = "--gpus";
        char* argv[] = {arg0, arg1};
        parseCliOptions(2, argv);
    }, "missing --gpus arg");

    expectThrowsInvalidArg([]() {
        char arg0[] = "prog"; char arg1[] = "--gpus=0,";
        char* argv[] = {arg0, arg1};
        parseCliOptions(2, argv);
    }, "trailing comma");

    expectThrowsInvalidArg([]() {
        char arg0[] = "prog"; char arg1[] = "--gpus=-1";
        char* argv[] = {arg0, arg1};
        parseCliOptions(2, argv);
    }, "negative gpu");

    expectThrowsInvalidArg([]() {
        char arg0[] = "prog"; char arg1[] = "--gpus=abc";
        char* argv[] = {arg0, arg1};
        parseCliOptions(2, argv);
    }, "non-numeric gpu");

    expectThrowsInvalidArg([]() {
        char arg0[] = "prog"; char arg1[] = "--bogus";
        char* argv[] = {arg0, arg1};
        parseCliOptions(2, argv);
    }, "unknown flag");
}

} // namespace

int main() {
    testSchedulerExhaustiveParallel();
    testHeterogeneousDynamicLoadBalance();
    testEarlyStopAndWinnerRace();
    testAbortStopsQueue();
    testInvalidBatchSize();
    testResolveDeviceIds();
    testCliParser();

    std::cout << "All multi-GPU host unit tests passed.\n";
    return 0;
}
