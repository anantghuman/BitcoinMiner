#ifndef BLOCKCHAIN_CONNECTION_HPP
#define BLOCKCHAIN_CONNECTION_HPP

#include <string>
#include <vector>
#include <cstdint>

// Structure to hold block information
struct BlockInfo {
    std::string hash;
    std::string previousBlockHash;
    uint32_t timestamp;
    uint32_t bits;
    uint32_t height;
    std::string merkleRoot;
};

// Function declarations
void initializeHttpClient();
void cleanupHttpClient();
BlockInfo getMiningInfo();
std::string createBlockHeader(const BlockInfo& prevBlock, const std::string& merkleRoot, uint32_t timestamp, uint32_t nonce);
std::vector<uint8_t> hexStringToBytes(const std::string& hex);
std::string reverseHexString(const std::string& hex);

#endif // BLOCKCHAIN_CONNECTION_HPP
