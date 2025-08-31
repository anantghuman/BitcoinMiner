#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <cstdint>
#include <ctime>

#include "sha256.cpp" // your SHA-256 implementation
#include "blockchain_connection.hpp"

using namespace std;

string doubleSHA256(const string &input) {
    return sha256(sha256(input));
}

string HexString(const string& bytes) {
    stringstream ss;
    for (unsigned char c : bytes)
        ss << hex << setfill('0') << setw(2) << (int)c;
    return ss.str();
}

void appendLE(string& s, uint32_t val) {
    for (int i = 0; i < 4; i++)
        s += static_cast<char>((val >> (8 * i)) & 0xFF);
}

int main() {
    // Initialize HTTP client for blockchain connection
    initializeHttpClient();
    
    // Get latest block information from the blockchain
    BlockInfo latestBlock = getMiningInfo();
    
    if (latestBlock.hash.empty()) {
        cerr << "Failed to get latest block information. Exiting." << endl;
        cleanupHttpClient();
        return 1;
    }
    
    cout << "\n=== Starting Mining Process ===" << endl;
    cout << "Mining next block after: " << latestBlock.hash << endl;
    cout << "Block height: " << latestBlock.height + 1 << endl;
    
    string header;

    // Version (4 bytes, little-endian) - using version 1
    appendLE(header, 1);

    // Previous block hash (32 bytes, little-endian)
    vector<uint8_t> prevHashBytes = hexStringToBytes(latestBlock.hash);
    reverse(prevHashBytes.begin(), prevHashBytes.end());
    for (uint8_t byte : prevHashBytes) {
        header.push_back(static_cast<char>(byte));
    }

    // Merkle root (for now using a placeholder)
    uint8_t merkle_root_be[32] = {
        0x4a, 0x5e, 0x1e, 0x4b, 0xaa, 0xb8, 0x9f, 0x3a,
        0x32, 0x51, 0x8a, 0x88, 0xc3, 0x1b, 0xc8, 0x7f,
        0x61, 0x8f, 0x76, 0x67, 0x3e, 0x2c, 0xc7, 0x7a,
        0xb2, 0x12, 0x7b, 0x7a, 0xfd, 0xed, 0xa3, 0x3b
    };
    for (int i = 31; i >= 0; --i)
        header.push_back(static_cast<char>(merkle_root_be[i]));

    uint32_t currentTime = static_cast<uint32_t>(time(nullptr));
    appendLE(header, currentTime);

    appendLE(header, latestBlock.bits);

    cout << "\nStarting mining with current difficulty..." << endl;
    uint32_t nonce = 0;
    bool found = false;
    
    while (!found && nonce < 1000000) { // Limit iterations for demo
        string currentHeader = header;
        appendLE(currentHeader, nonce);
        
        string hash = doubleSHA256(currentHeader);
        reverse(hash.begin(), hash.end());
        
        // Check if hash meets difficulty target (simplified check - just look for leading zeros)
        string hashHex = HexString(hash);
        
        if (nonce % 100000 == 0) {
            cout << "Nonce: " << nonce << " Hash: " << hashHex << endl;
        }
        
        // For demonstration, we'll consider any hash with 4+ leading zeros as "found"
        // (Real Bitcoin mining requires much more leading zeros)
        if (hashHex.substr(0, 4) == "0000") {
            found = true;
            cout << "\n=== BLOCK FOUND! ===" << endl;
            cout << "Winning nonce: " << nonce << endl;
            cout << "Block hash: " << hashHex << endl;
            cout << "Header (hex): " << HexString(currentHeader) << endl;
        }
        
        nonce++;
    }
    
    if (!found) {
        cout << "\nNo block found in " << nonce << " attempts." << endl;
        cout << "In real mining, you would continue with more nonces or get new block template." << endl;
    }

    // Cleanup
    cleanupHttpClient();
    return 0;
}