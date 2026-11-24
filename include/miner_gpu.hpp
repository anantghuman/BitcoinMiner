#pragma once

#include "miner.hpp"   // reuse MineResult

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Prints all detected CUDA-capable GPUs and their basic properties.
void printGpuDevices();

// GPU (CUDA) equivalent of mine(), scaled across the specified CUDA devices.
// Passing an empty deviceIds vector uses all detected GPUs.
MineResult mineGPU(const std::string& headerPrefix,
                   const std::array<uint8_t, 32>& target,
                   const std::vector<int>& deviceIds);

inline MineResult mineGPU(const std::string& headerPrefix,
                          const std::array<uint8_t, 32>& target) {
    return mineGPU(headerPrefix, target, {});
}
