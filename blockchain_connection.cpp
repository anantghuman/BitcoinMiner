#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include "json.hpp"
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace std;

// Structure to hold block information
struct BlockInfo {
    string hash;
    string previousBlockHash;
    uint32_t timestamp;
    uint32_t bits;
    uint32_t height;
    string merkleRoot;
};

// Callback function to write HTTP response data
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, string *response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

// Function to make HTTP GET request
string makeHttpRequest(const string& url) {
    CURL *curl;
    CURLcode res;
    string response;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Bitcoin-Miner/1.0");

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            cerr << "HTTP request failed: " << curl_easy_strerror(res) << endl;
            return "";
        }
    }
    return response;
}

// Parses Blockstream API JSON response and extracts block info
// Blockstream API: https://blockstream.info/api/block/{hash}
// Fields: id, previousblockhash, timestamp (unix), bits (decimal uint), height, merkle_root
BlockInfo parseBlockInfo(const string& jsonResponse) {
    BlockInfo blockInfo;

    try {
        auto root = nlohmann::json::parse(jsonResponse);
        blockInfo.hash              = root["id"].get<string>();
        blockInfo.previousBlockHash = root["previousblockhash"].get<string>();
        blockInfo.timestamp         = root["timestamp"].get<uint32_t>();
        blockInfo.bits              = root["bits"].get<uint32_t>();  // decimal uint from Blockstream
        blockInfo.height            = root["height"].get<uint32_t>();
        blockInfo.merkleRoot        = root["merkle_root"].get<string>();
    } catch (const exception& e) {
        cerr << "Failed to parse JSON response: " << e.what() << endl;
    }

    return blockInfo;
}

// Fetches the latest block from Blockstream.info API
BlockInfo getLatestBlock() {
    // Step 1: get the tip hash as plain text
    string tipUrl = "https://blockstream.info/api/blocks/tip/hash";
    string latestBlockHash = makeHttpRequest(tipUrl);

    if (latestBlockHash.empty()) {
        cerr << "Failed to get latest block hash" << endl;
        return BlockInfo();
    }

    // Trim any whitespace/newline
    while (!latestBlockHash.empty() && (latestBlockHash.back() == '\n' || latestBlockHash.back() == '\r' || latestBlockHash.back() == ' '))
        latestBlockHash.pop_back();

    // Step 2: get full block details
    string blockUrl = "https://blockstream.info/api/block/" + latestBlockHash;
    string blockResponse = makeHttpRequest(blockUrl);

    if (blockResponse.empty()) {
        cerr << "Failed to get block details" << endl;
        return BlockInfo();
    }

    return parseBlockInfo(blockResponse);
}

// Function to convert hex string to bytes
vector<uint8_t> hexStringToBytes(const string& hexStr) {
    vector<uint8_t> bytes;
    for (size_t i = 0; i + 1 < hexStr.length(); i += 2) {
        string byteString = hexStr.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(stoul(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// Reverses byte order of a hex string
string reverseHexString(const string& hexStr) {
    vector<uint8_t> bytes = hexStringToBytes(hexStr);
    reverse(bytes.begin(), bytes.end());

    stringstream ss;
    for (uint8_t byte : bytes) {
        ss << std::hex << setfill('0') << setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

// Main function to get mining information
BlockInfo getMiningInfo() {
    cout << "Fetching latest block from Blockstream API..." << endl;

    BlockInfo blockInfo = getLatestBlock();

    if (blockInfo.hash.empty()) {
        cerr << "Failed to fetch block information" << endl;
        return BlockInfo();
    }

    cout << "Latest Block Info:" << endl;
    cout << "  Hash:     " << blockInfo.hash << endl;
    cout << "  Previous: " << blockInfo.previousBlockHash << endl;
    cout << "  Height:   " << blockInfo.height << endl;
    cout << "  Time:     " << blockInfo.timestamp << endl;
    cout << "  Bits:     0x" << std::hex << blockInfo.bits << std::dec << endl;
    cout << "  MerkleRoot: " << blockInfo.merkleRoot << endl;

    return blockInfo;
}

// Function to create block header for mining
string createBlockHeader(const BlockInfo& prevBlock, const string& merkleRoot, uint32_t timestamp, uint32_t nonce) {
    string header;

    // Version (4 bytes, little-endian)
    uint32_t version = 4;
    for (int i = 0; i < 4; i++)
        header += static_cast<char>((version >> (8 * i)) & 0xFF);

    // Previous block hash (32 bytes, little-endian)
    vector<uint8_t> prevHashBytes = hexStringToBytes(prevBlock.hash);
    reverse(prevHashBytes.begin(), prevHashBytes.end());
    for (uint8_t byte : prevHashBytes)
        header += static_cast<char>(byte);

    // Merkle root (32 bytes, little-endian)
    vector<uint8_t> merkleBytes = hexStringToBytes(merkleRoot);
    reverse(merkleBytes.begin(), merkleBytes.end());
    for (uint8_t byte : merkleBytes)
        header += static_cast<char>(byte);

    // Timestamp (4 bytes, little-endian)
    for (int i = 0; i < 4; i++)
        header += static_cast<char>((timestamp >> (8 * i)) & 0xFF);

    // Bits (4 bytes, little-endian)
    for (int i = 0; i < 4; i++)
        header += static_cast<char>((prevBlock.bits >> (8 * i)) & 0xFF);

    // Nonce (4 bytes, little-endian)
    for (int i = 0; i < 4; i++)
        header += static_cast<char>((nonce >> (8 * i)) & 0xFF);

    return header;
}

// Initialize libcurl
void initializeHttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

// Cleanup libcurl
void cleanupHttpClient() {
    curl_global_cleanup();
}
