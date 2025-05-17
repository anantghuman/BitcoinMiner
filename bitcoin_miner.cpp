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



string doubleSHA256(const string &input) {
    return sha256(sha256(input));
}

string HexString(const string& bytes) {
    stringstream ss;
    for (unsigned char c : bytes) {
        ss << hex << setfill('0') << setw(2) << (int)c;
    }
    return ss.str();
}


int main() {
    uint32_t version = htonl(0x01000000); // already little-endian so htonl is okay

    // Previous hash (32 zero bytes for genesis block)
    uint8_t prevhash[32] = {0};

    // Merkle root (must be in little-endian)
    uint8_t merkle_root[32] = {
        0x3b, 0xa3, 0xed, 0xfd, 0x7a, 0x7b, 0x12, 0xb2,
        0x7a, 0xc7, 0x2c, 0x3e, 0x67, 0x76, 0x8f, 0x61,
        0x7f, 0xc8, 0x1b, 0xc3, 0x88, 0x8a, 0x51, 0x32,
        0x3a, 0x9f, 0xb8, 0xaa, 0x4b, 0x1e, 0x5e, 0x4a
    };

    // Time
    uint32_t time = (1231006505); 

    // Difficulty bits
    uint32_t nbits = (0x1d00ffff);

    // Nonce
    uint32_t nonce = (0x7c2bac1d); 

    // Assemble header
    string header;
    header.append(reinterpret_cast<const char*>(&version), 4);
    header.append(reinterpret_cast<const char*>(prevhash), 32);
    header.append(reinterpret_cast<const char*>(merkle_root), 32);
    header.append(reinterpret_cast<const char*>(&time), 4);
    header.append(reinterpret_cast<const char*>(&nbits), 4);
    header.append(reinterpret_cast<const char*>(&nonce), 4);

    string hexHeader = HexString(header);
    cout << hexHeader << endl;
    string hash = doubleSHA256(header);
    string hexHash = HexString(hash);
    cout << hexHash << endl;
    return 1;
}
