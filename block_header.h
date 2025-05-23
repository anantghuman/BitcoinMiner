// block_header.h
#ifndef BLOCK_HEADER_H
#define BLOCK_HEADER_H

#include <cstdint>

class block_header {
public:
    uint32_t version = 1;
    uint32_t prev_hash[8];
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
    uint32_t time = 1231006505;
    uint32_t difficulty_target = 0x1d00ffff;
    uint32_t nonce = 2083236893;
};

#endif