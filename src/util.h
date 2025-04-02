#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <bitset>

void trim(std::string &str);
std::vector<std::string> split(std::string const &str, char const c, bool allowEmpty = false);
std::vector<std::string> split(std::string const &str, std::string const &token, bool allowEmpty = false);
std::string toBinaryString(size_t num, size_t minBits);

template <typename Int>
bool stringToInt(std::string const &str, Int &result, int base = 10) {
    try {
	result = std::stoi(str, nullptr, base); }
    catch (...) {
	return false;
    }
    return true;

}

size_t bitsNeeded(size_t n);

#endif
