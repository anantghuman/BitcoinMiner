#pragma once

#include <cstdint>
#include <string>
#include <vector>

/// Metadata for a confirmed Bitcoin block, sourced from the Blockstream.info API.
/// All hash/root fields are hex-encoded in display form (byte-reversed from the
/// raw SHA-256 output, which is the standard Bitcoin representation).
struct BlockInfo {
    std::string hash;               ///< This block's hash (64 hex chars)
    std::string previousBlockHash;  ///< Previous block's hash (64 hex chars)
    std::string merkleRoot;         ///< Merkle root of all transactions (64 hex chars)
    uint32_t    version{0};         ///< Block version (encodes soft-fork activation via BIP9)
    uint32_t    timestamp{0};       ///< Unix timestamp of block creation
    uint32_t    bits{0};            ///< Compact difficulty target (nBits)
    uint32_t    height{0};          ///< Block height (genesis = 0)
};

/// Initialises the global libcurl state. Must be called once before any network
/// operations and matched by a single call to cleanupHttpClient() at exit.
void initializeHttpClient();

/// Releases all resources acquired by initializeHttpClient().
void cleanupHttpClient();

/// Fetches the current Bitcoin chain tip from Blockstream.info and returns its
/// metadata. On any network or parse failure, returns a default-constructed
/// BlockInfo whose hash field is empty.
BlockInfo getMiningInfo();

/// Decodes a hex-encoded string (upper- or lower-case) into raw bytes.
/// Pairs of characters map to one output byte; an odd-length input silently
/// ignores the trailing unpaired nibble.
std::vector<uint8_t> hexStringToBytes(const std::string& hex);
