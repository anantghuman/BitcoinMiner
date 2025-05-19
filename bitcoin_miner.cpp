#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <cstdint>

#include "sha256.cpp" // your SHA-256 implementation

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
    string header;

    appendLE(header, 1);

    header.append(32, '\0');

    uint8_t merkle_root_be[32] = {
        0x4a, 0x5e, 0x1e, 0x4b, 0xaa, 0xb8, 0x9f, 0x3a,
        0x32, 0x51, 0x8a, 0x88, 0xc3, 0x1b, 0xc8, 0x7f,
        0x61, 0x8f, 0x76, 0x67, 0x3e, 0x2c, 0xc7, 0x7a,
        0xb2, 0x12, 0x7b, 0x7a, 0xfd, 0xed, 0xa3, 0x3b
    };
    for (int i = 31; i >= 0; --i)
        header.push_back(static_cast<char>(merkle_root_be[i]));

    // Time
    appendLE(header, 1231006505);

    // Bits
    appendLE(header, 0x1d00ffff);

    // Nonce
    appendLE(header, 2083236893); // 0x7c2bac1d

    cout << "Header (hex): " << HexString(header) << endl;

    string hash = doubleSHA256(header);

    reverse(hash.begin(), hash.end());
    cout << "Block Hash (hex): " << HexString(hash) << endl;

    return 0;
}