#include "blockchain.hpp"
#include "sha256.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::string doubleSHA256(const std::string& data) {
    return sha256(sha256(data));
}

static std::string toHex(const std::string& bytes) {
    std::ostringstream oss;
    for (unsigned char c : bytes)
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(c);
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

// hash is the byte-reversed (display-form) digest; comparison is big-endian.
static bool meetsTarget(const std::string& hash,
                        const std::array<uint8_t, 32>& target) {
    for (std::size_t i = 0; i < 32; ++i) {
        const auto h = static_cast<uint8_t>(hash[i]);
        if (h < target[i]) return true;
        if (h > target[i]) return false;
    }
    return false;
}

static int leadingZeroBytes(const std::string& hashHex) {
    int count = 0;
    for (std::size_t i = 0; i + 1 < hashHex.size(); i += 2) {
        if (hashHex[i] == '0' && hashHex[i + 1] == '0') ++count;
        else break;
    }
    return count;
}

int main() {
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

    constexpr uint64_t NONCE_COUNT = static_cast<uint64_t>(UINT32_MAX) + 1;
    std::cout << "\nSearching " << NONCE_COUNT << " nonces...\n\n";

    int  bestZeros = 0;
    bool found     = false;

    for (uint64_t nonce = 0; nonce < NONCE_COUNT; ++nonce) {
        std::string header = prefix;
        appendLE32(header, static_cast<uint32_t>(nonce));

        // Double-SHA256, then byte-reverse to get the display-form digest.
        std::string hash = doubleSHA256(header);
        std::reverse(hash.begin(), hash.end());

        if (meetsTarget(hash, target)) {
            found = true;
            std::cout << "\n=== BLOCK FOUND ===\n"
                      << "Nonce : " << nonce       << '\n'
                      << "Hash  : " << toHex(hash) << '\n';
            break;
        }

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
        std::cout << "\nExhausted all " << NONCE_COUNT << " nonces. No block found.\n"
                  << "Expected: real Bitcoin requires ASIC hardware.\n";

    cleanupHttpClient();
    return 0;
}
