#include <iostream>
#include <fstream>
#include "mugen.h"

void printHelp(std::string const &progName) {
    std::cout << "Usage: " << progName << " <specification-file (.mu)> <output-file>\n\n"
	      << "Mugen is a microcode generator that converts a specification file\n"
              << "into microcode images suitable for flashing onto ROM chips.\n"
	      << "See https://github.com/jorenheit/mugen for more help.\n\n"
              << "Options:\n"
              << "  -h, --help    Display this help message and exit\n\n"
              << "Example:\n"
              << "  " << progName << " myspec.mu microcode.bin\n";
}

int main(int argc, char **argv) {

    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        printHelp(argv[0]);
        return 0;
    }

    if (argc != 3) {
        std::cerr << "ERROR: Invalid number of arguments.\n\n";
        printHelp(argv[0]);
        return 1;
    }

    std::string inFilename = argv[1];
    std::string outFilename = argv[2];
        
    auto images = Mugen::parse(inFilename);
        
    for (size_t part = 0; part != images.size(); ++part) {
	std::string filename = outFilename + ((images.size() > 0) ? ("." + std::to_string(part)) : "");
	std::ofstream out(filename, std::ios::binary);
	if (!out) {
	    std::cerr << "ERROR: Could not open output file \"" << filename << "\".";
	    return 1;
	}
	out.write(reinterpret_cast<char const *>(images[part].data()), images[part].size());
	out.close();
    }
} 
