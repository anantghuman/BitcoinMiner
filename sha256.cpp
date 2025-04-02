#include <iostream>
#include <string>
#include <bitset>
#include <vector>
#include <array>
#include <cstdint>
#include "sha256Constants.hpp"

using namespace std;


// g++ -std=c++11 sha256.cpp -o sha256
// ./sha256

int sigma0(int x);
int sigma1(int x);

int main() {
    string s;
    cin >> s;

    int n = s.size();
    string b = "";
    int bSize = n * 8;
    int padding = ((BLOCK_SIZE - CHUNK_SIZE - 1) - bSize % BLOCK_SIZE + BLOCK_SIZE) % BLOCK_SIZE;
    //cout << padding << endl;

    b.reserve(bSize + padding + CHUNK_SIZE);
    for (char ch : s) {
        b.append(bitset<8>(ch).to_string());
    }
    // cout << b;
    //cout << b << endl;
    b += '1';
    //cout << b << endl;
    b.append(padding, '0');
    cout << b << endl;
    string lengthBits = bitset<CHUNK_SIZE>(bSize).to_string();
    b.append(lengthBits);

    int size = b.size();
    int numBlocks = size / BLOCK_SIZE;
    int start = 0;
    array<uint32_t, 64> chunk;
    int j = 0;
    for (int k = 0; j < 16; j++) {
        chunk[j] = 0;
        for (int l = 0; l < 32; l++) {
            chunk[j] |= (b[start + k] - '0') << (31 - l);
            k++;
        }
    }
    start += BLOCK_SIZE;

    for (; j < BLOCK_SIZE; j++) {

    }
}

int sigma0(int x) {
    return (x >> 7) ^ (x >> 18) ^ (x >> 3);
}
int sigma1(int x) {
    return (x >> 17) ^ (x >> 19) ^ (x >> 10);
}