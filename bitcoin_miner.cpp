#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <climits>
#include <cstring>

#include "sha256.cpp"
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

// Converts the compact nBits field to a 32-byte big-endian target.
// The hash (byte-reversed after double-SHA256, i.e. display form) must be
// numerically less than this target for a block to be valid.
void bitsToTarget(uint32_t bits, uint8_t target[32]) {
    memset(target, 0, 32);
    uint32_t exp   = (bits >> 24) & 0xFF;
    uint32_t coeff = bits & 0x007FFFFF;

    // The 3-byte coefficient sits at byte offset (32 - exp) in the big-endian array.
    int pos = 32 - (int)exp;
    if (pos >= 0 && pos + 2 <= 31) {
        target[pos]     = (coeff >> 16) & 0xFF;
        target[pos + 1] = (coeff >>  8) & 0xFF;
        target[pos + 2] =  coeff        & 0xFF;
    }
}

// Returns true if the hash (already byte-reversed into display/big-endian form)
// is strictly less than the target produced by bitsToTarget().
bool meetsTarget(const string& hashBytes, const uint8_t target[32]) {
    for (int i = 0; i < 32; i++) {
        uint8_t h = static_cast<uint8_t>(hashBytes[i]);
        if (h < target[i]) return true;
        if (h > target[i]) return false;
    }
    return false; // equal is not strictly less
}

// Count leading zero bytes in display-form hash for progress reporting.
int leadingZeroBytes(const string& hashHex) {
    int count = 0;
    for (size_t i = 0; i + 1 < hashHex.size(); i += 2) {
        if (hashHex[i] == '0' && hashHex[i+1] == '0')
            count++;
        else
            break;
    }
    return count;
}

int main() {
    initializeHttpClient();

    BlockInfo latestBlock = getMiningInfo();

    if (latestBlock.hash.empty()) {
        cerr << "Failed to get latest block information. Exiting." << endl;
        cleanupHttpClient();
        return 1;
    }

    cout << "\n=== Starting Mining Process ===" << endl;
    cout << "Building on block #" << latestBlock.height
         << " -> mining block #" << latestBlock.height + 1 << endl;
    cout << "Previous hash: " << latestBlock.hash << endl;
    cout << "Merkle root:   " << latestBlock.merkleRoot << endl;

    // Compute the 256-bit difficulty target from the compact nBits field.
    uint8_t target[32];
    bitsToTarget(latestBlock.bits, target);

    cout << "Target (first 10 bytes): ";
    for (int i = 0; i < 10; i++)
        cout << hex << setfill('0') << setw(2) << (int)target[i];
    cout << "..." << dec << endl;

    // Build the 76-byte header prefix (everything except the 4-byte nonce).
    string headerPrefix;
    appendLE(headerPrefix, 4u);  // version 4

    // Previous block hash: 32 bytes little-endian
    vector<uint8_t> prevHashBytes = hexStringToBytes(latestBlock.hash);
    reverse(prevHashBytes.begin(), prevHashBytes.end());
    for (uint8_t byte : prevHashBytes)
        headerPrefix.push_back(static_cast<char>(byte));

    // Merkle root: 32 bytes little-endian (from the actual current block)
    vector<uint8_t> merkleBytes = hexStringToBytes(latestBlock.merkleRoot);
    reverse(merkleBytes.begin(), merkleBytes.end());
    for (uint8_t byte : merkleBytes)
        headerPrefix.push_back(static_cast<char>(byte));

    uint32_t currentTime = static_cast<uint32_t>(time(nullptr));
    appendLE(headerPrefix, currentTime);
    appendLE(headerPrefix, latestBlock.bits);

    cout << "\nSearching through all " << (uint64_t)UINT32_MAX + 1 << " nonces..." << endl;
    cout << "(Real Bitcoin difficulty is extremely high — this is for educational purposes.)" << endl;

    int bestZeros = 0;
    bool found = false;

    for (uint64_t nonce = 0; nonce <= UINT32_MAX; nonce++) {
        string header = headerPrefix;
        appendLE(header, static_cast<uint32_t>(nonce));

        string hash = doubleSHA256(header);
        reverse(hash.begin(), hash.end());  // display / big-endian form

        if (meetsTarget(hash, target)) {
            found = true;
            cout << "\n=== BLOCK FOUND! ===" << endl;
            cout << "Nonce: " << nonce << endl;
            cout << "Hash:  " << HexString(hash) << endl;
            break;
        }

        // Track and report the closest hash we've seen so far.
        string hashHex = HexString(hash);
        int zeros = leadingZeroBytes(hashHex);
        if (zeros > bestZeros) {
            bestZeros = zeros;
            cout << "Best so far — nonce " << nonce
                 << " leading zero bytes: " << zeros
                 << "  hash: " << hashHex.substr(0, 20) << "..." << endl;
        }

        if (nonce % 500000 == 0 && nonce > 0) {
            cout << "Progress: " << nonce / 1000000 << "M nonces checked, best leading zeros: "
                 << bestZeros << endl;
        }
    }

    if (!found) {
        cout << "\nExhausted all nonces (" << (uint64_t)UINT32_MAX + 1
             << " attempts) without finding a valid block." << endl;
        cout << "This is expected — real Bitcoin mining requires specialized hardware." << endl;
    }

    cleanupHttpClient();
    return 0;
}
