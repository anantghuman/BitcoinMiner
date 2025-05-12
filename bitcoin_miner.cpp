#include <array>
#include <bitset>
#include <cstdint>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdlib>

#include "sha256Constants.hpp"
#include "sha256.cpp"
using namespace std;

// Helper function: Convert a binary string (the header) to a hex string for display.
string toHex(const string &data) {
    stringstream ss;
    for (unsigned char c : data)
        ss << setw(2) << setfill('0') << hex << (int)c;
    return ss.str();
}

// ----- BlockHeader structure -----
struct BlockHeader {
    uint32_t version;
    array<unsigned char, 32> prevHash;   // 32 bytes (previous block hash)
    array<unsigned char, 32> merkleRoot; // 32 bytes (Merkle root)
    uint32_t timestamp;
    uint32_t difficulty; // Compact difficulty (bits)
    uint32_t nonce;
};

// Serialize the BlockHeader into an 80-byte string. 
// Numeric fields are stored in little-endian.
string serializeBlockHeader(const BlockHeader &header) {
    string serialized;
    serialized.resize(80);
    int pos = 0;
    
    // Serialize version (4 bytes, little-endian)
    serialized[pos++] = header.version & 0xFF;
    serialized[pos++] = (header.version >> 8) & 0xFF;
    serialized[pos++] = (header.version >> 16) & 0xFF;
    serialized[pos++] = (header.version >> 24) & 0xFF;
    
    // Serialize previous block hash (32 bytes)
    for (int i = 0; i < 32; i++) {
        serialized[pos++] = header.prevHash[i];
    }
    
    // Serialize Merkle root (32 bytes)
    for (int i = 0; i < 32; i++) {
        serialized[pos++] = header.merkleRoot[i];
    }
    
    // Serialize timestamp (4 bytes, little-endian)
    serialized[pos++] = header.timestamp & 0xFF;
    serialized[pos++] = (header.timestamp >> 8) & 0xFF;
    serialized[pos++] = (header.timestamp >> 16) & 0xFF;
    serialized[pos++] = (header.timestamp >> 24) & 0xFF;
    
    // Serialize difficulty target (4 bytes, little-endian)
    serialized[pos++] = header.difficulty & 0xFF;
    serialized[pos++] = (header.difficulty >> 8) & 0xFF;
    serialized[pos++] = (header.difficulty >> 16) & 0xFF;
    serialized[pos++] = (header.difficulty >> 24) & 0xFF;
    
    // Serialize nonce (4 bytes, little-endian)
    serialized[pos++] = header.nonce & 0xFF;
    serialized[pos++] = (header.nonce >> 8) & 0xFF;
    serialized[pos++] = (header.nonce >> 16) & 0xFF;
    serialized[pos++] = (header.nonce >> 24) & 0xFF;
    
    return serialized;
}


string binaryStringToString(string& binaryString) {
    std::string result = "";
    for (size_t i = 0; i < binaryString.length(); i += 8) {
        std::string byte = binaryString.substr(i, 8);
        if (byte.length() != 8) {
            break;
        }
        int decimalValue = 0;
        for (int j = 0; j < 8; ++j) {
            if (byte[j] == '1') {
                decimalValue += (1 << (7 - j));
            }
        }
        result += char(decimalValue);
    }
    return result;
}
// Helper: Perform double SHA-256 (apply sha256 twice)
string doubleSHA256(const string &input) {
    return sha256(sha256(input));
}
void hashblock(uint32_t nonce, uint32_t* version, char* prevhash, uint32_t* merkle_root, uint32_t* time, uint32_t* nbits, uint32_t* result) {
   
}


int main() {
    char prevhash[] = "0000000000000000000000000000000000000000000000000000000000000000";
    uint32_t time = 1231006505;
    uint32_t nbits = 0x1d00ffff;
    uint32_t version = 1;
    uint32_t merkle_root[8] = {
      0x4a5e1e4b,
      0xaab89f3a,
      0x32518a88,
      0xc31bc87f,
      0x618f7667,
      0x3e2cc77a,
      0xb2127b7a,
      0xfdeda33b
    };
    uint32_t nonce = 2083236893;
    uint32_t result[8];
    hashblock(nonce, &version, &prevhash[0], &merkle_root[0], &time, &nbits, &result[0]);
    return 0;
}
