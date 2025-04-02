#include <iostream>

int main() {
    std::string str = "before->after->again";

    std::string token = "->";
    size_t prev = 0;
    size_t current = 0;
    while ((current = str.find(token, prev)) != std::string::npos) {
	std::string part = str.substr(prev, current - prev);
	prev = current + token.length();
	std::cout << part << '\n';
    }
    std::cout << str.substr(prev) << '\n';
}
