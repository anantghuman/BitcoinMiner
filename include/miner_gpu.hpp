#pragma once

#include "miner.hpp"   // reuse MineResult

#include <array>
#include <cstdint>
#include <string>

// GPU (CUDA) equivalent of mine().
// Same interface as the CPU version: headerPrefix is 76 bytes, target is the
// 32-byte big-endian limit from expandTarget(nBits).
MineResult mineGPU(const std::string& headerPrefix,
                   const std::array<uint8_t, 32>& target);
