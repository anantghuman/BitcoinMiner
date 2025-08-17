CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LIBS = -lcurl -ljsoncpp

# Source files
SOURCES = bitcoin_miner.cpp blockchain_connection.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = bitcoin_miner

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compile object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install dependencies (macOS with Homebrew)
install-deps:
	@echo "Installing dependencies with Homebrew..."
	brew install curl jsoncpp

# Run the miner
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean install-deps run
