#include <iostream>
#include <fstream>
#include "mugen.h"

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
    for (int idx = 3; idx != argc; ++idx) {
        std::string flag = argv[idx];
        if (flag == "-l" || flag == "--layout") printReport = true;
	else if (flag == "-m" || flag == "--msb-first") lsbFirst = false;
	else if (flag == "-h" || flag == "--help") return printHelp(argv[0], 0);
        else {
            std::cerr << "ERROR: Unknown option \"" << flag << "\".\n\n";
            return printHelp(argv[0], 1);
        }
    }
    
    
    std::string inFilename = argv[1];
    std::string outFilename = argv[2];

    std::string report;
    auto images = Mugen::parse(inFilename, report, lsbFirst);

    std::vector<std::string> files;
    for (size_t idx = 0; idx != images.size(); ++idx) {
	std::string filename = outFilename + ((images.size() > 1) ? ("." + std::to_string(idx)) : "");
	std::ofstream out(filename, std::ios::binary);
	if (!out) {
	    std::cerr << "ERROR: Could not open output file \"" << filename << "\".";
	    return 1;
	}

	files.push_back(filename);
	out.write(reinterpret_cast<char const *>(images[idx].data()), images[idx].size());
	out.close();
    }

    std::cout << "Successfully generated " << images.size() << " images from " << inFilename <<": \n";
    for (size_t idx = 0; idx != images.size(); ++idx) {
	std::cout << "  " << "ROM " << idx << " : " << files[idx] << '\n';
    }

    if (printReport) {
	std::cout << '\n' << report << '\n';
    }
    
    return 0;
} 
