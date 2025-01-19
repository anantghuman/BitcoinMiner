#include <iostream>
#include <string>
#include <bitset>
#include <vector>
#include <array>
#include <cstdint>
#include "sha256Constants.hpp"

using namespace std;

vector<uint32_t> getFirstWords(const string& binaryMessage);
void extendChunks(vector<uint32_t>& W);
vector<vector<uint32_t>> processChunks(const string& binaryMessage);
void compress(const vector<uint32_t>& W, array<uint32_t, 8>& H);
uint32_t ROTR(uint32_t x, uint32_t n);
uint32_t sigma0(uint32_t x);
uint32_t sigma1(uint32_t x);
uint32_t Sigma0(uint32_t x);
uint32_t Sigma1(uint32_t x);
uint32_t Ch(uint32_t x, uint32_t y, uint32_t z);
uint32_t Maj(uint32_t x, uint32_t y, uint32_t z);

// g++ -std=c++11 sha256.cpp -o sha256
// ./sha256

int main() {
    string s;
    cin >> s;

    int n = s.size();
    string b = "";
    int bSize = n * 8 + 1;
    int padding = ((BLOCK_SIZE - CHUNK_SIZE) - bSize % BLOCK_SIZE + BLOCK_SIZE) % BLOCK_SIZE;

    b.reserve(bSize + padding + CHUNK_SIZE + 1);
    for (char ch : s) {
        b.append(bitset<8>(ch).to_string());
    }
    cout << b;

    b += '1';

    b.append(padding, '0');
    string lengthBits = bitset<LENGTH_FIELD_SIZE>(bSize).to_string();
    b.append(lengthBits);

    int totalSize = bSize + padding + CHUNK_SIZE;
    vector<vector<uint32_t>> chunks = processChunks(b);
    array<uint32_t, 8> H = INIT_HASH_VALUES;
    for (const auto& chunk : chunks) {
        compress(chunk, H);
    }
    for (uint32_t h : H) {
        printf("%08x", h);
    }
    
}
void compress(const vector<uint32_t>& W, array<uint32_t, 8>& H) {
    uint32_t a = H[0],  b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t T1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];
        uint32_t T2 = Sigma0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    // Update the hash values
    H[0] += a;
    H[1] += b;
    H[2] += c;
    H[3] += d;
    H[4] += e;
    H[5] += f;
    H[6] += g;
    H[7] += h;
}

void extendChunks(vector<uint32_t>& W) {
    for (int i = 16; i < 64; ++i) {
        W[i] = sigma1(W[i - 2]) + W[i - 7] + sigma0(W[i - 15]) + W[i - 16];
    }
}

vector<vector<uint32_t>> processChunks(const string& binaryMessage) {
    vector<vector<uint32_t>> chunkWords;

    for (size_t i = 0; i < binaryMessage.size(); i += BLOCK_SIZE) {
        string chunk = binaryMessage.substr(i, BLOCK_SIZE);
        vector<uint32_t> words = getFirstWords(chunk);
        extendChunks(words);
        chunkWords.push_back(words);
    }

    return chunkWords;
}

vector<uint32_t> getFirstWords(const string& binaryMessage) {
    vector<uint32_t> words(64, 0);
    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 32; ++j) {
            words[i] |= (binaryMessage[i * 32 + j] == '1') << (31 - j);
        }
    }
    return words;
}

uint32_t ROTR(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

uint32_t sigma0(uint32_t x) {
    return ROTR(x, 7) ^ ROTR(x, 18) ^ (x >> 3);
}

uint32_t sigma1(uint32_t x) {
    return ROTR(x, 17) ^ ROTR(x, 19) ^ (x >> 10);
}

uint32_t Sigma0(uint32_t x) {
    return ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22);
}

uint32_t Sigma1(uint32_t x) {
    return ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25);
}

uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}
