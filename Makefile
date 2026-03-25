CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude -Ithird_party
LIBS     = -lcurl

SRCS   = src/main.cpp src/blockchain.cpp src/sha256.cpp
OBJS   = $(patsubst src/%.cpp, build/%.o, $(SRCS))
TARGET = bitcoin_miner

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $^ -o $@ $(LIBS)

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build $(TARGET)

run: all
	./$(TARGET)
