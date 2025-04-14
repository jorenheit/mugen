#include "util.h"

void trim(std::string &str) {
  size_t idx = 0;
  while (idx < str.length() && std::isspace(str[idx])) { ++idx; }
  if (idx == str.length()) {
    str.clear();
    return;
  }
        
  size_t const start = idx;
  idx = str.length() - 1;
  while (idx >= 0 && std::isspace(str[idx])) { --idx; }
  size_t const stop = idx;
        
  str = str.substr(start, stop - start + 1);
}

std::vector<std::string> split(std::string const &str, char const c, bool allowEmpty) {
  return split(str, std::string{c}, allowEmpty);
}

std::vector<std::string> split(std::string const &str, std::string const &token, bool allowEmpty) {
  std::vector<std::string> result;

  size_t prev = 0;
  size_t current = 0;
  while ((current = str.find(token, prev)) != std::string::npos) {
    std::string part = str.substr(prev, current - prev);
    trim(part);
    if (allowEmpty || !part.empty()) result.push_back(part);
    prev = current + token.length();
  }
  std::string last = str.substr(prev);
  trim(last);
  if (allowEmpty || !last.empty()) result.push_back(last);

  return result;
}

std::string toBinaryString(size_t num, size_t minBits) {
  std::string binary = std::bitset<64>(num).to_string();
  size_t firstOne = binary.find('1');
  std::string trimmed = (firstOne == std::string::npos) ? "0" : binary.substr(firstOne);
    
  if (trimmed.size() < minBits) {
    trimmed = std::string(minBits - trimmed.size(), '0') + trimmed;
  }
    
  return trimmed;
}


// Returns the number of bits needed to store values 0 .. n-1
size_t bitsNeeded(size_t n) {
  if (n == 0) return 0;
  size_t result = 0;
  size_t cmp = 1;
  while (cmp <= (n - 1)) {
    cmp <<= 1;
    ++result;
  }
  return result;
}

unsigned char reverseBits(unsigned char byte) {
  unsigned char result = (byte << 4) | (byte >> 4); 
  result = ((result & 0xCC) >> 2) | ((result & 0x33) << 2);
  result = ((result & 0xAA) >> 1) | ((result & 0x55) << 1);
  return result;
}

