#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <json/json.h>
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
        
        // Set user agent
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

// parses JSON response and extract block info
BlockInfo parseBlockInfo(const string& jsonResponse) {
    BlockInfo blockInfo;
    Json::Reader reader;
    Json::Value root;
    
    if (!reader.parse(jsonResponse, root)) {
        cerr << "Failed to parse JSON response" << endl;
        return blockInfo;
    }
    
    blockInfo.hash = root["hash"].asString();
    blockInfo.previousBlockHash = root["previousblockhash"].asString();
    blockInfo.timestamp = root["time"].asUInt();
    blockInfo.bits = stoul(root["bits"].asString(), 0, 16);
    blockInfo.height = root["height"].asUInt();
    blockInfo.merkleRoot = root["merkleroot"].asString();
    
    return blockInfo;
}

// Function to get latest block info from BlockCypher API
BlockInfo getLatestBlockFromBlockCypher() {
    string url = "https://api.blockcypher.com/v1/btc/main";
    string response = makeHttpRequest(url);
    
    if (response.empty()) {
        cerr << "Failed to get blockchain info" << endl;
        return BlockInfo();
    }
    
    Json::Reader reader;
    Json::Value root;
    
    if (!reader.parse(response, root)) {
        cerr << "Failed to parse blockchain info" << endl;
        return BlockInfo();
    }
    
    // Get the latest block hash
    string latestBlockHash = root["hash"].asString();
    
    // Now get detailed info about this block
    string blockUrl = "https://api.blockcypher.com/v1/btc/main/blocks/" + latestBlockHash;
    string blockResponse = makeHttpRequest(blockUrl);
    
    return parseBlockInfo(blockResponse);
}

// Function to convert hex string to bytes
vector<uint8_t> hexStringToBytes(const string& hex) {
    vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(stoul(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// Function to reverse byte order (for little-endian conversion)
string reverseHexString(const string& hex) {
    vector<uint8_t> bytes = hexStringToBytes(hex);
    reverse(bytes.begin(), bytes.end());
    
    stringstream ss;
    for (uint8_t byte : bytes) {
        ss << hex << setfill('0') << setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

// Main function to get mining information
BlockInfo getMiningInfo() {
    cout << "Fetching latest block information from BlockCypher API..." << endl;
    
    // Get latest block info from BlockCypher
    BlockInfo blockInfo = getLatestBlockFromBlockCypher();
    
    if (blockInfo.hash.empty()) {
        cerr << "Failed to fetch block information from BlockCypher API" << endl;
        return BlockInfo();
    }
    
    cout << "Latest Block Info:" << endl;
    cout << "Hash: " << blockInfo.hash << endl;
    cout << "Previous Block Hash: " << blockInfo.previousBlockHash << endl;
    cout << "Height: " << blockInfo.height << endl;
    cout << "Timestamp: " << blockInfo.timestamp << endl;
    cout << "Bits: 0x" << hex << blockInfo.bits << dec << endl;
    cout << "Merkle Root: " << blockInfo.merkleRoot << endl;
    
    return blockInfo;
}

// Function to create block header for mining
string createBlockHeader(const BlockInfo& prevBlock, const string& merkleRoot, uint32_t timestamp, uint32_t nonce) {
    string header;
    
    // Version (4 bytes, little-endian) - using version 1
    uint32_t version = 1;
    for (int i = 0; i < 4; i++) {
        header += static_cast<char>((version >> (8 * i)) & 0xFF);
    }
    
    // Previous block hash (32 bytes, little-endian)
    vector<uint8_t> prevHashBytes = hexStringToBytes(prevBlock.hash);
    reverse(prevHashBytes.begin(), prevHashBytes.end());
    for (uint8_t byte : prevHashBytes) {
        header += static_cast<char>(byte);
    }
    
    // Merkle root (32 bytes, little-endian)
    vector<uint8_t> merkleBytes = hexStringToBytes(merkleRoot);
    reverse(merkleBytes.begin(), merkleBytes.end());
    for (uint8_t byte : merkleBytes) {
        header += static_cast<char>(byte);
    }
    
    // Timestamp (4 bytes, little-endian)
    for (int i = 0; i < 4; i++) {
        header += static_cast<char>((timestamp >> (8 * i)) & 0xFF);
    }
    
    // Bits (4 bytes, little-endian)
    for (int i = 0; i < 4; i++) {
        header += static_cast<char>((prevBlock.bits >> (8 * i)) & 0xFF);
    }
    
    // Nonce (4 bytes, little-endian)
    for (int i = 0; i < 4; i++) {
        header += static_cast<char>((nonce >> (8 * i)) & 0xFF);
    }
    
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