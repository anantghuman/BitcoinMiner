#pragma once

#include <iosfwd>
#include <vector>

struct CliOptions {
    bool             showHelp{false};
    bool             listGpus{false};
    std::vector<int> gpuIds;  // empty = use all detected GPUs
};

// Parses command-line options. Throws std::invalid_argument on malformed flags/values.
CliOptions parseCliOptions(int argc, char* argv[]);

void printUsage(const char* progName, std::ostream& os);
