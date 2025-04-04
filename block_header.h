// block_header.h
#ifndef BLOCK_HEADER_H
#define BLOCK_HEADER_H

#include <cstdint>

class block_header {
public:
    uint32_t prev_hash[8];
    uint32_t merkle_root[8];
    uint32_t version;
    uint32_t time;
    uint32_t difficulty_target;
    uint32_t nonce;
};

#endif