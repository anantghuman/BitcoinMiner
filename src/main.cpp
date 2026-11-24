#include "blockchain.hpp"
#include "cli.hpp"
#ifdef USE_GPU
#include "miner_gpu.hpp"
#else
#include "miner.hpp"
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <ctime>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

static std::string toHex(const std::array<uint8_t, 32>& bytes) {
    std::ostringstream oss;
    for (uint8_t b : bytes)
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    return oss.str();
}

static void appendLE32(std::string& buf, uint32_t val) {
    for (int i = 0; i < 4; ++i)
        buf += static_cast<char>((val >> (8 * i)) & 0xFF);
}

// prevHash and merkleRoot are stored in wire format as byte-reversed display strings.
static void appendHashLE(std::string& buf, const std::string& hexHash) {
    std::vector<uint8_t> bytes = hexStringToBytes(hexHash);
    std::reverse(bytes.begin(), bytes.end());
    for (uint8_t b : bytes)
        buf += static_cast<char>(b);
}

// Builds the 76-byte header prefix; nonce is appended per-iteration in the mining loop.
static std::string buildHeaderPrefix(const BlockInfo& tip) {
    std::string prefix;
    prefix.reserve(76);
    appendLE32(prefix, tip.version);
    appendHashLE(prefix, tip.hash);
    appendHashLE(prefix, tip.merkleRoot);
    appendLE32(prefix, static_cast<uint32_t>(std::time(nullptr)));
    appendLE32(prefix, tip.bits);
    return prefix;
}

// Expands compact nBits to a 32-byte big-endian target.
// nBits encoding: [exponent 8b | coefficient 24b], target = coeff × 256^(exp−3).
static std::array<uint8_t, 32> expandTarget(uint32_t nBits) {
    std::array<uint8_t, 32> target{};
    const uint32_t exp   = (nBits >> 24) & 0xFFu;
    const uint32_t coeff =  nBits        & 0x007FFFFFu;
    const int pos = 32 - static_cast<int>(exp);
    if (pos >= 0 && pos + 2 <= 31) {
        target[static_cast<std::size_t>(pos)]     = static_cast<uint8_t>((coeff >> 16) & 0xFF);
        target[static_cast<std::size_t>(pos) + 1] = static_cast<uint8_t>((coeff >>  8) & 0xFF);
        target[static_cast<std::size_t>(pos) + 2] = static_cast<uint8_t>( coeff        & 0xFF);
    }
    return target;
}

int main(int argc, char* argv[]) {
    CliOptions options;
    try {
        options = parseCliOptions(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n\n";
        printUsage(argv[0], std::cerr);
        return 1;
    }

    if (options.showHelp) {
        printUsage(argv[0], std::cout);
        return 0;
    }

#ifdef USE_GPU
    if (options.listGpus) {
        try {
            printGpuDevices();
            return 0;
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << "\n";
            return 1;
        }
    }
#else
    if (options.listGpus || !options.gpuIds.empty()) {
        std::cerr << "Error: --gpus and --list-gpus require the GPU build (make GPU=1).\n";
        return 1;
    }
#endif

    initializeHttpClient();

    const BlockInfo tip = getMiningInfo();
    if (tip.hash.empty()) {
        std::cerr << "Failed to retrieve chain tip. Exiting.\n";
        cleanupHttpClient();
        return 1;
    }

    std::cout << "\n=== Bitcoin Miner (Educational) ===\n"
              << "Mining block #" << tip.height + 1
              << " on top of block #" << tip.height << "\n\n";

    const std::array<uint8_t, 32> target = expandTarget(tip.bits);

    std::cout << "Difficulty target (first 12 bytes): ";
    for (int i = 0; i < 12; ++i)
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << static_cast<int>(target[i]);
    std::cout << "...\n" << std::dec;

    const std::string prefix = buildHeaderPrefix(tip);
#ifdef USE_GPU
    std::cout << "\nStarting GPU mining...\n\n";
#else
    std::cout << "\nStarting mining with " << std::thread::hardware_concurrency()
              << " threads...\n\n";
#endif

    cleanupHttpClient();

    MineResult result;
    try {
#ifdef USE_GPU
        result = mineGPU(prefix, target, options.gpuIds);
#else
        result = mine(prefix, target);
#endif
    } catch (const std::exception& ex) {
        std::cerr << "Mining failed: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "\n--- Results ---\n"
              << std::fixed << std::setprecision(2)
              << "Hashes checked : " << result.hashesChecked << "\n"
              << "Throughput     : " << result.hashesPerSecond / 1e6 << " MH/s\n";

    if (!result.gpuStats.empty()) {
        std::cout << "\nPer-GPU Breakdown:\n";
        for (const auto& gpu : result.gpuStats) {
            const double sharePct = (result.hashesChecked > 0)
                ? (100.0 * static_cast<double>(gpu.hashesChecked) /
                   static_cast<double>(result.hashesChecked))
                : 0.0;
            std::cout << "  GPU " << gpu.deviceId << " (" << gpu.name << "): "
                      << gpu.hashesChecked << " hashes ("
                      << std::setprecision(1) << sharePct << "%), "
                      << std::setprecision(2) << gpu.hashesPerSecond / 1e6 << " MH/s\n";
        }
    }

    if (result.found) {
        std::cout << "\n=== BLOCK FOUND ===\n"
                  << "Nonce : " << result.nonce << "\n"
                  << "Hash  : " << toHex(result.displayHash) << "\n";
    } else {
        std::cout << "Exhausted all 2^32 nonces. No block found.\n"
                  << "Real Bitcoin requires ASIC hardware.\n";
    }

    return 0;
}
