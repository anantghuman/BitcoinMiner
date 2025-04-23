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

int main() {
    // ----- Given Genesis Block Constant Values -----
    uint32_t version = 1;
    uint32_t merkle_values[8] = {
      0x4a5e1e4b,
      0xaab89f3a,
      0x32518a88,
      0xc31bc87f,
      0x618f7667,
      0x3e2cc77a,
      0xb2127b7a,
      0xfdeda33b
    };
    uint32_t timestamp = 1231006505;
    uint32_t difficulty_target = 0x1d00ffff;
    uint32_t nonce = 2083236893;
    // For genesis block: previous hash is 32 bytes of zeros.
    string prev_hash_str(32, '\0'); // This constructs a string of 32 null (zero) bytes.
    
    // ----- Construct the BlockHeader structure -----
    BlockHeader genesis;
    genesis.version = version;
    
    // Fill prevHash with 32 zero bytes.
    for (int i = 0; i < 32; i++) {
        genesis.prevHash[i] = prev_hash_str[i];
    }
    
    // Merge the eight 32-bit values into a 32-byte Merkle root (little-endian).
    int pos = 0;
    for (int j = 0; j < 8; j++) {
        uint32_t val = merkle_values[j];
        genesis.merkleRoot[pos++] = val & 0xFF;
        genesis.merkleRoot[pos++] = (val >> 8) & 0xFF;
        genesis.merkleRoot[pos++] = (val >> 16) & 0xFF;
        genesis.merkleRoot[pos++] = (val >> 24) & 0xFF;
    }
    
    genesis.timestamp = timestamp;
    genesis.difficulty = difficulty_target;
    genesis.nonce = nonce;
    
    // ----- Next Step: Serialize the Block Header -----
    string serializedHeader = serializeBlockHeader(genesis);
    cout << "Serialized Block Header:\n" << toHex(serializedHeader) << "\n" << endl;
    
    // ----- Next Step: Compute the Double SHA-256 Hash -----
    string genesisHash = doubleSHA256(serializedHeader);
    cout << "Genesis Block Hash:\n" << genesisHash << "\n" << endl;
    
    // ----- Optional: Implement a Simple Mining Loop -----
    // For demonstration, we set a simplified target. In a real scenario, the target would
    // be derived from the difficulty.
    uint32_t target = 0x00000fff; // This is a simplified target for test purposes.
    BlockHeader candidate = genesis; // Start with the genesis header as our candidate.
    
    while (true) {
        string candidateSerialized = serializeBlockHeader(candidate);
        string candidateHash = doubleSHA256(candidateSerialized);
        
        // Simplified check: interpret first 8 hex digits as a 32-bit unsigned integer.
        string hashPrefix = candidateHash.substr(0, 8);
        uint32_t value = stoul(hashPrefix, nullptr, 16);
        
        if (value < target) {
            cout << "Valid block found!" << endl;
            cout << "Nonce: " << candidate.nonce << endl;
            cout << "Block Hash: " << candidateHash << endl;
            break;
        } else {
            candidate.nonce++;
            // If nonce wraps around, update the timestamp (not a real rule, but for demonstration)
            if (candidate.nonce == 0) {
                candidate.timestamp++;
            }
        }
    }
    
    return 0;
}
