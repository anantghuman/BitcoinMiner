#include <iostream>
#include <bitset>
using namespace std;


int main() {
    string s;
    cin >> s;

    int n = s.size();
    string b = "";
    int bSize = n * 8;
    b.reserve(bSize);
    for (char ch : s) {
        b.append(bitset<8>(ch).to_string());
    }

    int padding = ((512 - 64) -  bSize % 512 + 512) % 512;


    
}