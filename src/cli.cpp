#include "cli.hpp"

#include <cctype>
#include <climits>
#include <ostream>
#include <stdexcept>
#include <string>

namespace {

int parseNonNegativeInt(const std::string& token) {
    if (token.empty())
        throw std::invalid_argument("Empty GPU device ID in --gpus list");

    for (unsigned char ch : token) {
        if (!std::isdigit(ch)) {
            throw std::invalid_argument(
                "Invalid GPU device ID '" + token + "': expected non-negative integer");
        }
    }

    try {
        const long long val = std::stoll(token);
        if (val > INT_MAX)
            throw std::invalid_argument("GPU device ID out of range: " + token);
        return static_cast<int>(val);
    } catch (const std::out_of_range&) {
        throw std::invalid_argument("GPU device ID out of range: " + token);
    }
}

void appendGpuIdList(const std::string& spec, std::vector<int>& out) {
    if (spec.empty())
        throw std::invalid_argument("--gpus requires a non-empty comma-separated list");

    std::size_t start = 0;
    while (true) {
        const std::size_t comma = spec.find(',', start);
        if (comma == std::string::npos) {
            out.push_back(parseNonNegativeInt(spec.substr(start)));
            break;
        }
        out.push_back(parseNonNegativeInt(spec.substr(start, comma - start)));
        start = comma + 1;
    }
}

} // namespace

CliOptions parseCliOptions(int argc, char* argv[]) {
    CliOptions opts;
    constexpr const char* GPUS_PREFIX = "--gpus=";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            opts.showHelp = true;
        } else if (arg == "--list-gpus") {
            opts.listGpus = true;
        } else if (arg == "--gpus") {
            if (i + 1 >= argc)
                throw std::invalid_argument("--gpus requires a value (e.g. --gpus 0,1)");
            appendGpuIdList(argv[++i], opts.gpuIds);
        } else if (arg.rfind(GPUS_PREFIX, 0) == 0) {
            appendGpuIdList(arg.substr(std::string(GPUS_PREFIX).size()), opts.gpuIds);
        } else {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }

    return opts;
}

void printUsage(const char* progName, std::ostream& os) {
    const char* name = (progName && *progName) ? progName : "bitcoin_miner";
    os << "Usage: " << name << " [options]\n\n"
       << "Options:\n"
       << "  --gpus <id0,id1,...>   Comma-separated CUDA device IDs to use (GPU build; default: all)\n"
       << "  --list-gpus            List detected CUDA devices and exit (GPU build)\n"
       << "  -h, --help             Show this help message and exit\n";
}
