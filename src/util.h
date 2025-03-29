#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <bitset>

namespace Mugen {
    extern int lineNr;
}

void trim(std::string &str);
std::vector<std::string> split(std::string const &str, char const c);
std::string toBinaryString(size_t num, size_t minBits);

template <typename ... Args>
void error_if(bool condition, Args ... args) {
    if (!condition) return;
        
    (std::cerr << "ERROR:" << Mugen::lineNr << ": " <<  ... << args) << '\n';
    std::exit(1);
}

#endif
