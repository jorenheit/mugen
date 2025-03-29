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

std::vector<std::string> split(std::string const &str, char const c) {
    std::vector<std::string> result;
    std::string component;
    for (size_t idx = 0; idx != str.length(); ++idx) {
	if (str[idx] != c) {
	    component += str[idx];
	}
	else {
	    trim(component);
	    result.push_back(component);
	    component.clear();
	}
    }
    trim(component);
    if (!component.empty()) result.push_back(component);
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
