# Bitcoin Miner in C++

A high-performance Bitcoin mining implementation written in C++ that connects to the Bitcoin blockchain to fetch the latest block information and attempt to mine new blocks.

## Features

- **Blockchain Integration**: Connects to Bitcoin APIs to fetch real-time block information
- **SHA-256 Implementation**: Custom SHA-256 hashing for block mining
- **BlockCypher API**: Uses BlockCypher API for reliable blockchain data
- **Block Header Construction**: Properly formats block headers for mining attempts
- **Nonce Iteration**: Systematic nonce testing for proof-of-work

## Project Structure

```
bitcoinMiner/
├── bitcoin_miner.cpp          # Main mining logic and algorithms
├── blockchain_connection.cpp   # Blockchain API integration
├── blockchain_connection.hpp   # Header file for blockchain functions
├── sha256.cpp                 # SHA-256 hashing implementation
├── sha256Constants.hpp        # SHA-256 constants and definitions
├── main                       # Compiled executable
├── Makefile                   # Build configuration
└── README.md                  # This file
```

## Dependencies

Before building the project, you need to install the following dependencies:

### macOS (using Homebrew)
```bash
brew install curl
brew install jsoncpp
```

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install libcurl4-openssl-dev libjsoncpp-dev
```

### CentOS/RHEL
```bash
sudo yum install libcurl-devel jsoncpp-devel
```

## Building the Project

1. Clone the repository:
```bash
git clone https://github.com/anantghuman/BitcoinMiner.git
cd BitcoinMiner
```

2. Build using the Makefile:
```bash
make
```

3. Run the miner:
```bash
./main
```

## How It Works

### 1. Blockchain Connection
The miner connects to Bitcoin blockchain APIs to fetch:
- Latest block hash
- Previous block hash
- Block height
- Timestamp
- Difficulty bits
- Merkle root

### 2. Block Header Construction
Creates a properly formatted 80-byte block header containing:
- Version (4 bytes)
- Previous block hash (32 bytes)
- Merkle root (32 bytes)
- Timestamp (4 bytes)
- Difficulty bits (4 bytes)
- Nonce (4 bytes)

### 3. Mining Process
- Iterates through nonce values
- Computes double SHA-256 hash of block header
- Checks if hash meets difficulty target
- Reports successful mining attempts

## API Endpoint Used

- **BlockCypher API**: `https://api.blockcypher.com/v1/btc/main` - Provides reliable Bitcoin blockchain data

## Important Notes

⚠️ **Educational Purpose**: This is a simplified Bitcoin miner for educational purposes. Real Bitcoin mining requires:
- Specialized ASIC hardware
- Mining pool participation
- Significant computational resources
- Economic considerations

⚠️ **Network Compliance**: This miner fetches real blockchain data but mining solo with CPU is not economically viable on the main Bitcoin network.

## Code Examples

### Fetching Latest Block Information
```cpp
#include "blockchain_connection.hpp"

// Initialize HTTP client
initializeHttpClient();

// Get latest block info
BlockInfo latestBlock = getMiningInfo();

// Use block info for mining
string blockHeader = createBlockHeader(latestBlock, merkleRoot, timestamp, nonce);
```

### Mining Loop (Simplified)
```cpp
uint32_t nonce = 0;
while (true) {
    string header = createBlockHeader(prevBlock, merkleRoot, timestamp, nonce);
    string hash = sha256(sha256(header));
    
    if (meetsTarget(hash, difficulty)) {
        cout << "Block mined! Nonce: " << nonce << endl;
        break;
    }
    nonce++;
}
```

## Performance Considerations

- **CPU Mining**: Uses CPU for hashing (educational implementation)
- **Hash Rate**: Typical performance varies by CPU (1-10 MH/s range)
- **Memory Usage**: Minimal memory footprint
- **Network Calls**: Periodic API calls to fetch latest block data

## Troubleshooting

### Common Issues

1. **Compilation Errors**:
   - Ensure libcurl and libjsoncpp are installed
   - Check compiler version (requires C++11 or later)

2. **Network Errors**:
   - Verify internet connection
   - Check if APIs are accessible
   - Some networks may block cryptocurrency-related requests

3. **JSON Parsing Errors**:
   - API response format may change
   - Network timeouts can cause empty responses

### Debug Mode
Add debug flags during compilation:
```bash
make CFLAGS="-DDEBUG -g"
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is for educational purposes. Please ensure compliance with local regulations regarding cryptocurrency mining.

## Disclaimer

This software is provided for educational and research purposes only. The authors are not responsible for any financial losses, hardware damage, or legal issues arising from the use of this software. Bitcoin mining involves significant risks and considerations that are beyond the scope of this educational implementation.

## Contact

For questions or contributions, please open an issue on GitHub or contact the maintainer.

---

**Happy Mining! ⛏️**
