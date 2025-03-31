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
	result.push_back(part);
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

bool stringToInt(std::string const &str, int &result, int base) {
    try {
	result = std::stoi(str, nullptr, base); }
    catch (...) {
	return false;
    }
    return true;

}
