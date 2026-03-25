#pragma once

#include <string>

// Returns the SHA-256 digest of data as a 32-byte binary string.
std::string sha256(const std::string& data);
