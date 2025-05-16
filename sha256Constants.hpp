#ifndef SHA256_CONSTANTS_H
#define SHA256_CONSTANTS_H

#include <array>
#include <cstdint>

// Size constants
constexpr uint32_t BLOCK_SIZE = 512;     // Size of each block in bits
constexpr uint32_t CHUNK_SIZE = 64;      // Size of each block in bytes
constexpr uint32_t HASH_SIZE = 256;      // Size of the final hash in bits
constexpr uint32_t LENGTH_FIELD_SIZE = 64; // Length field size in bits

// Initial Hash Values (H)
constexpr std::array<uint32_t, 8> INIT_HASH_VALUES = {
    htonl(0x6a09e667), htonl(0xbb67ae85),
    htonl(0x3c6ef372), htonl(0xa54ff53a),
    htonl(0x510e527f), htonl(0x9b05688c),
    htonl(0x1f83d9ab), htonl(0x5be0cd19)
};

// Round Constants (K) in network byte order (all with htonl)
constexpr std::array<uint32_t, 64> K = {
    htonl(0x428a2f98), htonl(0x71374491), htonl(0xb5c0fbcf), htonl(0xe9b5dba5),
    htonl(0x3956c25b), htonl(0x59f111f1), htonl(0x923f82a4), htonl(0xab1c5ed5),
    htonl(0xd807aa98), htonl(0x12835b01), htonl(0x243185be), htonl(0x550c7dc3),
    htonl(0x72be5d74), htonl(0x80deb1fe), htonl(0x9bdc06a7), htonl(0xc19bf174),
    htonl(0xe49b69c1), htonl(0xefbe4786), htonl(0x0fc19dc6), htonl(0x240ca1cc),
    htonl(0x2de92c6f), htonl(0x4a7484aa), htonl(0x5cb0a9dc), htonl(0x76f988da),
    htonl(0x983e5152), htonl(0xa831c66d), htonl(0xb00327c8), htonl(0xbf597fc7),
    htonl(0xc6e00bf3), htonl(0xd5a79147), htonl(0x06ca6351), htonl(0x14292967),
    htonl(0x27b70a85), htonl(0x2e1b2138), htonl(0x4d2c6dfc), htonl(0x53380d13),
    htonl(0x650a7354), htonl(0x766a0abb), htonl(0x81c2c92e), htonl(0x92722c85),
    htonl(0xa2bfe8a1), htonl(0xa81a664b), htonl(0xc24b8b70), htonl(0xc76c51a3),
    htonl(0xd192e819), htonl(0xd6990624), htonl(0xf40e3585), htonl(0x106aa070),
    htonl(0x19a4c116), htonl(0x1e376c08), htonl(0x2748774c), htonl(0x34b0bcb5),
    htonl(0x391c0cb3), htonl(0x4ed8aa4a), htonl(0x5b9cca4f), htonl(0x682e6ff3),
    htonl(0x748f82ee), htonl(0x78a5636f), htonl(0x84c87814), htonl(0x8cc70208),
    htonl(0x90befffa), htonl(0xa4506ceb), htonl(0xbef9a3f7), htonl(0xc67178f2)
};

#endif // SHA256_CONSTANTS_H