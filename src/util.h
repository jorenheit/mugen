#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <bitset>

namespace Mugen {
    extern int _lineNr;
    extern std::string _file;
}

void trim(std::string &str);
std::vector<std::string> split(std::string const &str, char const c, bool allowEmpty = false);
std::string toBinaryString(size_t num, size_t minBits);
bool stringToInt(std::string const &str, int &result, int base = 10);

template <typename ... Args>
void error(Args ... args) {
    (std::cerr << "ERROR:" << Mugen::_file << ":" << Mugen::_lineNr << ": " <<  ... << args) << '\n';
    std::exit(1);
}

template <typename ... Args>
void error_if(bool condition, Args ... args) {
    if (condition) error(args...);
}

#endif
