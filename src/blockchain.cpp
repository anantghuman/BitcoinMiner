#include "blockchain.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <curl/curl.h>
#include "json.hpp"

namespace {

std::size_t curlWriteCallback(void* contents, std::size_t size,
                               std::size_t nmemb, std::string* out) {
    out->append(reinterpret_cast<const char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string httpGet(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[blockchain] curl_easy_init() failed\n";
        return {};
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "BitcoinMiner/1.0");

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[blockchain] GET " << url << " failed: "
                  << curl_easy_strerror(res) << '\n';
        return {};
    }
    return response;
}

BlockInfo parseBlock(const std::string& json) {
    BlockInfo info;
    try {
        const auto root        = nlohmann::json::parse(json);
        info.hash              = root.at("id").get<std::string>();
        info.previousBlockHash = root.at("previousblockhash").get<std::string>();
        info.merkleRoot        = root.at("merkle_root").get<std::string>();
        info.version           = root.at("version").get<uint32_t>();
        info.timestamp         = root.at("timestamp").get<uint32_t>();
        info.bits              = root.at("bits").get<uint32_t>();
        info.height            = root.at("height").get<uint32_t>();
    } catch (const std::exception& ex) {
        std::cerr << "[blockchain] JSON parse error: " << ex.what() << '\n';
        info = BlockInfo{};
    }
    return info;
}

BlockInfo fetchChainTip() {
    std::string tipHash = httpGet("https://blockstream.info/api/blocks/tip/hash");
    if (tipHash.empty()) return {};

    while (!tipHash.empty() && (tipHash.back() == '\n' || tipHash.back() == '\r'
                                || tipHash.back() == ' '))
        tipHash.pop_back();

    const std::string blockJson =
        httpGet("https://blockstream.info/api/block/" + tipHash);
    if (blockJson.empty()) return {};

    return parseBlock(blockJson);
}

} // namespace

void initializeHttpClient() { curl_global_init(CURL_GLOBAL_DEFAULT); }
void cleanupHttpClient()     { curl_global_cleanup(); }

BlockInfo getMiningInfo() {
    std::cout << "Fetching chain tip from blockstream.info...\n";

    const BlockInfo tip = fetchChainTip();
    if (tip.hash.empty()) {
        std::cerr << "[blockchain] Failed to retrieve chain tip.\n";
        return {};
    }

    std::cout << "  Height    : " << tip.height            << '\n'
              << "  Hash      : " << tip.hash              << '\n'
              << "  PrevHash  : " << tip.previousBlockHash << '\n'
              << "  MerkleRoot: " << tip.merkleRoot        << '\n'
              << "  Timestamp : " << tip.timestamp         << '\n'
              << "  Bits      : 0x" << std::hex << tip.bits    << std::dec << '\n'
              << "  Version   : 0x" << std::hex << tip.version << std::dec << '\n';

    return tip;
}

std::vector<uint8_t> hexStringToBytes(const std::string& hex) {
    auto nibble = [](char c) noexcept -> uint8_t {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
        return 0;
    };

    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
        bytes.push_back(static_cast<uint8_t>((nibble(hex[i]) << 4) | nibble(hex[i + 1])));

    return bytes;
}
