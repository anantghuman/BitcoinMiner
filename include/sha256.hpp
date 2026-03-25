#pragma once

#include <string>

/// Returns the SHA-256 digest of data as a 32-byte binary string.
///
/// The input may contain arbitrary bytes, including null bytes, because
/// std::string is used as a raw byte buffer throughout this codebase.
/// Output byte order follows the SHA-256 standard (big-endian, most-significant
/// byte first at index 0).
std::string sha256(const std::string& data);
