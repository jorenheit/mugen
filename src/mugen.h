#ifndef PARSER_H
#define PARSER_H

#include <vector>

namespace Mugen {

    struct Result {
	std::vector<std::vector<unsigned char>> images;
	size_t target_rom_capacity;
	std::string report;
    };
    
    Result parse(std::string const &filename, bool lsbFirst = true);
}

#endif
