#ifndef PARSER_H
#define PARSER_H

#include <vector>

namespace Mugen {

    struct Options {
	enum class Padding {
	    NONE,
	    VALUE,
	    CATCH
	};
	
	bool printLayout = false;
	bool lsbFirst = true;
	Padding padImages = Padding::NONE;
	unsigned char padValue = 0;
    };
    
    struct Result {
	std::vector<std::vector<unsigned char>> images;
	size_t target_rom_capacity;
	std::string layout;
    };
    
    Result parse(std::string const &filename, Options const &opt);
}

#endif
