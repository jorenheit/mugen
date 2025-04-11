#include <iostream>
#include <fstream>
#include "mugen.h"
#include "util.h"

int printHelp(std::string const &progName, int ret) {
    std::cout << "Usage: " << progName << " <specification-file (.mu)> <output-file> [OPTIONS]\n\n"
              << "Mugen is a microcode generator that converts a specification file\n"
              << "into microcode images suitable for flashing onto ROM chips.\n"
              << "Optionally, the layout report can be printed using the --layout or -l flag.\n"
              << "See https://github.com/jorenheit/mugen for more help.\n\n"
              << "Options:\n"
              << "  -h, --help       Display this help message and exit\n"
              << "  -l, --layout     Print the ROM layout report after generation\n"
	      << "  -m, --msb-first  Store signals starting from the most significant bit.\n"
              << "\nExample:\n"
              << "  " << progName << " myspec.mu microcode.bin --msb-first --layout\n";

    return ret;
}

int main(int argc, char **argv) {

    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        printHelp(argv[0], 0);
        return 0;
    }
    if (argc < 3) {
        std::cerr << "ERROR: Invalid number of arguments.\n\n";
        printHelp(argv[0], 1);
        return 1;
    }

    bool printReport = false;
    bool lsbFirst = true;
    bool padImages = false;
    unsigned char padValue = 0;

    for (int idx = 3; idx != argc; ++idx) {
        std::string flag = argv[idx];
        if (flag == "-l" || flag == "--layout") printReport = true;
	else if (flag == "-m" || flag == "--msb-first") lsbFirst = false;
	else if (flag == "-p" || flag == "--pad") {
	    if (idx == argc - 1) {
		std::cerr << "ERROR: no argument to --pad (-p) option.\n\n";
		return printHelp(argv[0], 1);
	    }
	    int value = 0;
	    idx += 1;
	    if (!stringToInt(argv[idx], value, 16)) {
		std::cerr << "ERROR: argument passed to --pad (-p) must be a hex value.\n\n";
		return printHelp(argv[0], 1);
	    }
	    if (value > 0xff) {
		std::cerr << "ERROR: value passed to --pad (-p) exceeds 8 bits.\n\n";
		return printHelp(argv[0], 1);
	    }

	    padImages = true;
	    padValue = value;
	}
	else if (flag == "-h" || flag == "--help") return printHelp(argv[0], 0);
        else {
            std::cerr << "ERROR: Unknown option \"" << flag << "\".\n\n";
            return printHelp(argv[0], 1);
        }
    }
    
    std::string inFilename = argv[1];
    std::string outFilename = argv[2];

    auto result = Mugen::parse(inFilename, lsbFirst);
    size_t padSize = padImages ? (result.target_rom_capacity - result.images[0].size()) : 0;
    std::vector<unsigned char> padVector(padSize, padValue);
    
    std::vector<std::string> files;
    for (size_t idx = 0; idx != result.images.size(); ++idx) {
	std::string filename = outFilename + ((result.images.size() > 1) ? ("." + std::to_string(idx)) : "");
	std::ofstream out(filename, std::ios::binary);
	if (!out) {
	    std::cerr << "ERROR: Could not open output file \"" << filename << "\".";
	    return 1;
	}

	files.push_back(filename);
	out.write(reinterpret_cast<char const *>(result.images[idx].data()), result.images[idx].size());
	out.write(reinterpret_cast<char const *>(padVector.data()), padVector.size());
	out.close();
    }

    std::cout << "Successfully generated " << result.images.size() << " images from " << inFilename <<": \n";
    for (size_t idx = 0; idx != result.images.size(); ++idx) {
	std::cout << "  " << "ROM " << idx << " : " << files[idx]
		  << " (" << (result.images[idx].size() + padSize) << " bytes)\n";
    }

    if (printReport) {
	std::cout << '\n' << result.report << '\n';
    }
    
    return 0;
} 
