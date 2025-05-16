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
    uint8_t prevhash[32] = {0};
    uint32_t time = htonl(1231006505);
    uint32_t nbits = htonl(0x1d00ffff);
    uint32_t version = htonl(1);
    uint32_t merkle_root[8] = {
        htonl(0x4a5e1e4b),  
        htonl(0xcd98713f), 
        htonl(0xf48a6e0f),
        htonl(0xa2a390e6),
        htonl(0x5d7a2f3e),
        htonl(0xe8a5ed6b),
        htonl(0x1fea149e),
        htonl(0xc19b8c1a)
    };
    uint32_t nonce = htonl(2083236893);
    uint32_t result[8];
    //hashblock(nonce, &version, &prevhash[0], &merkle_root[0], &time, &nbits, &result[0]);
    string header = "";
    header.append(reinterpret_cast<const char*>(&version), 4)
            .append(reinterpret_cast<const char*>(prevhash), 32)
            .append(reinterpret_cast<const char*>(&time), 4)
            .append(reinterpret_cast<const char*>(merkle_root), 32)
            .append(reinterpret_cast<const char*>(&nbits), 4)
            .append(reinterpret_cast<const char*>(&nonce), 4);
    cout << "header_length: " << header.length() << endl;
    string hash = doubleSHA256(header);
    string hexHash = HexString(hash);
    cout << hash << endl;
    return 1;
}
