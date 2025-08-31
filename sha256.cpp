#include <array>
#include <bitset>
#include <cstdint>
#include <iostream>
#include <string>
#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <sstream>
#include <iomanip>


#include "sha256Constants.hpp"

using namespace std;

// g++ -std=c++11 sha256.cpp -o sha256
// ./sha256

uint32_t sigma0(uint32_t x);
uint32_t sigma1(uint32_t x);
uint32_t sigmaE0(array<uint32_t, 8>* hash);
uint32_t sigmaE1(array<uint32_t, 8>* hash);
uint32_t ch(array<uint32_t, 8>* hash);
uint32_t maj(array<uint32_t, 8>* hash);
uint32_t rotateRight(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

string sha256(string s) {
    string b = "";
    int n = s.size();
    int bSize = n * 8;
    int padding =
        ((BLOCK_SIZE - CHUNK_SIZE - 1) - bSize % BLOCK_SIZE + BLOCK_SIZE) %
        BLOCK_SIZE;

    b.reserve(bSize + padding + CHUNK_SIZE);
    for (char ch : s) {
        b.append(bitset<8>(ch).to_string());
    }

    b += '1';
    b.append(padding, '0');
    string lengthBits = bitset<CHUNK_SIZE>(bSize).to_string();
    b.append(lengthBits);

    int size = b.size();
    int numBlocks = size / BLOCK_SIZE;
    int start = 0;
    array<uint32_t, 8> hash;
    array<uint32_t, 8>* hash_ptr = &hash;
    array<uint32_t, 8> prev_hash = INIT_HASH_VALUES;
    for (int i = 0; i < 8; i++) {
        hash[i] = INIT_HASH_VALUES[i];
        prev_hash[i] = INIT_HASH_VALUES[i];
    }
    
    for (int i = 0; i < numBlocks; i++) {
        // Process each block
        array<uint32_t, 64> chunk;
        int j = 0;
        for (int k = 0; j < 16; j++) {
            chunk[j] = 0;
            for (int l = 0; l < 32; l++) {
                chunk[j] |= (b[start + k] - '0') << (31 - l);
                k++;
            }
        }
        for (; j < 64; j++) {
            chunk[j] = sigma1(chunk[j - 2]) + chunk[j - 7] +
                       sigma0(chunk[j - 15]) + chunk[j - 16];
        }
        for (int j = 0; j < 64; j++) {
            uint32_t T1 =
                hash[7] + sigmaE1(hash_ptr) + ch(hash_ptr) + K[j] + chunk[j];
            uint32_t T2 = sigmaE0(hash_ptr) + maj(hash_ptr);
            hash[7] = hash[6];
            hash[6] = hash[5];
            hash[5] = hash[4];
            hash[4] = hash[3] + T1;
            hash[3] = hash[2];
            hash[2] = hash[1];
            hash[1] = hash[0];
            hash[0] = T1 + T2;
        }

        hash[0] += prev_hash[0];
        hash[1] += prev_hash[1];
        hash[2] += prev_hash[2];
        hash[3] += prev_hash[3];
        hash[4] += prev_hash[4];
        hash[5] += prev_hash[5];
        hash[6] += prev_hash[6];
        hash[7] += prev_hash[7];
        prev_hash[0] = hash[0];
        prev_hash[1] = hash[1];
        prev_hash[2] = hash[2];
        prev_hash[3] = hash[3];
        prev_hash[4] = hash[4];
        prev_hash[5] = hash[5];
        prev_hash[6] = hash[6];
        prev_hash[7] = hash[7];

        start += BLOCK_SIZE;
    }
    
    string result;
    // for (int i = 0; i < 8; i++) {
    //     cout << bitset<32>(hash[i]).to_string();
    // }
    result.reserve(32);
    for (int i = 0; i < 8; i++) {
        result += static_cast<char>((hash[i] >> 24) & 0xFF);
        result += static_cast<char>((hash[i] >> 16) & 0xFF);
        result += static_cast<char>((hash[i] >> 8) & 0xFF);
        result += static_cast<char>((hash[i] >> 0) & 0xFF);
    }
    return result;
}


// int main() {
//     string s;
//     cout << "Enter a string: ";
//     cin >> s;
//     string str = sha256(s);
//     cout << str << endl;
//     return 0;
// }

uint32_t sigma0(uint32_t x) { return rotateRight(x, 7) ^ rotateRight(x, 18) ^ (x >> 3); }
uint32_t sigma1(uint32_t x) { return rotateRight(x, 17) ^ rotateRight(x, 19) ^ (x >> 10); }
uint32_t sigmaE0(array<uint32_t, 8>* hash) {
    return (rotateRight((*hash)[0], 2)) ^ rotateRight((*hash)[0], 13) ^ rotateRight((*hash)[0], 22);
}
uint32_t sigmaE1(array<uint32_t, 8>* hash) {
    return (rotateRight((*hash)[4], 6)) ^ (rotateRight((*hash)[4], 11)) ^ (rotateRight((*hash)[4],25));
}
uint32_t ch(array<uint32_t, 8>* hash) {
    return ((*hash)[4] & (*hash)[5]) ^ (~(*hash)[4] & (*hash)[6]);
}

uint32_t maj(array<uint32_t, 8>* hash) {
    return ((*hash)[0] & (*hash)[1]) ^ ((*hash)[0] & (*hash)[2]) ^
           ((*hash)[1] & (*hash)[2]);
}