#include <iostream>
#include <fstream>
#include "util.h"
#include "parser.h"

void printHelp(std::string const &progName) {
    std::cout << "Usage: " << progName << " <specification-file> <output-file>\n";
}

int main(int argc, char **argv) {
        
    if (argc != 3) {
	printHelp(argv[0]);
	return 1;
    }
        
    std::string inFilename = argv[1];
    std::string outFilename = argv[2];
        
    auto images = Mugen::parse(inFilename);
        
    for (size_t part = 0; part != images.size(); ++part) {
	std::string filename = outFilename + ((images.size() > 0) ? ("." + std::to_string(part)) : "");
	std::ofstream out(filename, std::ios::binary);
	error_if(!out, "could not open output file '", filename, "'.");
	out.write(reinterpret_cast<char const *>(images[part].data()), images[part].size());
	out.close();
    }
} 
