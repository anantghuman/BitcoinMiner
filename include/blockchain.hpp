#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Metadata for a confirmed Bitcoin block from the Blockstream.info API.
// All hash fields are hex-encoded in display form (byte-reversed from raw SHA-256).
struct BlockInfo {
    std::string hash;
    std::string previousBlockHash;
    std::string merkleRoot;
    uint32_t    version{0};
    uint32_t    timestamp{0};
    uint32_t    bits{0};    // compact difficulty target (nBits)
    uint32_t    height{0};
};

void      initializeHttpClient();
void      cleanupHttpClient();

// Fetches the current chain tip. Returns a default BlockInfo (hash.empty()) on failure.
BlockInfo getMiningInfo();

// Decodes a hex string into raw bytes. Handles upper- and lower-case.
std::vector<uint8_t> hexStringToBytes(const std::string& hex);
