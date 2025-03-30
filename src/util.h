#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <bitset>

void trim(std::string &str);
std::vector<std::string> split(std::string const &str, char const c, bool allowEmpty = false);
std::string toBinaryString(size_t num, size_t minBits);
bool stringToInt(std::string const &str, int &result, int base = 10);


#endif
