# Bitcoin Genesis Block Miner (C++11)

This is a simple C++11 project that replicates the mining of the Bitcoin **genesis block** — the very first block ever mined by Satoshi Nakamoto on January 3, 2009.

> _"The Times 03/Jan/2009 Chancellor on brink of second bailout for banks"_

## Overview

This project demonstrates:
- Construction of a Bitcoin block header
- Double SHA-256 hashing (as used in Bitcoin)
- Genesis block parameters

It is a self-contained, offline simulation and does **not** connect to the Bitcoin network.

## Requirements
- C++11-compliant compiler (`g++`, `clang++`, MSVC)
- No external libraries required

## Building
Compile using any C++11 compiler:

```bash
g++ -std=c++11 bitcoin_miner.cpp -o bitcoin_miner
