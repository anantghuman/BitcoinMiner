CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -march=native -Iinclude -Ithird_party
LIBS     = -lcurl -lpthread

NVCC      = nvcc
# Adjust -arch for your GPU: sm_61=Pascal, sm_75=Turing, sm_86=Ampere, sm_89=Ada
NVCCFLAGS = -std=c++17 -O3 -arch=sm_75 -Iinclude -Ithird_party \
            --compiler-options "-O3 -Wall"

GPU ?= 0

ifeq ($(GPU), 1)
CXXFLAGS += -DUSE_GPU
SRCS      = src/main.cpp src/blockchain.cpp
OBJS      = $(patsubst src/%.cpp, build/%.o, $(SRCS))
CUDA_OBJ  = build/miner_gpu.o
TARGET    = bitcoin_miner_gpu
LINK      = $(NVCC)
LINK_LIBS = $(LIBS) -lcudart
else
SRCS      = src/main.cpp src/blockchain.cpp src/miner_cpu.cpp
OBJS      = $(patsubst src/%.cpp, build/%.o, $(SRCS))
CUDA_OBJ  =
TARGET    = bitcoin_miner
LINK      = $(CXX)
LINK_LIBS = $(LIBS)
endif

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS) $(CUDA_OBJ)
	$(LINK) $^ -o $@ $(LINK_LIBS)

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/miner_gpu.o: src/miner_gpu.cu | build
	$(NVCC) $(NVCCFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build bitcoin_miner bitcoin_miner_gpu

run: all
	./$(TARGET)
