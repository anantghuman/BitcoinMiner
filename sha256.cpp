#include <iostream>
#include <string>
#include <bitset>
#include <vector>
#include "sha256Constants.h"
using namespace std;
using namespace SHA256Constants;


int main() {
    string s;
    cin >> s;

    int n = s.size();
    string b = "";
    int bSize = n * 8;
    int padding = ((BLOCK_SIZE - CHUNK_SIZE) -  bSize % BLOCK_SIZE + BLOCK_SIZE) % BLOCK_SIZE;
    b.reserve(bSize + padding + CHUNK_SIZE + 1);
    for (char ch : s) {
        b.append(bitset<8>(ch).to_string());
    }

    b += '1';

    

    b.append(padding, '0');    
    string lengthBits = bitset<64>(bSize).to_string();
    b.append(lengthBits);

    int totalSize = bSize + padding + CHUNK_SIZE;
}

void extendChunks(vector<uint32_t>& W) {
    for (int i = 16; i < 64; i++) {
        W[i] = sigma1(W[i - 2]) + W[i - 7] + sigma0(W[i - 15]) + W[i - 16];
    }
}

vector<vector<uint32_t>> processChunks(const string& binaryMessage) {
    vector<vector<uint32_t>> chunkWords;  // Vector to store words for each chunk

    // Process each 512-bit chunk
    for (size_t i = 0; i < binaryMessage.size(); i += 512) {
        string chunk = binaryMessage.substr(i, 512);  // Get the next 512-bit chunk
        vector<uint32_t> words = getFirstWords(chunk);  // Get the first 16 words of the chunk
        chunkWords.push_back(words);  // Add the words to the result
    }

    return chunkWords;  // Return the list of chunk words
}

vector<uint32_t> getFirstWords(const string& binaryMessage) {
    vector<uint32_t> words(16 ,0);
    // Iterate over the first 512 bits of the message and split it into 32-bit words
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


