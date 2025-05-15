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
void hashblock(uint32_t nonce, uint32_t* version, char* prevhash, uint32_t* merkle_root, uint32_t* time, uint32_t* nbits, uint32_t* result) {
    string header = "";
   

   
}


int main() {
    char prevhash[] = "0000000000000000000000000000000000000000000000000000000000000000";
    uint32_t time = 1231006505;
    uint32_t nbits = 0x1d00ffff;
    uint32_t version = 1;
    uint32_t merkle_root[8] = {
      0x4a5e1e4b,
      0xaab89f3a,
      0x32518a88,
      0xc31bc87f,
      0x618f7667,
      0x3e2cc77a,
      0xb2127b7a,
      0xfdeda33b
    };
    uint32_t nonce = 2083236893;
    uint32_t result[8];
    hashblock(nonce, &version, &prevhash[0], &merkle_root[0], &time, &nbits, &result[0]);
    return 0;
}
