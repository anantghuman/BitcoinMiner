// main.cpp — CPU-based Bitcoin miner (educational).
//
// Workflow:
//   1. Fetch the current chain tip from Blockstream.info.
//   2. Build the 80-byte Bitcoin block header for the candidate next block.
//   3. Iterate over all 2^32 nonce values, computing SHA-256^2(header) each time.
//   4. Check whether the result meets the real network difficulty target (nBits).
//
// Bitcoin block header layout (80 bytes, all fields little-endian):
//   [version  4B][prevHash 32B][merkleRoot 32B][timestamp 4B][bits 4B][nonce 4B]
//
// Proof-of-work check:
//   Let H = SHA-256(SHA-256(header)) in natural (big-endian) byte order.
//   Bitcoin displays H byte-reversed; the displayed hash must be numerically
//   less than the target derived from nBits. Equivalently, the last N bytes of
//   the raw SHA-256 output must be zero (and adjacent bytes bounded).
//
// Note: Real Bitcoin difficulty (≥ 9 leading zero bytes as of 2026) is
// computationally infeasible on a single CPU core. This miner will exhaust all
// 2^32 nonces without finding a block, which is the expected outcome.

#include "blockchain.hpp"
#include "sha256.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Hashing
// ---------------------------------------------------------------------------

// Bitcoin's proof-of-work hash: SHA-256 applied twice to the block header.
static std::string doubleSHA256(const std::string& data) {
    return sha256(sha256(data));
}

// Encodes a raw byte string as a lowercase hex string.
static std::string toHex(const std::string& bytes) {
    std::ostringstream oss;
    for (unsigned char c : bytes)
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(c);
    return oss.str();
}

// ---------------------------------------------------------------------------
// Header serialisation
// ---------------------------------------------------------------------------

// Appends val to buf as 4 bytes in little-endian order.
static void appendLE32(std::string& buf, uint32_t val) {
    for (int i = 0; i < 4; ++i)
        buf += static_cast<char>((val >> (8 * i)) & 0xFF);
}

// Appends a 32-byte hash field to buf in wire (little-endian) byte order.
// Bitcoin encodes prevHash and merkleRoot byte-reversed relative to their
// display representation.
static void appendHashLE(std::string& buf, const std::string& hexHash) {
    std::vector<uint8_t> bytes = hexStringToBytes(hexHash);
    std::reverse(bytes.begin(), bytes.end());
    for (uint8_t b : bytes)
        buf += static_cast<char>(b);
}

// Constructs the 76-byte header prefix (version + prevHash + merkleRoot +
// timestamp + bits). The 4-byte nonce is appended separately inside the loop
// so that the prefix is built only once for all 2^32 nonce candidates.
static std::string buildHeaderPrefix(const BlockInfo& tip) {
    std::string prefix;
    prefix.reserve(76);
    appendLE32(prefix, tip.version);
    appendHashLE(prefix, tip.hash);        // previous block hash (32 B, LE)
    appendHashLE(prefix, tip.merkleRoot);  // merkle root         (32 B, LE)
    appendLE32(prefix, static_cast<uint32_t>(std::time(nullptr)));
    appendLE32(prefix, tip.bits);
    return prefix;
}

// ---------------------------------------------------------------------------
// Difficulty target
// ---------------------------------------------------------------------------

// Expands the compact nBits field to a 32-byte big-endian difficulty target.
//
// nBits encoding (FIPS notation):
//   bits[31:24] = exponent (exp)
//   bits[22:0]  = coefficient (coeff, 23-bit mantissa, top bit reserved)
//   target      = coeff × 256^(exp − 3)
//
// In the 32-byte array the coefficient's most-significant byte is placed at
// index (32 − exp), with the two following bytes holding the lower bytes.
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

// Returns true iff hash (32 raw bytes, byte-reversed / display form) is
// numerically strictly less than target (big-endian, from expandTarget()).
// Comparison is a straightforward lexicographic byte scan from MSB to LSB.
static bool meetsTarget(const std::string& hash,
                        const std::array<uint8_t, 32>& target) {
    for (std::size_t i = 0; i < 32; ++i) {
        const auto h = static_cast<uint8_t>(hash[i]);
        if (h < target[i]) return true;
        if (h > target[i]) return false;
    }
    return false;  // exact equality does not satisfy the strict inequality
}

// Counts leading 0x00 bytes in a hex-encoded hash (two hex chars = one byte).
// Used solely for progress reporting; not part of the validity check.
static int leadingZeroBytes(const std::string& hashHex) {
    int count = 0;
    for (std::size_t i = 0; i + 1 < hashHex.size(); i += 2) {
        if (hashHex[i] == '0' && hashHex[i + 1] == '0')
            ++count;
        else
            break;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main() {
    initializeHttpClient();

    const BlockInfo tip = getMiningInfo();
    if (tip.hash.empty()) {
        std::cerr << "Failed to retrieve chain tip. Exiting.\n";
        cleanupHttpClient();
        return 1;
    }

    std::cout << "\n=== Bitcoin Miner (Educational) ===\n"
              << "Attempting to mine block #" << tip.height + 1
              << " on top of confirmed block #" << tip.height << "\n\n";

    const std::array<uint8_t, 32> target = expandTarget(tip.bits);

    std::cout << "Difficulty target (first 12 bytes): ";
    for (int i = 0; i < 12; ++i)
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << static_cast<int>(target[i]);
    std::cout << "...\n" << std::dec;

    // Build the 76-byte prefix once; only the 4-byte nonce varies per iteration.
    const std::string prefix = buildHeaderPrefix(tip);

    constexpr uint64_t NONCE_COUNT = static_cast<uint64_t>(UINT32_MAX) + 1;
    std::cout << "\nSearching " << NONCE_COUNT << " nonces "
              << "(exhausting all 2^32 candidates)...\n\n";

    int  bestZeros = 0;
    bool found     = false;

    for (uint64_t nonce = 0; nonce < NONCE_COUNT; ++nonce) {
        // Complete the 80-byte header by appending the nonce.
        std::string header = prefix;
        appendLE32(header, static_cast<uint32_t>(nonce));

        // Compute SHA-256^2, then byte-reverse the digest to obtain the
        // display form used by Bitcoin for difficulty comparison.
        std::string hash = doubleSHA256(header);
        std::reverse(hash.begin(), hash.end());

        if (meetsTarget(hash, target)) {
            found = true;
            std::cout << "\n=== BLOCK FOUND ===\n"
                      << "Nonce  : " << nonce        << '\n'
                      << "Hash   : " << toHex(hash)  << '\n';
            break;
        }

        // Track and display the best (most leading-zero bytes) hash seen.
        // Avoid calling toHex on every iteration; only do so when needed.
        const std::string hashHex = toHex(hash);
        const int zeros = leadingZeroBytes(hashHex);
        if (zeros > bestZeros) {
            bestZeros = zeros;
            std::cout << "New best  nonce=" << std::setw(10) << std::dec << nonce
                      << "  leading-zero-bytes=" << zeros
                      << "  " << hashHex.substr(0, 24) << "...\n";
        }

        if (nonce % 1'000'000 == 0 && nonce > 0)
            std::cout << std::dec << nonce / 1'000'000
                      << "M nonces checked, best leading zeros: " << bestZeros << '\n';
    }

    if (!found)
        std::cout << "\nExhausted all " << NONCE_COUNT
                  << " nonces without finding a valid block.\n"
                  << "This is expected: real Bitcoin mining requires ASIC hardware.\n";

    cleanupHttpClient();
    return 0;
}
