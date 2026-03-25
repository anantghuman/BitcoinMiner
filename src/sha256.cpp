#include "sha256.hpp"
#include "sha256_constants.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

inline uint32_t rotr(uint32_t x, uint32_t n) noexcept {
    return (x >> n) | (x << (32u - n));
}

// Message-schedule functions (σ0, σ1) and compression-round functions (Σ0, Σ1, Ch, Maj).
// Names match FIPS 180-4 §4.1.2.
inline uint32_t sigma0(uint32_t x) noexcept { return rotr(x,  7) ^ rotr(x, 18) ^ (x >>  3); }
inline uint32_t sigma1(uint32_t x) noexcept { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }
inline uint32_t Sigma0(uint32_t x) noexcept { return rotr(x,  2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint32_t Sigma1(uint32_t x) noexcept { return rotr(x,  6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline uint32_t ch (uint32_t e, uint32_t f, uint32_t g) noexcept { return (e & f) ^ (~e & g); }
inline uint32_t maj(uint32_t a, uint32_t b, uint32_t c) noexcept { return (a & b) ^ (a & c) ^ (b & c); }

inline uint32_t loadBE32(const uint8_t* p) noexcept {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
         | (static_cast<uint32_t>(p[2]) <<  8) |  static_cast<uint32_t>(p[3]);
}

inline void storeBE32(uint8_t* p, uint32_t v) noexcept {
    p[0] = static_cast<uint8_t>(v >> 24); p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >>  8); p[3] = static_cast<uint8_t>(v);
}

} // namespace

std::string sha256(const std::string& input) {
    const std::size_t len = input.size();

    // Pad to a multiple of 64 bytes: [msg | 0x80 | zeros | 8-byte big-endian bit-length].
    const std::size_t paddedLen =
        ((len + 9 + SHA256_BLOCK_BYTES - 1) / SHA256_BLOCK_BYTES) * SHA256_BLOCK_BYTES;

    std::vector<uint8_t> msg(paddedLen, 0x00);
    std::memcpy(msg.data(), input.data(), len);
    msg[len] = 0x80;

    const uint64_t bitLen = static_cast<uint64_t>(len) * 8;
    for (int i = 0; i < 8; ++i)
        msg[paddedLen - 8 + i] = static_cast<uint8_t>(bitLen >> (56 - 8 * i));

    std::array<uint32_t, 8> h = SHA256_INIT;

    const std::size_t numBlocks = paddedLen / SHA256_BLOCK_BYTES;
    for (std::size_t blk = 0; blk < numBlocks; ++blk) {
        const uint8_t* blkPtr = msg.data() + blk * SHA256_BLOCK_BYTES;

        std::array<uint32_t, 64> w;
        for (int i = 0;  i < 16; ++i) w[i] = loadBE32(blkPtr + i * 4);
        for (int i = 16; i < 64; ++i)
            w[i] = sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16];

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i) {
            const uint32_t T1 = hh + Sigma1(e) + ch(e, f, g) + SHA256_K[i] + w[i];
            const uint32_t T2 = Sigma0(a) + maj(a, b, c);
            hh = g;  g = f;  f = e;  e = d + T1;
            d  = c;  c = b;  b = a;  a = T1 + T2;
        }

        h[0] += a;  h[1] += b;  h[2] += c;  h[3] += d;
        h[4] += e;  h[5] += f;  h[6] += g;  h[7] += hh;
    }

    std::string digest(SHA256_DIGEST_BYTES, '\0');
    for (int i = 0; i < 8; ++i)
        storeBE32(reinterpret_cast<uint8_t*>(&digest[i * 4]), h[i]);

    return digest;
}
