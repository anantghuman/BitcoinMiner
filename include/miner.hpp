#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct GpuDeviceStats {
    int         deviceId{0};
    std::string name;
    int         smCount{0};
    uint64_t    hashesChecked{0};
    double      hashesPerSecond{0.0};
    double      elapsedSeconds{0.0};
};

struct MineResult {
    bool                        found{false};
    uint32_t                    nonce{0};
    std::array<uint8_t, 32>     displayHash{};  // byte-reversed digest (Bitcoin display form)
    uint64_t                    hashesChecked{0};
    double                      hashesPerSecond{0.0};
    std::vector<GpuDeviceStats> gpuStats{};     // populated by multi-GPU miner
};

// Searches all 2^32 nonces for a double-SHA256 that meets the difficulty target.
// headerPrefix: 76 bytes (full header minus the 4-byte nonce).
// target:       32-byte big-endian limit from expandTarget(nBits).
MineResult mine(const std::string& headerPrefix,
                const std::array<uint8_t, 32>& target);
