#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <bitset>
#include <cassert>

void trim(std::string &str);
std::vector<std::string> split(std::string const &str, char const c, bool allowEmpty = false);
std::vector<std::string> split(std::string const &str, std::string const &token, bool allowEmpty = false);
std::string toBinaryString(size_t num, size_t minBits);

template <typename Int>
bool stringToInt(std::string const &str, Int &result, int base = 10) {
  size_t pos;
  try {
    result = std::stoi(str, &pos, base); }
  catch (...) {
    return false;
  }
  return (pos == str.size());
}

size_t bitsNeeded(size_t n);
unsigned char reverseBits(unsigned char byte);

#define UNREACHABLE assert(false && "UNREACHABLE");

#endif
